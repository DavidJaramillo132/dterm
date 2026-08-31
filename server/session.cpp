#include "session.hpp"
#include "protocol.hpp"

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <vector>

#include <errno.h>
#include <poll.h>
#include <pty.h>
#include <pwd.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

using namespace std;
using namespace protocol;

namespace {

    constexpr int DEFAULT_PING_SECONDS = 30;
    constexpr int MAX_MISSED_PONGS = 2;

    // How long a silent client is given before it gets pinged. A phone on
    // mobile data may want this shorter than a machine on a cable.
    int ping_interval_ms() {
        const char *configured = getenv("DTERM_PING_SECONDS");
        int seconds = DEFAULT_PING_SECONDS;

        if (configured != nullptr) {
            const int parsed = atoi(configured);

            if (parsed >= 1 && parsed <= 3600) {
                seconds = parsed;
            }
        }

        return seconds * 1000;
    }
    constexpr size_t MAX_NAME = 32;

    // What a reattaching client gets replayed so the screen is not blank.
    constexpr size_t SCROLLBACK_MAX = 64 * 1024;

    string home_dir() {
        const char *home = getenv("HOME");

        if (home == nullptr) {
            const passwd *entry = getpwuid(getuid());
            home = entry != nullptr ? entry->pw_dir : "/tmp";
        }

        return string(home);
    }

    string socket_path(const string &name) {
        return home_dir() + "/.dterm/sessions/" + name + ".sock";
    }

    void ensure_directory() {
        mkdir((home_dir() + "/.dterm").c_str(), 0700);
        mkdir((home_dir() + "/.dterm/sessions").c_str(), 0700);
    }

    bool fill_address(sockaddr_un &address, const string &path) {
        if (path.size() >= sizeof(address.sun_path)) {
            return false;
        }

        address.sun_family = AF_UNIX;
        strncpy(address.sun_path, path.c_str(), sizeof(address.sun_path) - 1);
        return true;
    }

