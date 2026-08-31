#pragma once

#include "protocol.hpp"

#include <string>

// Runs HELLO -> CHALLENGE -> AUTH -> ATTACH before anything is spawned.
// On success `session_name` holds the session the client asked for, and
// `reader` may still hold bytes the client sent behind the ATTACH frame.
bool handshake(int client_fd, protocol::FrameReader &reader,
               const std::string &secret, std::string &session_name);
