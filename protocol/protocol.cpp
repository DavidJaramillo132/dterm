#include "protocol.hpp"

#include <unistd.h>
#include <errno.h>
#include <cstring>

namespace protocol {

    namespace {

        Status write_all(int fd, const std::uint8_t *data, std::size_t length) {
            std::size_t sent = 0;

            while (sent < length) {
                ssize_t written = write(fd, data + sent, length - sent);

                if (written == -1) {
                    if (errno == EINTR) {
                        continue;
                    }

                    return Status::Error;
                }

                if (written == 0) {
                    return Status::Closed;
                }

                sent += static_cast<std::size_t>(written);
            }

            return Status::Ok;
        }

        std::uint32_t read_uint32(const std::uint8_t *data) {
            return (static_cast<std::uint32_t>(data[0]) << 24) |
                   (static_cast<std::uint32_t>(data[1]) << 16) |
                   (static_cast<std::uint32_t>(data[2]) << 8) |
                   (static_cast<std::uint32_t>(data[3]));
        }

        void write_uint32(std::uint8_t *data, std::uint32_t value) {
            data[0] = static_cast<std::uint8_t>(value >> 24);
            data[1] = static_cast<std::uint8_t>(value >> 16);
            data[2] = static_cast<std::uint8_t>(value >> 8);
            data[3] = static_cast<std::uint8_t>(value);
        }
    }

    Status send_frame(int fd, Type type, const void *payload, std::uint32_t length) {
        if (length > MAX_PAYLOAD) {
            return Status::Error;
        }

        // One buffer, one write: never split a header across TCP segments.
        std::vector<std::uint8_t> message(HEADER_SIZE + length);

        message[0] = static_cast<std::uint8_t>(type);
        write_uint32(message.data() + 1, length);

        if (length > 0) {
            std::memcpy(message.data() + HEADER_SIZE, payload, length);
        }

        return write_all(fd, message.data(), message.size());
    }

    Status send_frame(int fd, Type type) {
        return send_frame(fd, type, nullptr, 0);
    }

    Status FrameReader::feed(int fd) {
        std::uint8_t chunk[4096];

        ssize_t bytes_read = read(fd, chunk, sizeof(chunk));

        if (bytes_read == -1) {
            if (errno == EINTR) {
                return Status::Ok;
            }

            return Status::Error;
        }

        if (bytes_read == 0) {
            return Status::Closed;
        }

        buffer.insert(buffer.end(), chunk, chunk + bytes_read);
        return Status::Ok;
    }

    Parse FrameReader::next(Frame &frame) {
        if (buffer.size() < HEADER_SIZE) {
            return Parse::Incomplete;
        }

        const std::uint32_t length = read_uint32(buffer.data() + 1);

        if (length > MAX_PAYLOAD) {
            return Parse::Malformed;
        }

        const std::size_t total = HEADER_SIZE + length;

        if (buffer.size() < total) {
            return Parse::Incomplete;
        }

        frame.type = static_cast<Type>(buffer[0]);
        frame.payload.assign(buffer.begin() + HEADER_SIZE, buffer.begin() + total);

        buffer.erase(buffer.begin(), buffer.begin() + total);

        return Parse::Ready;
    }

    std::vector<std::uint8_t> encode_resize(std::uint16_t rows, std::uint16_t cols) {
        return {
            static_cast<std::uint8_t>(rows >> 8),
            static_cast<std::uint8_t>(rows),
            static_cast<std::uint8_t>(cols >> 8),
            static_cast<std::uint8_t>(cols),
        };
    }

    bool decode_resize(const std::vector<std::uint8_t> &payload,
                       std::uint16_t &rows, std::uint16_t &cols) {
        if (payload.size() != 4) {
            return false;
        }

        rows = static_cast<std::uint16_t>((payload[0] << 8) | payload[1]);
        cols = static_cast<std::uint16_t>((payload[2] << 8) | payload[3]);

        return true;
    }
}
