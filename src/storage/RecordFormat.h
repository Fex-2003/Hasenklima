#pragma once

#include "Identification.h"

#include <cstddef>

namespace klima {

constexpr uint16_t kTestSchemaVersion = 1;
constexpr size_t kTestRecordBytes = 32;
constexpr size_t kTestHeaderBytes = 256;
constexpr char kTestCsvColumns[] = "time_s,plate_median_c,room_temp_c,outside_temp_c,vin_v,pwm_effective_percent,plate_max_c\n";

using TestRecord = std::array<uint8_t, kTestRecordBytes>;
using TestHeader = std::array<uint8_t, kTestHeaderBytes>;

uint32_t crc32(const uint8_t* bytes, size_t size);
void putLe32(uint8_t* bytes, uint32_t value);
uint32_t getLe32(const uint8_t* bytes);
void putLe64(uint8_t* bytes, uint64_t value);
uint64_t getLe64(const uint8_t* bytes);
void putLeFloat(uint8_t* bytes, float value);
float getLeFloat(const uint8_t* bytes);

TestHeader encodeTestHeader(const TestMetadata& metadata);
bool decodeTestHeader(const TestHeader& bytes, TestMetadata& metadata);
TestRecord encodeTestPoint(const TestPoint& point);
bool decodeTestPoint(const TestRecord& bytes, TestPoint& point);

enum class RecordFrame : uint8_t { Invalid, Point, Start, Transition, Terminal };

// Markers use a reserved elapsed-time word, leaving ordinary points at exactly 32 bytes.
TestRecord encodeStartMarker(const TestMetadata& metadata);
TestRecord encodePhaseMarker(const TestMetadata& metadata, uint32_t count, bool terminal);
RecordFrame inspectFrame(const TestRecord& bytes);
bool terminalFrameTag(const TestRecord& bytes);
bool applyMarker(const TestRecord& bytes, TestMetadata& metadata, uint32_t count);
int formatCsvPoint(char* output, size_t capacity, const TestPoint& point);

class TestPrefix {
public:
    explicit TestPrefix(const TestMetadata& metadata) : metadata_(metadata) {}
    bool append(const TestRecord& frame);
    bool terminal() const { return terminal_; }
    uint32_t points() const { return points_; }
    const TestMetadata& metadata() const { return metadata_; }

private:
    TestMetadata metadata_;
    uint32_t points_ = 0, previousMs_ = 0;
    bool terminal_ = false, broken_ = false;
};

} // namespace klima
