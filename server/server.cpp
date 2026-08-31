//
// Created by davidjaramillo on 8/29/26.
//

#include "server.hpp"
#include "handshake.hpp"
#include "session.hpp"
#include "auth.hpp"

#include <iostream>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>
#include <sys/wait.h>
#include <cstdlib>
#include <poll.h>
#include <vector>
#include <string>

using namespace std;

namespace {

    constexpr int MAX_CLIENTS = 16;

    bool write_all(int fd, const char *data, size_t length) {
        size_t sent = 0;

        while (sent < length) {
            const ssize_t wrote = write(fd, data + sent, length - sent);

            if (wrote == -1) {
                if (errno == EINTR) {
                    continue;
                }

                return false;
            }

            sent += static_cast<size_t>(wrote);
        }

        return true;
    }

    // Copies bytes both ways until either side goes away. This process does
    // not parse frames: the session on the other end does that.
    void pump(int client_fd, int session_fd) {
        pollfd fds[2];
        fds[0] = {client_fd, POLLIN, 0};
        fds[1] = {session_fd, POLLIN, 0};

        while (true) {
            if (poll(fds, 2, -1) == -1) {
                if (errno == EINTR) {
                    continue;
                }

                return;
            }

            for (int i = 0; i < 2; ++i) {
                if (fds[i].revents & POLLIN) {
                    char buffer[8192];
                    const ssize_t got = read(fds[i].fd, buffer, sizeof(buffer));

                    if (got <= 0) {
                        return;
                    }

                    if (!write_all(fds[1 - i].fd, buffer, static_cast<size_t>(got))) {
                        return;
                    }
                }
            }

            if ((fds[0].revents | fds[1].revents) & (POLLHUP | POLLERR)) {
                return;
            }
        }
    }

    // Touched by both the accept loop and the SIGCHLD handler, so it must be
    // sig_atomic_t and every read-modify-write has to run with SIGCHLD blocked.
    volatile sig_atomic_t active_clients = 0;

    // Without this, every finished session would linger as a zombie forever.
    void reap_children(int) {
        // waitpid clobbers errno, and we may have interrupted an accept()
        // that is about to read it.
        const int saved_errno = errno;

        while (waitpid(-1, nullptr, WNOHANG) > 0) {
            active_clients = active_clients - 1;
        }

        errno = saved_errno;
    }
}

Server::Server(int port) : port(port), listen_fd(-1) {
}

Server::~Server() {
    if (listen_fd != -1) {
        close(listen_fd);
    }
}

void Server::run() {
    // Fail closed: no secret, no server. Better than silently serving a shell.
    string secret;
    string error;

    if (!protocol::load_secret(secret, error)) {
        cerr << error << endl;
        return;
    }

    // A client that disappears mid-write would otherwise kill us with SIGPIPE.
    signal(SIGPIPE, SIG_IGN);

    struct sigaction on_child = {};
    on_child.sa_handler = reap_children;
    sigemptyset(&on_child.sa_mask);
    on_child.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &on_child, nullptr);

    sigset_t child_signal;
    sigemptyset(&child_signal);
    sigaddset(&child_signal, SIGCHLD);

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd == -1) {
        cerr << "failed to create socket\n";
        return;
    }

    // Allow an immediate restart while the old port is still in TIME_WAIT.
    int reuse = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

    sockaddr_in address = {};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (bind(listen_fd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) == -1) {
        cerr << "failed to bind port " << port << "\n";
        return;
    }

    if (listen(listen_fd, MAX_CLIENTS) == -1) {
        cerr << "failed to listen\n";
        return;
    }

    cout << "DTerm server listening on port " << port << endl;

    while (true) {
        sockaddr_in client_address = {};
        socklen_t client_length = sizeof(client_address);

        int client_fd = accept(
            listen_fd,
            reinterpret_cast<sockaddr *>(&client_address),
            &client_length
        );

        if (client_fd == -1) {
            if (errno == EINTR) {
                continue;
            }

            cerr << "failed to accept\n";
            break;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_address.sin_addr, client_ip, sizeof(client_ip));

        if (active_clients >= MAX_CLIENTS) {
            cerr << "too many clients, refusing " << client_ip << endl;
            close(client_fd);
            continue;
        }

        // Hold SIGCHLD off until the counter and the fork agree, otherwise a
        // child that dies instantly could decrement before we increment.
        sigset_t previous;
        sigprocmask(SIG_BLOCK, &child_signal, &previous);

        active_clients = active_clients + 1;

        const pid_t pid = fork();

        if (pid == -1) {
            active_clients = active_clients - 1;
            sigprocmask(SIG_SETMASK, &previous, nullptr);

            cerr << "failed to fork for " << client_ip << endl;
            close(client_fd);
            continue;
        }

        if (pid == 0) {
            // CHILD: owns exactly one client and nothing else.
            sigprocmask(SIG_SETMASK, &previous, nullptr);

            // It must not keep the listening socket alive, and Terminal waits
            // for its own bash, so the reaper would steal that wait().
            close(listen_fd);
            signal(SIGCHLD, SIG_DFL);

            cout << "client connected from " << client_ip << endl;

            protocol::FrameReader reader;
            string session_name;

            if (handshake(client_fd, reader, secret, session_name)) {
                // The session must not inherit this socket, or the connection
                // would never fully close when this process goes away.
                const int session_fd = session::open(session_name, client_fd);

                if (session_fd == -1) {
                    cerr << "could not reach session '" << session_name << "'" << endl;
                } else {
                    // Anything the client sent behind the ATTACH frame belongs
                    // to the session, not to us.
                    const vector<uint8_t> pending = reader.take_buffer();

                    if (pending.empty() ||
                        write_all(session_fd,
                                  reinterpret_cast<const char *>(pending.data()),
                                  pending.size())) {
                        pump(client_fd, session_fd);
                    }

                    close(session_fd);
                }
            }

            close(client_fd);
            cout << "client disconnected" << endl;

            // _exit, not exit: the parent's atexit handlers are not ours to run.
            _exit(EXIT_SUCCESS);
        }

        // PARENT: the child owns the connection now.
        close(client_fd);
        sigprocmask(SIG_SETMASK, &previous, nullptr);
    }
}
