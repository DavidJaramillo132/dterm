#include "raw_mode.h"
#include "protocol.hpp"

#include <iostream>
#include <cstdlib>

#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/ioctl.h>

using namespace std;
using namespace protocol;

namespace {

    volatile sig_atomic_t window_resized = 0;

    void handle_sigwinch(int) {
        window_resized = 1;
    }

    int connect_to(const char *host, const char *port) {
        addrinfo hints = {};
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_STREAM;

        addrinfo *candidates = nullptr;
        int error = getaddrinfo(host, port, &hints, &candidates);
        if (error != 0) {
            cerr << "cannot resolve " << host << ": " << gai_strerror(error) << "\n";
            return -1;
        }

        int fd = -1;
        for (addrinfo *it = candidates; it != nullptr; it = it->ai_next) {
            fd = socket(it->ai_family, it->ai_socktype, it->ai_protocol);
            if (fd == -1) {
                continue;
            }

            if (connect(fd, it->ai_addr, it->ai_addrlen) == 0) {
                break;
            }

            close(fd);
            fd = -1;
        }

        freeaddrinfo(candidates);
        return fd;
    }

    Status send_hello(int fd) {
        const uint8_t hello[] = {'D', 'T', 'R', 'M', VERSION};
        return send_frame(fd, Type::Hello, hello, sizeof(hello));
    }

    Status send_current_size(int fd) {
        winsize size = {};

        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == -1) {
            return Status::Ok;
        }

        const vector<uint8_t> payload = encode_resize(size.ws_row, size.ws_col);
        return send_frame(fd, Type::Resize, payload.data(),
                          static_cast<uint32_t>(payload.size()));
    }
}

int main(int argc, char *argv[]) {
    const char *host = argc > 1 ? argv[1] : "127.0.0.1";
    const char *port = argc > 2 ? argv[2] : "4242";

    signal(SIGPIPE, SIG_IGN);

    int server_fd = connect_to(host, port);
    if (server_fd == -1) {
        cerr << "failed to connect to " << host << ":" << port << "\n";
        return EXIT_FAILURE;
    }

    if (send_hello(server_fd) != Status::Ok) {
        cerr << "failed to send handshake\n";
        close(server_fd);
        return EXIT_FAILURE;
    }

    cout << "connected to " << host << ":" << port << "\n";

    struct sigaction sa = {};
    sa.sa_handler = handle_sigwinch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sa, nullptr);

    {
        RawMode raw_mode;

        send_current_size(server_fd);

        FrameReader reader;

        pollfd fds[2];

        fds[0].fd = STDIN_FILENO;
        fds[0].events = POLLIN;

        fds[1].fd = server_fd;
        fds[1].events = POLLIN;

        bool running = true;

        while (running) {
            const int ready = poll(fds, 2, -1);

            if (window_resized) {
                window_resized = 0;
                send_current_size(server_fd);
            }

            if (ready == -1) {
                if (errno == EINTR) {
                    continue;
                }

                break;
            }

            // keyboard -> server
            if (fds[0].revents & POLLIN) {
                char buffer[4096];

                ssize_t bytes_read = read(STDIN_FILENO, buffer, sizeof(buffer));
                if (bytes_read <= 0) {
                    break;
                }

                if (send_frame(server_fd, Type::Input, buffer,
                               static_cast<uint32_t>(bytes_read)) != Status::Ok) {
                    break;
                }
            }

            // server -> screen
            if (fds[1].revents & POLLIN) {
                if (reader.feed(server_fd) != Status::Ok) {
                    running = false;
                    break;
                }

                Frame frame;
                Parse parsed;

                while ((parsed = reader.next(frame)) == Parse::Ready) {
                    switch (frame.type) {
                        case Type::Output:
                            if (!frame.payload.empty()) {
                                write(STDOUT_FILENO, frame.payload.data(),
                                      frame.payload.size());
                            }
                            break;

                        case Type::Ping:
                            send_frame(server_fd, Type::Pong);
                            break;

                        default:
                            break;
                    }
                }

                if (parsed == Parse::Malformed) {
                    running = false;
                    break;
                }
            }

            if ((fds[0].revents | fds[1].revents) & (POLLHUP | POLLERR)) {
                running = false;
            }
        }
    }

    close(server_fd);
    cout << "\nconnection closed\n";

    return EXIT_SUCCESS;
}
