#include "raw_mode.h"

#include <iostream>
#include <cstdlib>
#include <cstring>

#include <poll.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <netdb.h>
#include <sys/socket.h>

using namespace std;

namespace {
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

    cout << "connected to " << host << ":" << port << "\n";

    {
        RawMode raw_mode;

        pollfd fds[2];

        fds[0].fd = STDIN_FILENO;
        fds[0].events = POLLIN;

        fds[1].fd = server_fd;
        fds[1].events = POLLIN;

        bool running = true;

        while (running) {
            if (poll(fds, 2, -1) == -1) {
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

                write(server_fd, buffer, bytes_read);
            }

            // server -> screen
            if (fds[1].revents & POLLIN) {
                char buffer[4096];

                ssize_t bytes_read = read(server_fd, buffer, sizeof(buffer));
                if (bytes_read <= 0) {
                    running = false;
                    break;
                }

                write(STDOUT_FILENO, buffer, bytes_read);
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
