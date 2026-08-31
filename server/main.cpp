#include "server.hpp"

#include <iostream>
#include <cstdlib>
#include <string>

int main(int argc, char *argv[]) {
    int port = 4242;

    if (argc > 1) {
        port = std::atoi(argv[1]);

        if (port < 1 || port > 65535) {
            std::cerr << "usage: dterm [port]\n";
            return EXIT_FAILURE;
        }
    }

    Server server(port);
    server.run();

    return EXIT_SUCCESS;
}
