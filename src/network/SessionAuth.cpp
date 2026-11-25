#include "SessionAuth.h"
#include <esp_system.h>
#include <mbedtls/md.h>
#include <mbedtls/pkcs5.h>
#include <cstddef>
#include <cstring>

namespace klima {
namespace {
constexpr char kPrefix[] = "pbkdf2-sha256$";
constexpr uint32_t kIterations = 20000;
constexpr uint64_t kLifetimeUs = 12 * 3600 * 1000000ULL;

void toHex(const uint8_t* in, size_t length, char* out) {
    constexpr char digits[] = "0123456789abcdef";
    for (size_t i = 0; i < length; ++i) {
        out[i * 2] = digits[in[i] >> 4];
        out[i * 2 + 1] = digits[in[i] & 15];
    }
    out[length * 2] = '\0';
}

bool fromHex(const char* in, uint8_t* out, size_t length) {
    for (size_t i = 0; i < length * 2; ++i) {
        const char c = in[i];
        const int digit = c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1;
        if (digit < 0) return false;
        if (i % 2 == 0) out[i / 2] = static_cast<uint8_t>(digit << 4);
        else out[i / 2] |= static_cast<uint8_t>(digit);
    }
    return true;
}

bool constantEqual(const void* a, const void* b, size_t length) {
    const auto* first = static_cast<const uint8_t*>(a);
    const auto* second = static_cast<const uint8_t*>(b);
    uint8_t difference = 0;
    for (size_t i = 0; i < length; ++i) difference |= first[i] ^ second[i];
    return difference == 0;
}

bool derive(const char* password, const uint8_t (&salt)[16], uint8_t (&derived)[32]) {
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    const int setup = mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 1);
    const bool ok = setup == 0 &&
        mbedtls_pkcs5_pbkdf2_hmac(&ctx, reinterpret_cast<const uint8_t*>(password), strlen(password),
            salt, sizeof(salt), kIterations, sizeof(derived), derived) == 0;
    mbedtls_md_free(&ctx);
    return ok;
}

bool verify(const char* password, const char* encoded) {
    constexpr size_t prefixLen = sizeof(kPrefix) - 1;
    if (strlen(encoded) != prefixLen + 97 || strncmp(encoded, kPrefix, prefixLen) ||
        encoded[prefixLen + 32] != '$' || strlen(password) < 12 || strlen(password) > 128) return false;
    uint8_t salt[16], stored[32], calculated[32];
    return fromHex(encoded + prefixLen, salt, sizeof(salt)) &&
        fromHex(encoded + prefixLen + 33, stored, sizeof(stored)) && derive(password, salt, calculated) &&
        constantEqual(stored, calculated, sizeof(stored));
}
} // namespace

bool makePasswordHash(const char* password, char* out, size_t capacity) {
    constexpr size_t prefixLen = sizeof(kPrefix) - 1;
    if (!password || !out || capacity < prefixLen + 98 || strlen(password) < 12 || strlen(password) > 128)
        return false;
    uint8_t salt[16], derived[32];
    esp_fill_random(salt, sizeof(salt));
    if (!derive(password, salt, derived)) return false;
    memcpy(out, kPrefix, prefixLen);
    toHex(salt, sizeof(salt), out + prefixLen);
    out[prefixLen + 32] = '$';
    toHex(derived, sizeof(derived), out + prefixLen + 33);
    return true;
}

void SessionAuth::checkVerifier(const char* hash) {
    if (strcmp(hash, verifier_) == 0) return;
    for (auto& session : sessions_) session = {};
    strncpy(verifier_, hash, sizeof(verifier_) - 1);
}

bool SessionAuth::login(const char* password, const char* hash, uint64_t nowUs, char (&token)[33]) {
    checkVerifier(hash);
    if (nowUs < nextLoginUs_) return false;
    nextLoginUs_ = nowUs + 1000000;
    if (!verify(password, hash)) return false;
    Entry* entry = &sessions_[0];
    for (auto& candidate : sessions_)
        if (candidate.expiresUs < entry->expiresUs) entry = &candidate;
    uint8_t bytes[16];
    esp_fill_random(bytes, sizeof(bytes));
    toHex(bytes, sizeof(bytes), entry->token);
    memcpy(token, entry->token, sizeof(entry->token));
    do { entry->owner = esp_random(); } while (!entry->owner);
    entry->expiresUs = nowUs + kLifetimeUs;
    return true;
}

uint32_t SessionAuth::authorize(const char* token, const char* hash, uint64_t nowUs) {
    checkVerifier(hash);
    if (strlen(token) != 32) return 0;
    for (const auto& session : sessions_)
        if (session.expiresUs > nowUs && constantEqual(session.token, token, 32)) return session.owner;
    return 0;
}

void SessionAuth::logout(const char* token) {
    if (strlen(token) != 32) return;
    for (auto& session : sessions_)
        if (constantEqual(session.token, token, 32)) session = {};
}

} // namespace klima
