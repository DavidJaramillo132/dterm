#pragma once

#include <string>

class Terminal {
public:
    // Runs a bash session, reading input from and writing output to client_fd.
    // The client must pass the HELLO + challenge-response handshake first.
    void start(int client_fd, const std::string &secret);
};
