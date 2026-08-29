#include "terminal.hpp"

#include "raw_mode.h"

#include <iostream>
#include <cstdlib>

#include <poll.h>
#include <pty.h>
#include <unistd.h>
#include <sys/wait.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <errno.h>

using namespace std;

namespace {
    // Set by the SIGWINCH handler, read by the poll loop.
    volatile sig_atomic_t window_resized = 0;

    void handle_sigwinch(int) {
        window_resized = 1;
    }
}

void Terminal::start() {
    int master_fd;

    struct winsize size;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &size);

    pid_t pid = forkpty(
        &master_fd,
        nullptr,
        nullptr,
        &size
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

    struct sigaction sa = {};
    sa.sa_handler = handle_sigwinch;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGWINCH, &sa, nullptr);

    RawMode raw_mode;

    while (running) {
         int result = poll(
             fds,
             2,
             -1
         );

        if (window_resized) {
            window_resized = 0;

            struct winsize current;
            if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &current) != -1) {
                ioctl(master_fd, TIOCSWINSZ, &current);
            }
        }

        if (result == -1) {
            if (errno == EINTR) {
                continue;
            }

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

        // bash exited: the PTY master hangs up
        if (fds[1].revents & (POLLHUP | POLLERR)) {
            running = false;
        }
    }

    close(master_fd);
    waitpid(
        pid,
        nullptr,
        0
    );
}
