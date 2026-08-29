//
// Created by davidjaramillo on 8/29/26.
//

#pragma once

class Server {
public:
    explicit Server(int port);
    ~Server();

    // Binds, listens, and serves one client at a time until killed.
    void run();

private:
    int port;
    int listen_fd;
};
