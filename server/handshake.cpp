#include "handshake.hpp"
#include "session.hpp"
#include "auth.hpp"

#include <iostream>
#include <vector>

#include <errno.h>
#include <poll.h>

using namespace std;
using namespace protocol;

namespace {

    constexpr int HANDSHAKE_TIMEOUT_MS = 5000;

    // Blocks until one whole frame arrives, or the client stays silent too long.
    bool next_frame(int client_fd, FrameReader &reader, Frame &frame) {
        pollfd fd = {client_fd, POLLIN, 0};

        while (true) {
            const Parse parsed = reader.next(frame);

            if (parsed == Parse::Ready) {
                return true;
            }

            if (parsed == Parse::Malformed) {
                cerr << "malformed frame during handshake" << endl;
                return false;
            }

            const int ready = poll(&fd, 1, HANDSHAKE_TIMEOUT_MS);

            if (ready == -1) {
                if (errno == EINTR) {
                    continue;
                }

                return false;
            }

            if (ready == 0) {
                cerr << "handshake timed out" << endl;
                return false;
            }

            if (reader.feed(client_fd) != Status::Ok) {
                return false;
            }
        }
    }

    // Checks the client speaks our protocol before anything else happens.
    bool check_hello(const Frame &frame) {
        if (frame.type != Type::Hello) {
            cerr << "expected HELLO, got frame type "
                 << static_cast<int>(frame.type) << endl;
            return false;
        }

        if (frame.payload.size() != 5 ||
            frame.payload[0] != 'D' || frame.payload[1] != 'T' ||
            frame.payload[2] != 'R' || frame.payload[3] != 'M') {
            cerr << "bad HELLO signature" << endl;
            return false;
        }

        if (frame.payload[4] != VERSION) {
            cerr << "client speaks protocol v" << static_cast<int>(frame.payload[4])
                 << ", server speaks v" << static_cast<int>(VERSION) << endl;
            return false;
        }

        return true;
    }

    // Proves the client holds the shared secret without either side sending it.
    bool authenticate(int client_fd, FrameReader &reader, const string &secret) {
        const vector<uint8_t> challenge = random_bytes(CHALLENGE_SIZE);

        if (challenge.size() != CHALLENGE_SIZE) {
            cerr << "failed to generate a challenge" << endl;
            return false;
        }

        if (send_frame(client_fd, Type::Challenge, challenge.data(),
                       static_cast<uint32_t>(challenge.size())) != Status::Ok) {
            return false;
        }

        Frame frame;

        if (!next_frame(client_fd, reader, frame)) {
            return false;
        }

        if (frame.type != Type::Auth) {
            cerr << "expected AUTH, got frame type "
                 << static_cast<int>(frame.type) << endl;
            return false;
        }

        const vector<uint8_t> expected = hmac_sha256(secret, challenge);

        if (expected.empty() || !constant_time_equal(expected, frame.payload)) {
            cerr << "authentication failed: wrong secret" << endl;
            send_frame(client_fd, Type::AuthFail);
            return false;
        }

        cout << "client authenticated" << endl;
        return send_frame(client_fd, Type::AuthOk) == Status::Ok;
    }

    bool handshake(int client_fd, FrameReader &reader, const string &secret) {
        Frame frame;

        if (!next_frame(client_fd, reader, frame)) {
            return false;
        }

        if (!check_hello(frame)) {
            return false;
        }

        return authenticate(client_fd, reader, secret);
    }

    // The client names the session it wants. An empty payload means the
    // default one, which is what a client with no preference sends.
    bool attach_request(int client_fd, FrameReader &reader, string &session_name) {
        Frame frame;

        if (!next_frame(client_fd, reader, frame)) {
            return false;
        }

        if (frame.type != Type::Attach) {
            cerr << "expected ATTACH, got frame type "
                 << static_cast<int>(frame.type) << endl;
            return false;
        }

        session_name.assign(frame.payload.begin(), frame.payload.end());

        if (session_name.empty()) {
            session_name = session::DEFAULT_NAME;
        }

        if (!session::valid_name(session_name)) {
            cerr << "rejected session name" << endl;
            return false;
        }

        return true;
    }
}

bool handshake(int client_fd, FrameReader &reader,
               const string &secret, string &session_name) {
    Frame frame;

    if (!next_frame(client_fd, reader, frame)) {
        return false;
    }

    if (!check_hello(frame)) {
        return false;
    }

    if (!authenticate(client_fd, reader, secret)) {
        return false;
    }

    return attach_request(client_fd, reader, session_name);
}