    int connect_existing(const string &name) {
        sockaddr_un address = {};

        if (!fill_address(address, socket_path(name))) {
            return -1;
        }

        const int fd = socket(AF_UNIX, SOCK_STREAM, 0);

        if (fd == -1) {
            return -1;
        }

        if (connect(fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == -1) {
            const int saved_errno = errno;
            close(fd);
            errno = saved_errno;
            return -1;
        }

        return fd;
    }

    // Sends the recent PTY output so a reattaching client sees its screen back.
    void replay(int client_fd, const vector<uint8_t> &scrollback) {
        if (scrollback.empty()) {
            return;
        }

        send_frame(client_fd, Type::Output, scrollback.data(),
                   static_cast<uint32_t>(scrollback.size()));
    }

    void remember(vector<uint8_t> &scrollback, const char *data, size_t length) {
        scrollback.insert(scrollback.end(), data, data + length);

        if (scrollback.size() > SCROLLBACK_MAX) {
            scrollback.erase(scrollback.begin(),
                             scrollback.begin() + (scrollback.size() - SCROLLBACK_MAX));
        }
    }

    void apply_resize(int master_fd, const Frame &frame) {
        uint16_t rows = 0;
        uint16_t cols = 0;

        if (!decode_resize(frame.payload, rows, cols)) {
            return;
        }

        winsize size = {};
        size.ws_row = rows;
        size.ws_col = cols;

        ioctl(master_fd, TIOCSWINSZ, &size);
    }

    // The session process. Owns the PTY for as long as bash lives, with or
    // without anybody attached. Never returns.
    [[noreturn]] void run(const string &name) {
        const string path = socket_path(name);

        sockaddr_un address = {};
        const int listen_fd = socket(AF_UNIX, SOCK_STREAM, 0);

        if (listen_fd == -1 || !fill_address(address, path)) {
            _exit(EXIT_FAILURE);
        }

        // Losing this race just means another process got the session first.
        if (::bind(listen_fd, reinterpret_cast<sockaddr *>(&address),
                   sizeof(address)) == -1) {
            _exit(EXIT_FAILURE);
        }

        chmod(path.c_str(), 0600);
        listen(listen_fd, 4);

        int master_fd = -1;

        winsize size = {};
        size.ws_row = 24;
        size.ws_col = 80;

        const pid_t shell = forkpty(&master_fd, nullptr, nullptr, &size);

        if (shell == -1) {
            unlink(path.c_str());
            _exit(EXIT_FAILURE);
        }

        if (shell == 0) {
            execlp("bash", "bash", "--login", nullptr);
            _exit(EXIT_FAILURE);
        }

        cout << "session '" << name << "' started" << endl;

        const int ping_timeout = ping_interval_ms();

        vector<uint8_t> scrollback;
        FrameReader reader;

        int client_fd = -1;
        int missed_pongs = 0;
        bool running = true;

        while (running) {
            // poll skips negative descriptors, so a detached session simply
            // watches two things instead of three.
            pollfd fds[3];
            fds[0] = {listen_fd, POLLIN, 0};
            fds[1] = {master_fd, POLLIN, 0};
            fds[2] = {client_fd, POLLIN, 0};

            const int timeout = client_fd == -1 ? -1 : ping_timeout;
            const int ready = poll(fds, 3, timeout);

            if (ready == -1) {
                if (errno == EINTR) {
                    continue;
                }

                break;
            }

            if (ready == 0) {
                if (missed_pongs >= MAX_MISSED_PONGS) {
                    cerr << "session '" << name << "': client went quiet, detaching" << endl;
                    close(client_fd);
                    client_fd = -1;
                    reader.take_buffer();
                    continue;
                }

                if (send_frame(client_fd, Type::Ping) != Status::Ok) {
                    close(client_fd);
                    client_fd = -1;
                    reader.take_buffer();
                    continue;
                }

                ++missed_pongs;
                continue;
            }

            // Somebody wants to attach.
            if (fds[0].revents & POLLIN) {
                const int incoming = accept(listen_fd, nullptr, nullptr);

                if (incoming != -1) {
                    if (client_fd != -1) {
                        // Only one client at a time: the newcomer wins.
                        close(client_fd);
                    }

                    client_fd = incoming;
                    missed_pongs = 0;

                    // A new connection is a new byte stream: drop any tail
                    // left over from the previous one.
                    reader.take_buffer();

                    replay(client_fd, scrollback);
                    cout << "session '" << name << "' attached" << endl;
                }
            }

            // The shell said something.
            if (fds[1].revents & POLLIN) {
                char buffer[4096];
                const ssize_t got = read(master_fd, buffer, sizeof(buffer));

                if (got <= 0) {
                    running = false;
                    break;
                }

                // Read even when nobody is attached, or the PTY buffer fills
                // up and bash blocks forever.
                remember(scrollback, buffer, static_cast<size_t>(got));

                if (client_fd != -1 &&
                    send_frame(client_fd, Type::Output, buffer,
                               static_cast<uint32_t>(got)) != Status::Ok) {
                    close(client_fd);
                    client_fd = -1;
                    reader.take_buffer();
                }
            }

            // The attached client said something.
            if (client_fd != -1 && (fds[2].revents & POLLIN)) {
                if (reader.feed(client_fd) != Status::Ok) {
                    cout << "session '" << name << "' detached" << endl;
                    close(client_fd);
                    client_fd = -1;
                    reader.take_buffer();
                    continue;
                }

                missed_pongs = 0;

                Frame frame;
                Parse parsed;

                while ((parsed = reader.next(frame)) == Parse::Ready) {
                    switch (frame.type) {
                        case Type::Input:
                            if (!frame.payload.empty()) {
                                write(master_fd, frame.payload.data(),
                                      frame.payload.size());
                            }
                            break;

                        case Type::Resize:
                            apply_resize(master_fd, frame);
                            break;

                        case Type::Ping:
                            send_frame(client_fd, Type::Pong);
                            break;

                        default:
                            break;
                    }
                }

                if (parsed == Parse::Malformed) {
                    close(client_fd);
                    client_fd = -1;
                    reader.take_buffer();
                }
            }

            // A client hanging up detaches it. It never touches the shell.
            if (client_fd != -1 && (fds[2].revents & (POLLHUP | POLLERR))) {
                cout << "session '" << name << "' detached" << endl;
                close(client_fd);
                client_fd = -1;
                reader.take_buffer();
            }

            if (fds[1].revents & (POLLHUP | POLLERR)) {
                running = false;
            }
        }

        cout << "session '" << name << "' ended" << endl;

        if (client_fd != -1) {
            close(client_fd);
        }

        close(master_fd);
        close(listen_fd);
        unlink(path.c_str());

        kill(shell, SIGHUP);
        waitpid(shell, nullptr, 0);

        _exit(EXIT_SUCCESS);
    }

    // Starts a session process that outlives whoever asked for it.
    pid_t spawn(const string &name, int close_in_child) {
        const pid_t pid = fork();

        if (pid != 0) {
            return pid;   // -1 on failure, the child's pid otherwise
        }

        // CHILD. setsid detaches it from the caller's process group and
        // controlling terminal, so it survives the connection that started it.
        setsid();

        if (close_in_child != -1) {
            close(close_in_child);
        }

        signal(SIGCHLD, SIG_DFL);
        run(name);
    }
}

bool session::valid_name(const string &name) {
    if (name.empty() || name.size() > MAX_NAME) {
        return false;
    }

    for (const char c : name) {
        const bool allowed = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                             (c >= '0' && c <= '9') || c == '-' || c == '_';

        if (!allowed) {
            return false;
        }
    }

    return true;
}

int session::open(const string &name, int close_in_child) {
    ensure_directory();

    int fd = connect_existing(name);

    if (fd != -1) {
        return fd;
    }

    // A socket file whose session was killed refuses connections. Clear it.
    if (errno == ECONNREFUSED) {
        unlink(socket_path(name).c_str());
    }

    if (spawn(name, close_in_child) == -1) {
        return -1;
    }

    // The new session needs a moment to bind. Poll for up to a second.
    for (int attempt = 0; attempt < 50; ++attempt) {
        usleep(20000);

        fd = connect_existing(name);

        if (fd != -1) {
            return fd;
        }
    }

    return -1;
}
