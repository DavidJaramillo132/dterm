//
// DTerm wire protocol: length-prefixed binary frames.
//
//  0        1                                5
//  +--------+--------+--------+--------+--------+---------------+
//  |  type  |          length (uint32 BE)       |    payload    |
//  +--------+--------+--------+--------+--------+---------------+
//

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace protocol {

    inline constexpr std::size_t HEADER_SIZE = 5;
    inline constexpr std::uint32_t MAX_PAYLOAD = 1u << 20;   // 1 MiB
    inline constexpr std::uint8_t VERSION = 1;

    enum class Type : std::uint8_t {
        Hello  = 0x00,   // payload: "DTRM" + version byte
        Input  = 0x01,   // client -> server: keystrokes
        Output = 0x02,   // server -> client: PTY output
        Resize = 0x03,   // client -> server: rows + cols, uint16 BE each
        Ping   = 0x04,
        Pong   = 0x05,

        Challenge = 0x06,   // server -> client: CHALLENGE_SIZE random bytes
        Auth      = 0x07,   // client -> server: HMAC-SHA256(secret, challenge)
        AuthOk    = 0x08,
        AuthFail  = 0x09,
    };

    struct Frame {
        Type type;
        std::vector<std::uint8_t> payload;
    };

    enum class Status {
        Ok,
        Closed,      // the peer hung up
        Error,       // the syscall failed
    };

    enum class Parse {
        Incomplete,  // not enough bytes yet, keep feeding
        Ready,       // a whole frame came out
        Malformed,   // unusable stream, drop the connection
    };

    // Writes one whole frame, looping over partial writes.
    Status send_frame(int fd, Type type, const void *payload, std::uint32_t length);

    Status send_frame(int fd, Type type);

    // Accumulates bytes off a socket and hands back whole frames.
    class FrameReader {
    public:
        // One read() into the internal buffer.
        Status feed(int fd);

        // Pops a complete frame if the buffer holds one.
        Parse next(Frame &frame);

    private:
        std::vector<std::uint8_t> buffer;
    };

    std::vector<std::uint8_t> encode_resize(std::uint16_t rows, std::uint16_t cols);
    bool decode_resize(const std::vector<std::uint8_t> &payload,
                       std::uint16_t &rows, std::uint16_t &cols);
}
