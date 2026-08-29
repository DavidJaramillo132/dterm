//
// Created by davidjaramillo on 8/24/26.
//

#pragma once

class Terminal {
public:
    // Runs a bash session, reading input from and writing output to client_fd.
    void start(int client_fd);
};
