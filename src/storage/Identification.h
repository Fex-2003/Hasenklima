#pragma once

#include <array>
#include <cmath>
#include <cstdint>

namespace klima {

constexpr uint32_t kNoTestTime = UINT32_MAX;
constexpr uint32_t kTestIntervalMs = 10000;
constexpr uint32_t kTestFlushMs = 30000;
constexpr uint32_t kMaximumTestPoints = 3730;

enum class TestPhase : uint8_t { Idle, Precheck, Baseline, Heating, Cooldown, Finishing, Complete, Aborted };
enum class TestReason : uint8_t {
    None, User, Reset, Io, Incomplete, InputLimit, HeatingDisabled, Safety, InvalidReadings, QueueFull, Deadline
};

const char* testPhaseCode(TestPhase phase);
const char* testReasonCode(TestReason reason);

struct IdentificationPlan {
    uint16_t baselineMinutes = 0;
    uint16_t heatMinutes = 0;
    uint16_t cooldownMinutes = 0;
    float pwmPercent = 12.5f;

    bool valid() const;
    uint32_t baselineMs() const { return 60000u * baselineMinutes; }
    uint32_t heatEndMs() const { return baselineMs() + 60000u * heatMinutes; }
    uint32_t totalMs() const { return heatEndMs() + 60000u * cooldownMinutes; }
    uint32_t expectedPoints() const { return totalMs() / kTestIntervalMs + 1; }
};

struct TestMetadata {
    IdentificationPlan plan;
    uint64_t testId = 0, bootId = 0, startUs = 0, startUtcSeconds = 0;
    uint32_t configRevision = 0;
    uint16_t activeMask = 0;
    std::array<char, 24> firmwareVersion{{'1', '.', '0', '.', '0', '\0'}};
    std::array<uint64_t, 9> plateRomIds{};
    float confirmedDuty = 0;
    float resistanceOhm = NAN, currentLimitA = NAN, plateLimitC = NAN;
    float vinGain = NAN, vinOffsetV = NAN, vinMinV = NAN, vinMaxV = NAN;
    float piKp = NAN, piTiS = NAN, heatingTargetC = NAN, coolingTargetC = NAN, supplyRatedCurrentA = NAN;
    uint32_t piIntervalMs = 0;
    bool piIntegralEnabled = false;
    TestPhase phase = TestPhase::Idle;
    TestReason reason = TestReason::None;
    uint32_t heatStartMs = kNoTestTime, heatStopMs = kNoTestTime, endMs = kNoTestTime;
    float trigger = NAN;
};

struct TestReadings {
    float plateMedianC = NAN, roomTempC = NAN, outsideTempC = NAN, vinV = NAN, plateMaxC = NAN;
    bool valid() const;
};

struct TestPoint {
    uint32_t elapsedMs = 0;
    float plateMedianC = NAN, roomTempC = NAN, outsideTempC = NAN, vinV = NAN;
    float pwmEffectivePercent = 0, plateMaxC = NAN;
    bool valid() const;
};

class Identification {
public:
    bool prepare(const TestMetadata& metadata);
    void begin(uint64_t nowUs);
    float advance(uint64_t nowUs, TestReason failure, float currentDutyLimit);
    bool capture(uint64_t nowUs, const TestReadings& readings, float effectiveDuty, TestPoint& point);
    void abort(uint64_t nowUs, TestReason reason, float trigger = NAN);
    void resolveFinish(bool success);
    bool takeMarker();
    bool active() const;
    bool finishPending() const { return meta_.phase == TestPhase::Finishing; }
    const TestMetadata& metadata() const { return meta_; }

private:
    uint32_t elapsedMs(uint64_t nowUs) const;
    TestMetadata meta_;
    uint32_t nextPointMs_ = 0;
    bool initialPoint_ = false;
    bool marker_ = false;
    bool finalPoint_ = false;
};

} // namespace klima
