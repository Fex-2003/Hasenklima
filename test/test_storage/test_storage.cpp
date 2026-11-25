#include "storage/Identification.h"
#include "storage/PersistenceFormat.h"
#include "storage/RecordFormat.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace klima;

namespace {
void check(bool okay, const char* expression, int line) {
    if (okay) return;
    std::fprintf(stderr, "FAIL line %d: %s\n", line, expression);
    std::exit(1);
}
#define CHECK(expression) check((expression), #expression, __LINE__)

constexpr uint64_t startUs = 21000000;
constexpr TestReadings readings{23.5f, 23, 19, 24, 24};

TestMetadata metadata(IdentificationPlan plan) {
    TestMetadata meta;
    meta.plan = plan;
    meta.testId = 112233445566ULL;
    meta.bootId = 998877665544ULL;
    meta.startUs = startUs;
    meta.startUtcSeconds = 1795600000;
    meta.configRevision = 7;
    meta.activeMask = 1;
    meta.plateRomIds[0] = 0x1234567890123428ULL;
    meta.confirmedDuty = .125f;
    meta.resistanceOhm = 4;
    meta.currentLimitA = 1.5f;
    meta.plateLimitC = 31;
    return meta;
}

void maxDurationAndPhaseEdges() {
    auto meta = metadata({60, 500, 60, 12.5f});
    CHECK(meta.plan.totalMs() == 37200000);
    CHECK(meta.plan.expectedPoints() == 3721);
    CHECK((!IdentificationPlan{0, 0, 0, 12.5f}.valid()));
    CHECK((!IdentificationPlan{61, 1, 0, 12.5f}.valid()));
    CHECK((!IdentificationPlan{0, 501, 0, 12.5f}.valid()));
    CHECK((!IdentificationPlan{0, 1, 61, 12.5f}.valid()));
    CHECK((!IdentificationPlan{0, 1, 0, NAN}.valid()));
    CHECK((IdentificationPlan{0, 1, 0, 12.5f}.valid()));
    Identification test;
    CHECK(test.prepare(meta));
    CHECK(!test.prepare(meta));
    test.begin(startUs);
    TestMetadata recovered;
    CHECK(decodeTestHeader(encodeTestHeader(meta), recovered));
    uint32_t points = 0;
    for (uint32_t elapsed = 0; elapsed <= meta.plan.totalMs(); elapsed += kTestIntervalMs) {
        const auto now = startUs + elapsed * 1000ULL;
        const bool heat = elapsed >= meta.plan.baselineMs() && elapsed < meta.plan.heatEndMs();
        const float duty = test.advance(now, TestReason::None, .25f);
        CHECK(duty == (heat ? .125f : 0));
        TestPoint point;
        CHECK(test.capture(now, readings, duty, point));
        CHECK(point.elapsedMs == elapsed);
        CHECK(point.pwmEffectivePercent == (heat ? 12.5f : 0));
        ++points;
        if (test.takeMarker()) {
            const auto& step = test.metadata();
            const auto marker = step.phase == TestPhase::Baseline ? encodeStartMarker(step) : encodePhaseMarker(step, points, false);
            CHECK(applyMarker(marker, recovered, points));
        }
    }
    CHECK(points == 3721);
    CHECK(test.metadata().heatStartMs == 3600000);
    CHECK(test.metadata().heatStopMs == 33600000);
    CHECK(test.metadata().endMs == 37200000);
    CHECK(test.finishPending());
    auto footer = encodePhaseMarker(test.metadata(), points, true);
    CHECK(!applyMarker(footer, recovered, points - 1));
    CHECK(applyMarker(footer, recovered, points));
    CHECK(recovered.phase == TestPhase::Complete);
    test.resolveFinish(true);
    CHECK(!test.active());
    CHECK(test.metadata().phase == TestPhase::Complete);
}

void startWithNoBaselineAndAbort() {
    Identification test;
    CHECK(test.prepare(metadata({0, 1, 0, 12.5f})));
    test.begin(startUs);
    TestPoint point;
    CHECK(test.advance(startUs, TestReason::None, .25f) == 0);
    CHECK(test.capture(startUs, readings, 0, point));
    CHECK(point.elapsedMs == 0);
    CHECK(point.pwmEffectivePercent == 0);
    CHECK(test.takeMarker());
    CHECK(test.advance(startUs + 10000, TestReason::None, .25f) == .125f);
    CHECK(!test.capture(startUs + 10000, readings, .125f, point));
    CHECK(test.metadata().heatStartMs == 10);
    CHECK(test.takeMarker());
    CHECK(test.advance(startUs + 3000000, TestReason::None, .124f) == 0);
    CHECK(test.metadata().reason == TestReason::InputLimit);
    CHECK(test.metadata().heatStopMs == 3000);
    CHECK(!test.capture(startUs + 3000000, readings, 0, point));
    test.resolveFinish(true);
    CHECK(test.metadata().phase == TestPhase::Aborted);

    CHECK(test.prepare(metadata({2, 1, 0, 12.5f})));
    test.begin(startUs);
    CHECK(test.capture(startUs, readings, 0, point));
    CHECK(test.advance(startUs + 5000000, TestReason::InvalidReadings, .25f) == 0);
    CHECK(test.metadata().reason == TestReason::InvalidReadings);
    CHECK(!test.capture(startUs + 5000000, readings, 0, point));
    test.resolveFinish(true);
    CHECK(test.metadata().phase == TestPhase::Aborted);

    CHECK(test.prepare(metadata({1, 1, 0, 12.5f})));
    test.begin(startUs);
    CHECK(test.capture(startUs, readings, 0, point));
    CHECK(test.advance(startUs + 20000000, TestReason::None, .25f) == 0);
    CHECK(test.metadata().reason == TestReason::Deadline);
    test.resolveFinish(false);
    CHECK(test.metadata().reason == TestReason::Io);
}

void binaryIntegrityAndCsv() {
    const uint8_t vector[] = {'1','2','3','4','5','6','7','8','9'};
    CHECK(crc32(vector, sizeof(vector)) == 0xcbf43926u);
    const TestPoint source{10234, -0.25f, 0.5f, -10.f, 23.99f, 12.5f, 1.f};
    auto bytes = encodeTestPoint(source);
    CHECK(bytes.size() == 32);
    CHECK(bytes[0] == 0xfa && bytes[1] == 0x27 && bytes[2] == 0 && bytes[3] == 0);
    TestPoint point;
    CHECK(decodeTestPoint(bytes, point));
    CHECK(point.elapsedMs == source.elapsedMs && point.outsideTempC == source.outsideTempC);
    CHECK(point.pwmEffectivePercent == source.pwmEffectivePercent && point.plateMaxC == source.plateMaxC);
    bytes[25] ^= 1;
    CHECK(!decodeTestPoint(bytes, point));
    char csv[256];
    CHECK(formatCsvPoint(csv, sizeof(csv), source) > 0);
    CHECK(std::strcmp(csv, "10.234,-0.250000,0.500000,-10.000000,23.990000,12.50000000,1.000000\n") == 0);
    CHECK(formatCsvPoint(csv, 4, source) < 0);
    point.roomTempC = NAN;
    CHECK(formatCsvPoint(csv, sizeof(csv), point) < 0);
    CHECK(std::strcmp(kTestCsvColumns,
          "time_s,plate_median_c,room_temp_c,outside_temp_c,vin_v,pwm_effective_percent,plate_max_c\n") == 0);

    auto meta = metadata({0, 1, 1, 12.5f});
    auto header = encodeTestHeader(meta);
    TestMetadata result;
    CHECK(decodeTestHeader(header, result));
    CHECK(result.phase == TestPhase::Precheck && result.plateRomIds[0] == meta.plateRomIds[0]);
    CHECK(result.configRevision == 7 && result.startUs == startUs);
    TestPrefix prefix(result);
    TestPoint first = source;
    first.elapsedMs = 0;
    first.pwmEffectivePercent = 0;
    CHECK(prefix.append(encodeTestPoint(first)));
    CHECK(prefix.append(encodeStartMarker(meta)));
    CHECK(prefix.append(encodeTestPoint(source)));
    TestPoint third = source;
    third.elapsedMs = 20317;
    auto broken = encodeTestPoint(third);
    broken[31] ^= 1;
    CHECK(!prefix.append(broken));
    CHECK(!prefix.append(encodeTestPoint(third)));
    CHECK(prefix.points() == 2 && !prefix.terminal());
    header.back() ^= 1;
    CHECK(!decodeTestHeader(header, result));
    result.phase = TestPhase::Precheck;
    CHECK(applyMarker(encodeStartMarker(meta), result, 0));
    CHECK(result.phase == TestPhase::Baseline);
    meta.phase = TestPhase::Aborted;
    meta.reason = TestReason::Reset;
    CHECK(applyMarker(encodePhaseMarker(meta, 1, true), result, 1));
    CHECK(result.phase == TestPhase::Aborted && result.reason == TestReason::Reset);
}

void persistenceFormat() {
    Config config;
    config.revision = 17;
    config.vinValidMinV = 22;
    config.vinValidMaxV = 25.5f;
    std::memcpy(config.timezone, "UTC", 4);
    ConfigBlob data;
    const auto length = encodeConfig(config, data);
    CHECK(length > 0 && length < data.size());
    Config restored;
    CHECK(decodeConfig(data.data(), length, NAN, restored));
    CHECK(restored.revision == config.revision && restored.vinValidMaxV == config.vinValidMaxV);
    CHECK(std::strcmp(restored.timezone, "UTC") == 0 && std::isnan(restored.heatingTargetC));
    CHECK(!decodeConfig(data.data(), length - 1, NAN, restored));
    data[17] ^= 1;
    CHECK(!decodeConfig(data.data(), length, NAN, restored));
    config.vinValidMinV = 26;
    const auto invalidLength = encodeConfig(config, data);
    CHECK(!decodeConfig(data.data(), invalidLength, NAN, restored));

    const EnergyCheckpoint saved{42000001, 998877665544ULL, 17};
    auto energy = encodeEnergy(saved);
    EnergyCheckpoint output;
    CHECK(decodeEnergy(energy, output));
    CHECK(output.installationId == saved.installationId && output.microWh == saved.microWh && output.sequence == saved.sequence);
    energy.back() ^= 1;
    CHECK(!decodeEnergy(energy, output));
}
} // namespace

int main() {
    maxDurationAndPhaseEdges();
    startWithNoBaselineAndAbort();
    binaryIntegrityAndCsv();
    persistenceFormat();
    std::puts("PASS storage: A19/A20 phase and 3721 points, A21/A22 abort, A24 CRC/reset, A30 config, A31 energy checkpoint");
    return 0;
}
