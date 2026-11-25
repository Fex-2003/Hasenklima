#include "RecordFormat.h"

#include <cstdio>
#include <cstring>
#include <limits>

namespace klima {
namespace {
constexpr char kMagic[] = "KLIMATST";
constexpr uint32_t kStartMarker = 0xfffffffdu;
constexpr uint32_t kPhaseMarker = 0xfffffffeu;
constexpr uint32_t kEndMarker = 0xffffffffu;
constexpr uint32_t kFormat = 0x01012001; // schema=1, ISO-HDLC=1, record size=32, little endian=1
constexpr std::array<float TestMetadata::*, 13> kFrozenFloats{{
    &TestMetadata::resistanceOhm, &TestMetadata::currentLimitA, &TestMetadata::plateLimitC,
    &TestMetadata::vinGain, &TestMetadata::vinOffsetV, &TestMetadata::vinMinV, &TestMetadata::vinMaxV,
    &TestMetadata::piKp, &TestMetadata::piTiS, &TestMetadata::heatingTargetC, &TestMetadata::coolingTargetC,
    &TestMetadata::supplyRatedCurrentA, &TestMetadata::confirmedDuty
}};

template <size_t N> bool validCrc(const std::array<uint8_t, N>& bytes) {
    return crc32(bytes.data(), N - 4) == getLe32(bytes.data() + N - 4);
}

template <size_t N> void appendCrc(std::array<uint8_t, N>& bytes) {
    putLe32(bytes.data() + N - 4, crc32(bytes.data(), N - 4));
}
} // namespace

uint32_t crc32(const uint8_t* bytes, size_t size) {
    uint32_t crc = 0xffffffffu;
    while (size--) {
        crc ^= *bytes++;
        for (int bit = 0; bit < 8; ++bit) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return crc ^ 0xffffffffu;
}

void putLe32(uint8_t* bytes, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) bytes[i] = static_cast<uint8_t>(value >> (8 * i));
}

uint32_t getLe32(const uint8_t* bytes) {
    uint32_t value = 0;
    for (unsigned i = 0; i < 4; ++i) value |= static_cast<uint32_t>(bytes[i]) << (8 * i);
    return value;
}

void putLe64(uint8_t* bytes, uint64_t value) {
    putLe32(bytes, static_cast<uint32_t>(value));
    putLe32(bytes + 4, static_cast<uint32_t>(value >> 32));
}

uint64_t getLe64(const uint8_t* bytes) {
    return getLe32(bytes) | (static_cast<uint64_t>(getLe32(bytes + 4)) << 32);
}

void putLeFloat(uint8_t* bytes, float value) {
    static_assert(sizeof(float) == 4 && std::numeric_limits<float>::is_iec559, "IEEE float32 required");
    uint32_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    putLe32(bytes, bits);
}

float getLeFloat(const uint8_t* bytes) {
    const uint32_t bits = getLe32(bytes);
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}

TestHeader encodeTestHeader(const TestMetadata& meta) {
    TestHeader bytes{};
    std::memcpy(bytes.data(), kMagic, 8);
    putLe32(bytes.data() + 8, kFormat);
    putLe32(bytes.data() + 12, kTestHeaderBytes);
    putLe64(bytes.data() + 16, meta.testId);
    putLe64(bytes.data() + 24, meta.bootId);
    putLe64(bytes.data() + 32, meta.startUs);
    putLe64(bytes.data() + 40, meta.startUtcSeconds);
    putLe32(bytes.data() + 48, meta.configRevision);
    putLe32(bytes.data() + 52, meta.activeMask);
    putLe32(bytes.data() + 56, meta.plan.baselineMinutes);
    putLe32(bytes.data() + 60, meta.plan.heatMinutes);
    putLe32(bytes.data() + 64, meta.plan.cooldownMinutes);
    putLeFloat(bytes.data() + 68, meta.plan.pwmPercent);
    for (size_t i = 0; i < meta.plateRomIds.size(); ++i) putLe64(bytes.data() + 72 + i * 8, meta.plateRomIds[i]);
    for (size_t i = 0; i < kFrozenFloats.size(); ++i) putLeFloat(bytes.data() + 144 + i * 4, meta.*kFrozenFloats[i]);
    putLe32(bytes.data() + 196, meta.piIntervalMs);
    bytes[200] = meta.piIntegralEnabled;
    bytes[201] = 1; // RUNNING until a verified terminal frame supersedes the header
    std::memcpy(bytes.data() + 204, meta.firmwareVersion.data(), meta.firmwareVersion.size());
    appendCrc(bytes);
    return bytes;
}

bool decodeTestHeader(const TestHeader& bytes, TestMetadata& meta) {
    if (std::memcmp(bytes.data(), kMagic, 8) || getLe32(bytes.data() + 8) != kFormat ||
        getLe32(bytes.data() + 12) != kTestHeaderBytes || bytes[201] != 1 || !validCrc(bytes)) return false;
    const auto mask = getLe32(bytes.data() + 52);
    const auto baseline = getLe32(bytes.data() + 56);
    const auto heat = getLe32(bytes.data() + 60);
    const auto cooldown = getLe32(bytes.data() + 64);
    if (mask == 0 || mask > 0x1ff || baseline > 60 || heat > 500 || cooldown > 60 || bytes[200] > 1) return false;
    TestMetadata decoded{};
    decoded.testId = getLe64(bytes.data() + 16);
    decoded.bootId = getLe64(bytes.data() + 24);
    decoded.startUs = getLe64(bytes.data() + 32);
    decoded.startUtcSeconds = getLe64(bytes.data() + 40);
    decoded.configRevision = getLe32(bytes.data() + 48);
    decoded.activeMask = static_cast<uint16_t>(mask);
    decoded.plan = {static_cast<uint16_t>(baseline), static_cast<uint16_t>(heat),
                    static_cast<uint16_t>(cooldown), getLeFloat(bytes.data() + 68)};
    for (size_t i = 0; i < decoded.plateRomIds.size(); ++i) decoded.plateRomIds[i] = getLe64(bytes.data() + 72 + i * 8);
    for (size_t i = 0; i < kFrozenFloats.size(); ++i) decoded.*kFrozenFloats[i] = getLeFloat(bytes.data() + 144 + i * 4);
    decoded.piIntervalMs = getLe32(bytes.data() + 196);
    decoded.piIntegralEnabled = bytes[200];
    std::memcpy(decoded.firmwareVersion.data(), bytes.data() + 204, decoded.firmwareVersion.size() - 1);
    decoded.firmwareVersion.back() = '\0';
    decoded.phase = TestPhase::Precheck;
    if (!decoded.testId || !decoded.plan.valid() || !(decoded.confirmedDuty > 0 && decoded.confirmedDuty <= 1) ||
        decoded.confirmedDuty * 100 > decoded.plan.pwmPercent) return false;
    meta = decoded;
    return true;
}

TestRecord encodeTestPoint(const TestPoint& point) {
    TestRecord bytes{};
    putLe32(bytes.data(), point.elapsedMs);
    putLeFloat(bytes.data() + 4, point.plateMedianC);
    putLeFloat(bytes.data() + 8, point.roomTempC);
    putLeFloat(bytes.data() + 12, point.outsideTempC);
    putLeFloat(bytes.data() + 16, point.vinV);
    putLeFloat(bytes.data() + 20, point.pwmEffectivePercent);
    putLeFloat(bytes.data() + 24, point.plateMaxC);
    appendCrc(bytes);
    return bytes;
}

bool decodeTestPoint(const TestRecord& bytes, TestPoint& point) {
    if (!validCrc(bytes)) return false;
    const TestPoint decoded{getLe32(bytes.data()), getLeFloat(bytes.data() + 4), getLeFloat(bytes.data() + 8),
                            getLeFloat(bytes.data() + 12), getLeFloat(bytes.data() + 16),
                            getLeFloat(bytes.data() + 20), getLeFloat(bytes.data() + 24)};
    if (!decoded.valid()) return false;
    point = decoded;
    return true;
}

TestRecord encodeStartMarker(const TestMetadata& meta) {
    TestRecord bytes{};
    putLe32(bytes.data(), kStartMarker);
    putLe64(bytes.data() + 4, meta.startUs);
    putLe64(bytes.data() + 12, meta.startUtcSeconds);
    appendCrc(bytes);
    return bytes;
}

TestRecord encodePhaseMarker(const TestMetadata& meta, uint32_t count, bool terminal) {
    TestRecord bytes{};
    putLe32(bytes.data(), terminal ? kEndMarker : kPhaseMarker);
    putLe32(bytes.data() + 4, meta.heatStartMs);
    putLe32(bytes.data() + 8, meta.heatStopMs);
    putLe32(bytes.data() + 12, meta.endMs);
    putLe32(bytes.data() + 16, count);
    putLeFloat(bytes.data() + 20, meta.trigger);
    bytes[24] = static_cast<uint8_t>(meta.reason);
    bytes[25] = static_cast<uint8_t>(meta.phase);
    appendCrc(bytes);
    return bytes;
}

RecordFrame inspectFrame(const TestRecord& bytes) {
    if (!validCrc(bytes)) return RecordFrame::Invalid;
    switch (getLe32(bytes.data())) {
    case kStartMarker: return RecordFrame::Start;
    case kPhaseMarker: return RecordFrame::Transition;
    case kEndMarker: return RecordFrame::Terminal;
    default: return RecordFrame::Point;
    }
}

bool terminalFrameTag(const TestRecord& bytes) {
    return getLe32(bytes.data()) == kEndMarker;
}

bool applyMarker(const TestRecord& bytes, TestMetadata& meta, uint32_t count) {
    const RecordFrame type = inspectFrame(bytes);
    if (type == RecordFrame::Start) {
        if (meta.phase != TestPhase::Precheck) return false;
        meta.startUs = getLe64(bytes.data() + 4);
        meta.startUtcSeconds = getLe64(bytes.data() + 12);
        meta.phase = TestPhase::Baseline;
        return true;
    }
    if ((type != RecordFrame::Transition && type != RecordFrame::Terminal) ||
        getLe32(bytes.data() + 16) != count || bytes[24] > static_cast<uint8_t>(TestReason::Deadline)) return false;
    const uint32_t heatStart = getLe32(bytes.data() + 4);
    const uint32_t heatStop = getLe32(bytes.data() + 8);
    const uint32_t end = getLe32(bytes.data() + 12);
    const auto reason = static_cast<TestReason>(bytes[24]);
    const auto phase = static_cast<TestPhase>(bytes[25]);
    if ((meta.heatStartMs != kNoTestTime && heatStart != meta.heatStartMs) ||
        (meta.heatStopMs != kNoTestTime && heatStop != meta.heatStopMs) ||
        (heatStop != kNoTestTime && (heatStart == kNoTestTime || heatStop < heatStart)) ||
        (end != kNoTestTime && heatStop != kNoTestTime && end < heatStop)) return false;
    if (type == RecordFrame::Terminal) {
        if ((phase != TestPhase::Finishing && phase != TestPhase::Aborted) ||
            (end == kNoTestTime && reason != TestReason::Reset) ||
            (reason == TestReason::None && (count != meta.plan.expectedPoints() || end < meta.plan.totalMs() ||
                                           heatStart == kNoTestTime || heatStop == kNoTestTime))) return false;
        meta.phase = reason == TestReason::None ? TestPhase::Complete : TestPhase::Aborted;
    } else {
        if (phase != TestPhase::Heating && phase != TestPhase::Cooldown && phase != TestPhase::Finishing) return false;
        if (reason != TestReason::None) return false;
        meta.phase = phase;
    }
    meta.heatStartMs = heatStart;
    meta.heatStopMs = heatStop;
    meta.endMs = end;
    meta.reason = reason;
    meta.trigger = getLeFloat(bytes.data() + 20);
    return true;
}

int formatCsvPoint(char* output, size_t capacity, const TestPoint& point) {
    if (!point.valid()) return -1;
    const int n = std::snprintf(output, capacity, "%lu.%03lu,%.6f,%.6f,%.6f,%.6f,%.8f,%.6f\n",
        static_cast<unsigned long>(point.elapsedMs / 1000), static_cast<unsigned long>(point.elapsedMs % 1000),
        point.plateMedianC, point.roomTempC, point.outsideTempC, point.vinV,
        point.pwmEffectivePercent, point.plateMaxC);
    return n >= 0 && static_cast<size_t>(n) < capacity ? n : -1;
}

bool TestPrefix::append(const TestRecord& frame) {
    if (terminal_ || broken_) return false;
    const auto type = inspectFrame(frame);
    if (type == RecordFrame::Invalid) {
        broken_ = true;
        return false;
    }
    if (type != RecordFrame::Point) {
        if (!applyMarker(frame, metadata_, points_)) {
            broken_ = true;
            return false;
        }
        terminal_ = type == RecordFrame::Terminal;
        return true;
    }
    TestPoint point;
    if (points_ >= metadata_.plan.expectedPoints() || !decodeTestPoint(frame, point) ||
        (points_ ? (point.elapsedMs <= previousMs_ || point.elapsedMs / kTestIntervalMs != points_) : point.elapsedMs != 0)) {
        broken_ = true;
        return false;
    }
    previousMs_ = point.elapsedMs;
    ++points_;
    return true;
}

} // namespace klima
