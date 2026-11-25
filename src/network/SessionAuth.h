#pragma once

#include <cstdint>

namespace klima {

class SessionAuth {
public:
    bool login(const char* password, const char* hash, uint64_t nowUs, char (&token)[33]);
    uint32_t authorize(const char* token, const char* hash, uint64_t nowUs);
    void logout(const char* token);
private:
    void checkVerifier(const char* hash);
    struct Entry { char token[33]{}; uint32_t owner{}; uint64_t expiresUs{}; } sessions_[4]{};
    char verifier_[129]{};
    uint64_t nextLoginUs_{};
};

} // namespace klima
