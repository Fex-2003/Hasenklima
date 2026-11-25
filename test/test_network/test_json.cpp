#include "network/JsonWriter.h"
#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
unsigned failures = 0;
void check(const char* name, bool passed) {
    std::printf("%s %s\n", passed ? "PASS" : "FAIL", name);
    if (!passed) ++failures;
}
}

int main() {
    char buffer[256];
    klima::JsonWriter writer(buffer, sizeof(buffer));
    writer.object();
    writer.number("ungueltig", NAN);
    writer.text("text", "Anf\"uehrung\\ und\nZeile");
    writer.array("werte");
    writer.number(nullptr, 12.5);
    writer.boolean(nullptr, false);
    writer.hex(nullptr, UINT64_MAX);
    writer.hex(nullptr, 0, true);
    writer.end();
    writer.end();
    check("HTTP JSON keeps invalid channels null and ROM IDs exact", writer.good() &&
        std::strcmp(buffer, "{\"ungueltig\":null,\"text\":\"Anf\\\"uehrung\\\\ und\\u000aZeile\",\"werte\":[12.5,false,\"FFFFFFFFFFFFFFFF\",null]}") == 0);
    check("HTTP JSON reports exact payload length", writer.size() == std::strlen(buffer));
    char shortBuffer[7];
    klima::JsonWriter shortWriter(shortBuffer, sizeof(shortBuffer));
    shortWriter.text(nullptr, "1234567");
    check("HTTP JSON does not transmit a truncated response", !shortWriter.good() && shortBuffer[6] == '\0');
    klima::JsonWriter incomplete(buffer, sizeof(buffer));
    incomplete.object();
    incomplete.text("key", "value");
    check("HTTP JSON requires closed objects", !incomplete.good());
    klima::JsonWriter arrayWriter(buffer, sizeof(buffer));
    arrayWriter.array();
    arrayWriter.boolean("illegal_key", true);
    arrayWriter.end();
    check("HTTP JSON rejects keyed members of arrays", !arrayWriter.good());
    return failures ? 1 : 0;
}
