//
// Created by davidjaramillo on 8/29/26.
//

#include "server.hpp"
#include "terminal.hpp"

#include <iostream>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <errno.h>

using namespace std;

Server::Server(int port) : port(port), listen_fd(-1) {
}

Server::~Server() {
    if (listen_fd != -1) {
        close(listen_fd);
    }
}

void Server::run() {
    // A client that disappears mid-write would otherwise kill us with SIGPIPE.
    signal(SIGPIPE, SIG_IGN);

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

    if (listen(listen_fd, 1) == -1) {
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
        cout << "client connected from " << client_ip << endl;

        Terminal terminal;
        terminal.start(client_fd);

        close(client_fd);
        cout << "client disconnected" << endl;
    }
}
