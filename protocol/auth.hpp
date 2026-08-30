//
// Shared-secret authentication for DTerm.
//
// The secret never crosses the wire. The server sends 32 random bytes, the
// client answers with HMAC-SHA256(secret, challenge), and the server recomputes
// the same MAC and compares. An eavesdropper sees a challenge and a MAC, and
// neither one can be replayed: the next challenge is different.
//

#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace protocol {

    inline constexpr std::size_t CHALLENGE_SIZE = 32;
    inline constexpr std::size_t MAC_SIZE = 32;   // SHA-256 output

    // Reads the shared secret from $DTERM_SECRET, or from ~/.dterm/secret.
    // Returns false and fills `error` with an actionable message.
    bool load_secret(std::string &secret, std::string &error);

    // Cryptographically strong random bytes. Empty vector on failure.
    std::vector<std::uint8_t> random_bytes(std::size_t count);

    std::vector<std::uint8_t> hmac_sha256(const std::string &key,
                                          const std::vector<std::uint8_t> &data);

    // Comparison whose running time does not depend on where the first
    // mismatching byte is, so it leaks nothing about the expected MAC.
    bool constant_time_equal(const std::vector<std::uint8_t> &a,
                             const std::vector<std::uint8_t> &b);
}
