#include "protocol.hpp"

#include <iostream>
#include <vector>
#include <cstring>

#include <sys/socket.h>
#include <unistd.h>

using namespace std;
using namespace protocol;

namespace {
    int failures = 0;

    void check(bool condition, const string &what) {
        cout << (condition ? "  ok   " : "  FAIL ") << what << "\n";
        if (!condition) {
            ++failures;
        }
    }

    // Serialises the three test frames into a raw byte stream.
    vector<uint8_t> build_stream() {
        int pair[2];
        socketpair(AF_UNIX, SOCK_STREAM, 0, pair);

        const char keys[] = "ls\n";
        send_frame(pair[0], Type::Input, keys, 3);

        const vector<uint8_t> resize = encode_resize(45, 150);
        send_frame(pair[0], Type::Resize, resize.data(), resize.size());

        send_frame(pair[0], Type::Ping);
        close(pair[0]);

        vector<uint8_t> stream;
        uint8_t chunk[256];
        ssize_t n;
        while ((n = read(pair[1], chunk, sizeof(chunk))) > 0) {
            stream.insert(stream.end(), chunk, chunk + n);
        }
        close(pair[1]);

        return stream;
    }

    // Pushes the stream through a FrameReader in slices of `chunk_size`.
    vector<Frame> parse_in_chunks(const vector<uint8_t> &stream, size_t chunk_size) {
        int pair[2];
        socketpair(AF_UNIX, SOCK_STREAM, 0, pair);

        FrameReader reader;
        vector<Frame> frames;

        for (size_t offset = 0; offset < stream.size(); offset += chunk_size) {
            const size_t size = min(chunk_size, stream.size() - offset);
            write(pair[0], stream.data() + offset, size);

            reader.feed(pair[1]);

            Frame frame;
            while (reader.next(frame) == Parse::Ready) {
                frames.push_back(frame);
            }
        }

        close(pair[0]);
        close(pair[1]);

        return frames;
    }

    void expect_three(const vector<Frame> &frames, const string &label) {
        cout << label << "\n";

        if (frames.size() != 3) {
            check(false, "3 frames (got " + to_string(frames.size()) + ")");
            return;
        }

        check(frames[0].type == Type::Input, "frame 0 is INPUT");
        check(frames[0].payload == vector<uint8_t>({'l', 's', '\n'}), "frame 0 payload is \"ls\\n\"");

        check(frames[1].type == Type::Resize, "frame 1 is RESIZE");
        uint16_t rows = 0, cols = 0;
        check(decode_resize(frames[1].payload, rows, cols), "frame 1 decodes");
        check(rows == 45 && cols == 150, "frame 1 is 45x150 (got " +
              to_string(rows) + "x" + to_string(cols) + ")");

        check(frames[2].type == Type::Ping, "frame 2 is PING");
        check(frames[2].payload.empty(), "frame 2 payload is empty");
    }
}

int main() {
    const vector<uint8_t> stream = build_stream();
    cout << "raw stream: " << stream.size() << " bytes\n\n";

    expect_three(parse_in_chunks(stream, stream.size()), "all bytes at once:");
    cout << "\n";
    expect_three(parse_in_chunks(stream, 1), "one byte at a time:");
    cout << "\n";
    expect_three(parse_in_chunks(stream, 7), "in slices of 7 bytes:");

    cout << "\n" << (failures == 0 ? "ALL PASSED" : to_string(failures) + " FAILED") << "\n";
    return failures == 0 ? 0 : 1;
}
