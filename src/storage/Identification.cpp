#include "Identification.h"

#include <algorithm>

namespace klima {

const char* testPhaseCode(TestPhase phase) {
    switch (phase) {
    case TestPhase::Idle: return "IDLE";
    case TestPhase::Precheck: return "PRECHECK";
    case TestPhase::Baseline: return "BASELINE";
    case TestPhase::Heating: return "HEATING";
    case TestPhase::Cooldown: return "COOLDOWN";
    case TestPhase::Finishing: return "FINISHING";
    case TestPhase::Complete: return "COMPLETE";
    case TestPhase::Aborted: return "ABORTED";
    }
    return "UNKNOWN";
}

const char* testReasonCode(TestReason reason) {
    switch (reason) {
    case TestReason::None: return "";
    case TestReason::User: return "ABORTED_USER";
    case TestReason::Reset: return "ABORTED_RESET";
    case TestReason::Io: return "ABORTED_IO";
    case TestReason::Incomplete: return "INCOMPLETE";
    case TestReason::InputLimit: return "TEST_INPUT_LIMIT";
    case TestReason::HeatingDisabled: return "HEATING_DISABLED";
    case TestReason::Safety: return "TEST_SAFETY";
    case TestReason::InvalidReadings: return "TEST_REQUIRED_READING_INVALID";
    case TestReason::QueueFull: return "RECORD_QUEUE_FULL";
    case TestReason::Deadline: return "RECORD_DEADLINE_MISSED";
    }
    return "UNKNOWN";
}

bool IdentificationPlan::valid() const {
    return baselineMinutes <= 60 && heatMinutes >= 1 && heatMinutes <= 500 && cooldownMinutes <= 60 &&
           std::isfinite(pwmPercent) && pwmPercent > 0 && pwmPercent <= 100;
}

bool TestReadings::valid() const {
    return std::isfinite(plateMedianC) && std::isfinite(roomTempC) && std::isfinite(outsideTempC) &&
           std::isfinite(vinV) && vinV > 0 && std::isfinite(plateMaxC) && plateMaxC >= plateMedianC;
}

bool TestPoint::valid() const {
    return elapsedMs < 620u * 60000u + kTestIntervalMs && std::isfinite(pwmEffectivePercent) &&
           pwmEffectivePercent >= 0 && pwmEffectivePercent <= 100 &&
           TestReadings{plateMedianC, roomTempC, outsideTempC, vinV, plateMaxC}.valid();
}

bool Identification::active() const {
    return meta_.phase >= TestPhase::Precheck && meta_.phase <= TestPhase::Finishing;
}

bool Identification::prepare(const TestMetadata& metadata) {
    if (active() || !metadata.plan.valid() || !std::isfinite(metadata.confirmedDuty) ||
        metadata.confirmedDuty <= 0 || metadata.confirmedDuty > 1 ||
        metadata.confirmedDuty * 100.0f > metadata.plan.pwmPercent) return false;
    meta_ = metadata;
    meta_.phase = TestPhase::Precheck;
    meta_.reason = TestReason::None;
    meta_.heatStartMs = meta_.heatStopMs = meta_.endMs = kNoTestTime;
    meta_.trigger = NAN;
    nextPointMs_ = 0;
    initialPoint_ = finalPoint_ = marker_ = false;
    return true;
}

void Identification::begin(uint64_t nowUs) {
    if (meta_.phase != TestPhase::Precheck) return;
    if (meta_.startUtcSeconds && nowUs >= meta_.startUs) {
        meta_.startUtcSeconds += (nowUs - meta_.startUs) / 1000000;
    }
    meta_.startUs = nowUs;
    meta_.phase = TestPhase::Baseline;
    marker_ = true;
}

uint32_t Identification::elapsedMs(uint64_t nowUs) const {
    return static_cast<uint32_t>(std::min<uint64_t>(
        nowUs >= meta_.startUs ? (nowUs - meta_.startUs) / 1000 : 0, kNoTestTime - 1));
}

float Identification::advance(uint64_t nowUs, TestReason failure, float currentDutyLimit) {
    if (meta_.phase < TestPhase::Baseline || meta_.phase > TestPhase::Cooldown) return 0;
    const uint32_t elapsed = elapsedMs(nowUs);
    if (failure != TestReason::None) {
        abort(nowUs, failure);
    } else if (!std::isfinite(currentDutyLimit) || meta_.confirmedDuty > currentDutyLimit) {
        abort(nowUs, TestReason::InputLimit, currentDutyLimit);
    } else if (nowUs < meta_.startUs || elapsed >= nextPointMs_ + kTestIntervalMs) {
        abort(nowUs, TestReason::Deadline);
    } else if (!initialPoint_) {
        // The t=0 row is captured with the output at zero even when baseline=0.
        return 0;
    } else if (elapsed >= meta_.plan.totalMs()) {
        meta_.endMs = elapsed;
        meta_.phase = TestPhase::Finishing;
        finalPoint_ = true;
    } else if (elapsed >= meta_.plan.heatEndMs()) {
        meta_.phase = TestPhase::Cooldown;
    } else if (elapsed >= meta_.plan.baselineMs()) {
        meta_.phase = TestPhase::Heating;
        return meta_.confirmedDuty;
    }
    return 0;
}

bool Identification::capture(uint64_t nowUs, const TestReadings& readings, float effectiveDuty, TestPoint& point) {
    if (meta_.phase < TestPhase::Baseline || meta_.phase > TestPhase::Finishing ||
        (meta_.phase == TestPhase::Finishing && !finalPoint_)) return false;
    const uint32_t elapsed = elapsedMs(nowUs);
    if (!readings.valid() || (std::isfinite(meta_.plateLimitC) && readings.plateMaxC >= meta_.plateLimitC)) {
        abort(nowUs, TestReason::InvalidReadings, readings.plateMaxC);
        return false;
    }
    const bool heating = meta_.phase == TestPhase::Heating;
    if (effectiveDuty != (heating ? meta_.confirmedDuty : 0)) {
        abort(nowUs, TestReason::Safety);
        return false;
    }
    // Mark the applied output change, separately from its ideal scheduled time.
    if (heating && meta_.heatStartMs == kNoTestTime) {
        meta_.heatStartMs = elapsed;
        marker_ = true;
    } else if (!heating && meta_.heatStartMs != kNoTestTime && meta_.heatStopMs == kNoTestTime) {
        meta_.heatStopMs = elapsed;
        marker_ = true;
    }
    if (!finalPoint_ && elapsed < nextPointMs_) return false;
    point = {initialPoint_ ? elapsed : 0, readings.plateMedianC, readings.roomTempC, readings.outsideTempC,
             readings.vinV, effectiveDuty * 100.0f, readings.plateMaxC};
    initialPoint_ = true;
    finalPoint_ = false;
    nextPointMs_ += kTestIntervalMs;
    return true;
}

void Identification::abort(uint64_t nowUs, TestReason reason, float trigger) {
    if (!active() || reason == TestReason::None ||
        (meta_.phase == TestPhase::Finishing && meta_.reason != TestReason::None)) return;
    meta_.endMs = elapsedMs(nowUs);
    if (meta_.heatStartMs != kNoTestTime && meta_.heatStopMs == kNoTestTime) meta_.heatStopMs = meta_.endMs;
    meta_.reason = reason;
    meta_.trigger = trigger;
    meta_.phase = TestPhase::Finishing;
    finalPoint_ = false;
}

void Identification::resolveFinish(bool success) {
    if (!finishPending()) return;
    if (!success) meta_.reason = TestReason::Io;
    meta_.phase = meta_.reason == TestReason::None ? TestPhase::Complete : TestPhase::Aborted;
}

bool Identification::takeMarker() {
    const bool changed = marker_;
    marker_ = false;
    return changed;
}

} // namespace klima
