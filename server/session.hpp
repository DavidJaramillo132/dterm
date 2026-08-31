//
// A session is a bash process with its own PTY that outlives the network
// connection. It lives in its own process and listens on a Unix socket under
// ~/.dterm/sessions/, so a client that drops off the network can come back and
// find its shell exactly where it left it.
//

#pragma once

#include <string>

namespace session {

    inline constexpr char DEFAULT_NAME[] = "default";

    // Session names become file names, so they are deliberately restrictive.
    bool valid_name(const std::string &name);

    // Connects to the named session, starting it first if it is not running.
    // `close_in_child` is a descriptor the session process must not inherit
    // (the caller's client socket). Returns -1 on failure.
    int open(const std::string &name, int close_in_child);
}
