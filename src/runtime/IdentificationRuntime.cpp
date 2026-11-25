#include "Runtime.h"
#include "HardwareLimits.h"

namespace klima {
TestReadings Runtime::testReadings(uint64_t now) const {
    TestReadings readings;
    if (result_.plate.complete && inputs_.configRevision == config_.revision) {
        readings.plateMedianC = result_.plate.medianC;
        readings.plateMaxC = result_.plate.maxC;
    }
    if (freshTemperature(inputs_.room.temperature, now, config_.sensorMaxAgeMs)) {
        readings.roomTempC = inputs_.room.temperature.value;
    }
    if (freshTemperature(inputs_.outside.temperature, now, config_.sensorMaxAgeMs)) {
        readings.outsideTempC = inputs_.outside.temperature.value;
    }
    if (fresh(inputs_.vin, now, config_.vinMaxAgeMs)) readings.vinV = inputs_.vin.value;
    return readings;
}

void Runtime::processIdentification(uint64_t now) {
    if (testArming_) {
        bool saved = false;
        if (recorder_.pollStart(saved)) {
            testArming_ = false;
            if (saved) {
                flags_.recordIoError = recorder_.status().ioError;
                identification_.begin(now);
                finish(testOperation_, 200, "OK", "Sprungantwort gestartet");
            } else {
                identification_.abort(now, TestReason::Io);
                identification_.resolveFinish(false);
                const bool capacity = recorder_.status().startFailure == RecorderStartFailure::Capacity;
                finish(testOperation_, 503, capacity ? "RECORD_CAPACITY" : "RECORD_IO",
                       capacity ? "Nicht genügend freier LittleFS-Speicher" : "Testdatei konnte nicht geschrieben werden");
                endService(now);
            }
            testOperation_ = 0;
        }
    }
    if (testFinishing_) {
        bool saved = false;
        if (recorder_.pollFinish(saved)) {
            testFinishing_ = false;
            identification_.resolveFinish(saved);
            endService(now);
            requestCheckpoint(now, true);
        }
    }
    const auto mode = requestedMode();
    result_ = controller_.evaluate(config_, inputs_, now, MAX_PLATE_TEMP_C, flags_, mode);
    if (testArming_ || testFinishing_ || !identification_.active()) return;
    TestReason failure = TestReason::None;
    if (!config_.heatingEnabled) failure = TestReason::HeatingDisabled;
    else if (flags_.recordIoError) failure = TestReason::Io;
    else if (result_.faults.activeMask & faultBit(FaultCode::PlateOvertemp)) {
        identification_.abort(now, TestReason::Safety, result_.safety.overtempTriggerC);
    } else if (!testReadings(now).valid()) failure = TestReason::InvalidReadings;
    else if (!result_.safety.heaterAllowed) failure = TestReason::Safety;
    const float duty = identification_.advance(now, failure, result_.safety.heaterDutyLimit);
    flags_.testInputLimit = identification_.metadata().reason == TestReason::InputLimit;
    if (duty > 0) {
        result_ = controller_.evaluate(config_, inputs_, now, MAX_PLATE_TEMP_C, flags_,
                                       OperatingMode::Identification, {duty});
        result_.requested.heaterDuty = identification_.metadata().plan.pwmPercent / 100.0f;
    }
}

void Runtime::captureIdentification(uint64_t now) {
    if (testArming_ || testFinishing_) return;
    TestPoint point;
    if (identification_.capture(now, testReadings(now), outputs_.heaterDuty, point)) {
        if (!recorder_.record(point)) {
            mainOutputsOff(7, now);
            identification_.abort(now, TestReason::QueueFull);
            flags_.recordQueueFull = true;
        } else if (serialLive_ && xQueueSend(serialPoints_, &point, 0) != pdPASS) {
            ++serialPointsDropped_;
        }
    }
    const auto& metadata = identification_.metadata();
    const bool marker = identification_.takeMarker();
    if (marker && !identification_.finishPending() && !recorder_.marker(metadata)) {
        mainOutputsOff(7, now);
        identification_.abort(now, TestReason::QueueFull);
        flags_.recordQueueFull = true;
    }
    if (identification_.finishPending()) {
        mainOutputsOff(7, now);
        testFinishing_ = true;
        recorder_.requestFinish(metadata);
    }
}
}
