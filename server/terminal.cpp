#include "terminal.hpp"

#include <iostream>
#include <cstdlib>

#include <poll.h>
#include <pty.h>
#include <unistd.h>
#include <sys/wait.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <signal.h>

using namespace std;

void Terminal::start(int client_fd) {
    int master_fd;

    // Provisional: the real size will be negotiated by the protocol in v0.3.
    struct winsize size = {};
    size.ws_row = 24;
    size.ws_col = 80;

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
    pollfd fds[2];

    fds[0].fd = client_fd;
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
            if (errno == EINTR) {
                continue;
            }

            cerr << "failed to poll\n";
            break;
        }

        // client -> PTY
        if (fds[0].revents & POLLIN) {
            char buffer[4096];

            ssize_t bytes_read = read(
                client_fd,
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

        // PTY -> client
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
                client_fd,
                buffer,
                bytes_read
            );
        }

        // the client hung up, or bash exited
        if ((fds[0].revents | fds[1].revents) & (POLLHUP | POLLERR)) {
            running = false;
        }
    }

    close(master_fd);

    kill(pid, SIGHUP);
    waitpid(
        pid,
        nullptr,
        0
    );
}
