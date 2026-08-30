#include "auth.hpp"

#include <openssl/crypto.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <sstream>

#include <pwd.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

namespace {

    string secret_path() {
        const char *home = getenv("HOME");

        if (home == nullptr) {
            const passwd *entry = getpwuid(getuid());
            home = entry != nullptr ? entry->pw_dir : "";
        }

        return string(home) + "/.dterm/secret";
    }

    string trim(const string &text) {
        size_t begin = 0;
        size_t end = text.size();

        while (begin < end && isspace(static_cast<unsigned char>(text[begin]))) {
            ++begin;
        }

        while (end > begin && isspace(static_cast<unsigned char>(text[end - 1]))) {
            --end;
        }

        return text.substr(begin, end - begin);
    }
}

bool protocol::load_secret(string &secret, string &error) {
    if (const char *from_env = getenv("DTERM_SECRET"); from_env != nullptr) {
        secret = trim(from_env);

        if (secret.empty()) {
            error = "DTERM_SECRET is set but empty";
            return false;
        }

        return true;
    }

    const string path = secret_path();

    struct stat info = {};

    if (stat(path.c_str(), &info) == -1) {
        error = "no shared secret found. Create one on BOTH machines:\n"
                "  mkdir -p ~/.dterm && openssl rand -hex 32 > ~/.dterm/secret"
                " && chmod 600 ~/.dterm/secret\n"
                "or export DTERM_SECRET=<same value on both sides>";
        return false;
    }

    // Group or other permissions on a secret defeat the point of having one.
    if ((info.st_mode & (S_IRWXG | S_IRWXO)) != 0) {
        error = path + " is readable by other users. Run: chmod 600 " + path;
        return false;
    }

    ifstream file(path);

    if (!file) {
        error = "cannot open " + path;
        return false;
    }

    ostringstream contents;
    contents << file.rdbuf();
    secret = trim(contents.str());

    if (secret.empty()) {
        error = path + " is empty";
        return false;
    }

    return true;
}

vector<uint8_t> protocol::random_bytes(size_t count) {
    vector<uint8_t> bytes(count);

    if (RAND_bytes(bytes.data(), static_cast<int>(count)) != 1) {
        return {};
    }

    return bytes;
}

vector<uint8_t> protocol::hmac_sha256(const string &key,
                                      const vector<uint8_t> &data) {
    vector<uint8_t> mac(EVP_MAX_MD_SIZE);
    unsigned int length = 0;

    const unsigned char *result = HMAC(EVP_sha256(),
                                       key.data(), static_cast<int>(key.size()),
                                       data.data(), data.size(),
                                       mac.data(), &length);

    if (result == nullptr) {
        return {};
    }

    mac.resize(length);
    return mac;
}

bool protocol::constant_time_equal(const vector<uint8_t> &a,
                                   const vector<uint8_t> &b) {
    if (a.size() != b.size()) {
        return false;
    }

    return CRYPTO_memcmp(a.data(), b.data(), a.size()) == 0;
}
