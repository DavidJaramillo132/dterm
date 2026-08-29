//
// Created by davidjaramillo on 8/28/26.
//

#include "raw_mode.h"

#include <unistd.h>

RawMode::RawMode() {
    tcgetattr(STDIN_FILENO, &original);

    struct termios raw = original;
    cfmakeraw(&raw);

    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
}

RawMode::~RawMode() {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &original);
}
