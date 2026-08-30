#include "terminal.hpp"
#include "protocol.hpp"
#include "auth.hpp"

#include <iostream>
#include <cstdlib>

#include <poll.h>
#include <pty.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <errno.h>
#include <signal.h>

using namespace std;
using namespace protocol;

namespace {

    constexpr int HANDSHAKE_TIMEOUT_MS = 5000;

    // A silent client gets pinged; two unanswered pings and it is gone.
    constexpr int PING_INTERVAL_MS = 30000;
    constexpr int MAX_MISSED_PONGS = 2;

    // Blocks until one whole frame arrives, or the client stays silent too long.
    bool next_frame(int client_fd, FrameReader &reader, Frame &frame) {
        pollfd fd = {client_fd, POLLIN, 0};

        while (true) {
            const Parse parsed = reader.next(frame);

            if (parsed == Parse::Ready) {
                return true;
            }

            if (parsed == Parse::Malformed) {
                cerr << "malformed frame during handshake" << endl;
                return false;
            }

            const int ready = poll(&fd, 1, HANDSHAKE_TIMEOUT_MS);

            if (ready == -1) {
                if (errno == EINTR) {
                    continue;
                }

                return false;
            }

            if (ready == 0) {
                cerr << "handshake timed out" << endl;
                return false;
            }

            if (reader.feed(client_fd) != Status::Ok) {
                return false;
            }
        }
    }

    // Checks the client speaks our protocol before anything else happens.
    bool check_hello(const Frame &frame) {
        if (frame.type != Type::Hello) {
            cerr << "expected HELLO, got frame type "
                 << static_cast<int>(frame.type) << endl;
            return false;
        }

        if (frame.payload.size() != 5 ||
            frame.payload[0] != 'D' || frame.payload[1] != 'T' ||
            frame.payload[2] != 'R' || frame.payload[3] != 'M') {
            cerr << "bad HELLO signature" << endl;
            return false;
        }

        if (frame.payload[4] != VERSION) {
            cerr << "client speaks protocol v" << static_cast<int>(frame.payload[4])
                 << ", server speaks v" << static_cast<int>(VERSION) << endl;
            return false;
        }

        return true;
    }

    // Proves the client holds the shared secret without either side sending it.
    bool authenticate(int client_fd, FrameReader &reader, const string &secret) {
        const vector<uint8_t> challenge = random_bytes(CHALLENGE_SIZE);

        if (challenge.size() != CHALLENGE_SIZE) {
            cerr << "failed to generate a challenge" << endl;
            return false;
        }

        if (send_frame(client_fd, Type::Challenge, challenge.data(),
                       static_cast<uint32_t>(challenge.size())) != Status::Ok) {
            return false;
        }

        Frame frame;

        if (!next_frame(client_fd, reader, frame)) {
            return false;
        }

        if (frame.type != Type::Auth) {
            cerr << "expected AUTH, got frame type "
                 << static_cast<int>(frame.type) << endl;
            return false;
        }

        const vector<uint8_t> expected = hmac_sha256(secret, challenge);

        if (expected.empty() || !constant_time_equal(expected, frame.payload)) {
            cerr << "authentication failed: wrong secret" << endl;
            send_frame(client_fd, Type::AuthFail);
            return false;
        }

        cout << "client authenticated" << endl;
        return send_frame(client_fd, Type::AuthOk) == Status::Ok;
    }

    bool handshake(int client_fd, FrameReader &reader, const string &secret) {
        Frame frame;

        if (!next_frame(client_fd, reader, frame)) {
            return false;
        }

        if (!check_hello(frame)) {
            return false;
        }

        return authenticate(client_fd, reader, secret);
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

        // The kernel raises SIGWINCH in the slave for us.
        ioctl(master_fd, TIOCSWINSZ, &size);
    }
}

void Terminal::start(int client_fd, const string &secret) {
    FrameReader reader;

    if (!handshake(client_fd, reader, secret)) {
        return;
    }

    int master_fd;

    // Provisional until the client's first RESIZE arrives.
    winsize size = {};
    size.ws_row = 24;
    size.ws_col = 80;

    pid_t pid = forkpty(&master_fd, nullptr, nullptr, &size);

    if (pid == -1) {
        cerr << "failed to create PTY\n";
        return;
    }

    if (pid == 0) {
        execlp("bash", "bash", "--login", nullptr);

        cerr << "failed to execute bash\n";
        exit(EXIT_FAILURE);
    }

    // PARENT PROCESS
    pollfd fds[2];

    fds[0].fd = client_fd;
    fds[0].events = POLLIN;

    fds[1].fd = master_fd;
    fds[1].events = POLLIN;

    bool running = true;
    int missed_pongs = 0;

    while (running) {
        const int ready = poll(fds, 2, PING_INTERVAL_MS);

        if (ready == -1) {
            // A signal interrupted us; revents was never written, so start over.
            if (errno == EINTR) {
                continue;
            }

            cerr << "failed to poll" << endl;
            break;
        }

        if (ready == 0) {
            if (missed_pongs >= MAX_MISSED_PONGS) {
                cerr << "client stopped answering pings, dropping it" << endl;
                break;
            }

            if (send_frame(client_fd, Type::Ping) != Status::Ok) {
                break;
            }

            ++missed_pongs;
            continue;   // revents is stale after a timeout: do not read it
        }

        // client -> PTY
        if (fds[0].revents & POLLIN) {
            if (reader.feed(client_fd) != Status::Ok) {
                break;
            }

            // Any frame at all proves the client is still there.
            missed_pongs = 0;

            Frame frame;
            Parse parsed;

            // One read can carry several frames: drain them all.
            while ((parsed = reader.next(frame)) == Parse::Ready) {
                switch (frame.type) {
                    case Type::Input:
                        if (!frame.payload.empty()) {
                            write(master_fd, frame.payload.data(), frame.payload.size());
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
                cerr << "malformed frame, dropping client\n";
                break;
            }
        }

        // PTY -> client
        if (fds[1].revents & POLLIN) {
            char buffer[4096];

            ssize_t bytes_read = read(master_fd, buffer, sizeof(buffer));

            if (bytes_read <= 0) {
                running = false;
                break;
            }

            if (send_frame(client_fd, Type::Output, buffer,
                           static_cast<uint32_t>(bytes_read)) != Status::Ok) {
                running = false;
                break;
            }
        }

        // the client hung up, or bash exited
        if ((fds[0].revents | fds[1].revents) & (POLLHUP | POLLERR)) {
            running = false;
        }
    }

    close(master_fd);

    kill(pid, SIGHUP);
    waitpid(pid, nullptr, 0);
}
