//
// Created by davidjaramillo on 8/28/26.
//

#pragma once

#include <termios.h>

class RawMode {
public:
    RawMode();
    ~RawMode();

private:
    struct termios original;
};
