#pragma once

#include <cstddef>
#include <cstdint>

namespace klima {

// Bounded, single-pass JSON output. A response is usable only if good() is true.
class JsonWriter {
public:
    JsonWriter(char* buffer, size_t capacity);
    bool good() const { return good_ && length_ && depth_ == 0; }
    size_t size() const { return length_; }
    void object(const char* key = nullptr);
    void array(const char* key = nullptr);
    void end();
    void text(const char* key, const char* value);
    void boolean(const char* key, bool value);
    void number(const char* key, double value, int precision = 8);
    void integer(const char* key, uint64_t value);
    void hex(const char* key, uint64_t value, bool unsetNull = false);
    void nullable(const char* key);

private:
    void prefix(const char* key);
    void token(const char* value);
    void character(char value);
    void quoted(const char* value);
    void open(const char* key, char opening, char closing);

    char* buffer_;
    size_t capacity_, length_{};
    int depth_{};
    bool good_;
    struct Context { char closing; bool first; } stack_[10]{};
};

} // namespace klima
