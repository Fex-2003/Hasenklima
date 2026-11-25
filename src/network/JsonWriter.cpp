#include "JsonWriter.h"
#include <cmath>
#include <cstdio>

namespace klima {

JsonWriter::JsonWriter(char* buffer, size_t capacity)
    : buffer_(buffer), capacity_(capacity), good_(capacity != 0) {
    if (good_) buffer_[0] = '\0';
}

void JsonWriter::character(char value) {
    if (!good_ || length_ + 1 >= capacity_) {
        good_ = false;
        return;
    }
    buffer_[length_++] = value;
    buffer_[length_] = '\0';
}

void JsonWriter::token(const char* value) {
    for (; *value && good_; ++value) character(*value);
}

void JsonWriter::quoted(const char* value) {
    static constexpr char digits[] = "0123456789abcdef";
    character('"');
    for (const unsigned char* p = reinterpret_cast<const unsigned char*>(value); *p && good_; ++p) {
        if (*p == '"' || *p == '\\') {
            character('\\');
            character(*p);
        } else if (*p < 0x20) {
            token("\\u00");
            character(digits[*p >> 4]);
            character(digits[*p & 15]);
        } else {
            character(*p);
        }
    }
    character('"');
}

void JsonWriter::prefix(const char* key) {
    if (depth_ == 0) {
        if (key || length_) good_ = false;
        return;
    }
    auto& context = stack_[depth_ - 1];
    if ((context.closing == '}') != (key != nullptr)) {
        good_ = false;
        return;
    }
    if (!context.first) character(',');
    context.first = false;
    if (key) {
        quoted(key);
        character(':');
    }
}

void JsonWriter::open(const char* key, char opening, char closing) {
    prefix(key);
    if (!good_ || depth_ == static_cast<int>(sizeof(stack_) / sizeof(stack_[0]))) {
        good_ = false;
        return;
    }
    character(opening);
    stack_[depth_++] = {closing, true};
}

void JsonWriter::object(const char* key) { open(key, '{', '}'); }
void JsonWriter::array(const char* key) { open(key, '[', ']'); }
void JsonWriter::end() {
    if (depth_ == 0) { good_ = false; return; }
    character(stack_[--depth_].closing);
}

void JsonWriter::text(const char* key, const char* value) {
    if (!value) { nullable(key); return; }
    prefix(key);
    quoted(value);
}
void JsonWriter::boolean(const char* key, bool value) {
    prefix(key);
    token(value ? "true" : "false");
}
void JsonWriter::number(const char* key, double value, int precision) {
    if (!std::isfinite(value)) { nullable(key); return; }
    prefix(key);
    char digits[40];
    snprintf(digits, sizeof(digits), "%.*g", precision, value);
    token(digits);
}
void JsonWriter::integer(const char* key, uint64_t value) {
    prefix(key);
    char digits[21];
    snprintf(digits, sizeof(digits), "%llu", static_cast<unsigned long long>(value));
    token(digits);
}
void JsonWriter::hex(const char* key, uint64_t value, bool unsetNull) {
    if (unsetNull && value == 0) { nullable(key); return; }
    char digits[17];
    snprintf(digits, sizeof(digits), "%016llX", static_cast<unsigned long long>(value));
    text(key, digits);
}
void JsonWriter::nullable(const char* key) { prefix(key); token("null"); }

} // namespace klima
