#include "terminal.hpp"

#include <iostream>
#include <cstdlib>

#include <poll.h>
#include <pty.h>
#include <unistd.h>
#include <sys/wait.h>
#include <termios.h>

using namespace std;

void Terminal::start() {
    int master_fd;

    pid_t pid = forkpty(
        &master_fd,
        nullptr,
        nullptr,
        nullptr
        );

    if (pid == -1) {
        cerr << "failed to create PTY\n";
        return;
    }

    if  (pid == 0) {
        execlp(
            "bash",
            "bash",
            "--login",
            nullptr
        );

        cerr << "failed to execute bash\n";
        exit(EXIT_FAILURE);
    }

    // PARENT PROCESS
    cout << "Dterm v0.1\n";
    cout << "-------------------------\n\n";

    pollfd fds[2];

    fds[0].fd = STDIN_FILENO;
    fds[0].events = POLLIN;

    fds[1].fd = master_fd;
    fds[1].events = POLLIN;

    bool running = true;

    while (running) {
         int result = poll(
             fds,
             2,
             -1
         );

        if (result == -1) {
            cerr << "failed to poll\n";
            break;
        }
        // Keyboard PTY
        if (fds [0].revents & POLLIN) {
            char buffer[4096];

            ssize_t bytes_read;
            bytes_read = read(
                STDIN_FILENO,
                buffer,
                sizeof(buffer)
            );

            if (bytes_read <= 0) {
                break;
            }
            write(
                master_fd,
                buffer,
                bytes_read
            );

        }

        // PTY -> screen
        if (fds[1].revents & POLLIN) {
            char buffer[4096];

            ssize_t bytes_read = read(
                master_fd,
                buffer,
                sizeof(buffer)
            );

            if (bytes_read <= 0) {
                running = false;
                break;
            }

            write(
                STDOUT_FILENO,
                buffer,
                bytes_read
            );
        }
    }

    close(master_fd);
    waitpid(
        pid,
        nullptr,
        0
    );
}
