#include "Control.h"

#include <algorithm>
#include <cmath>

namespace klima {
namespace {
using Code = FaultCode;
constexpr uint32_t plateFaults = faultBit(Code::PlateSensorInvalid) | faultBit(Code::PlateSensorStale) |
    faultBit(Code::PlateOvertemp) | faultBit(Code::NoActivePlateSensor);
constexpr uint32_t controllerFaults = faultBit(Code::ConfigInvalid) | faultBit(Code::ActuatorInitFailed) |
    faultBit(Code::ControlOverrun);
constexpr uint32_t vinFaults = faultBit(Code::VinInvalid) | faultBit(Code::VinOutOfRange);

bool temperatureInRange(float temperatureC) {
    return std::isfinite(temperatureC) && temperatureC >= minDsC && temperatureC <= maxDsC;
}
bool validHumidity(const Sample& sample, uint64_t nowUs, uint32_t ageMs) {
    return fresh(sample, nowUs, ageMs) && sample.value >= 0 && sample.value <= 100;
}

void invalidateAggregates(PlateStatistics& stats) {
    stats.medianC = stats.meanC = stats.minC = stats.maxC = stats.spreadK = unset;
    stats.complete = false;
}

template<typename T>
bool hysteresis(bool wasOn, T value, T low, T high, bool inclusiveStart) {
    if (wasOn) return value > low;
    return inclusiveStart ? value >= high : value > high;
}

bool requestedHeat(float duty) { return std::isfinite(duty) && duty > 0; }

const char* validateManual(const OutputRequest& request, const SafetyDecision& safety) {
    if (!std::isfinite(request.heaterDuty) || request.heaterDuty < 0 || request.heaterDuty > 1 ||
        request.driverDuty != 0) return "MANUAL_RANGE";
    if (request.heaterDuty > 0 && (request.cooling || request.evaporation)) return "OUTPUT_CONFLICT";
    if ((request.heaterDuty > 0 && !safety.heaterAllowed) ||
        (request.cooling && !safety.coolingAllowed) ||
        (request.evaporation && !safety.evaporationAllowed)) return "MANUAL_INHIBITED";
    if (request.heaterDuty > safety.heaterDutyLimit) return "MANUAL_PWM_LIMIT";
    if (request.heaterDuty > 0 && !quantizeHeaterDuty(request.heaterDuty)) return "MANUAL_PWM_TOO_LOW";
    if (request.cooling && !request.condensationOverride && !safety.condensationAllowed) return "CONDENSATION_INHIBIT";
    return nullptr;
}
}  // namespace

void PIHeatingController::reset(uint64_t nowUs, uint32_t plateSequence) {
    resetPlateSequence_ = plateSequence;
    lastCycleUs_ = nowUs;
    integralDuty_ = 0;
    errorK_ = 0;
    rawDuty_ = 0;
    requestedDuty_ = 0;
    pendingDeltaS_ = 0;
}

void PIHeatingController::pause(uint64_t nowUs) {
    lastCycleUs_ = nowUs;
    requestedDuty_ = 0;
    pendingDeltaS_ = 0;
}

float PIHeatingController::demand(const Config& cfg, float medianC, uint64_t nowUs, uint32_t plateSequence, bool& overrun) {
    overrun = false;
    pendingDeltaS_ = 0;
    if (!cfg.piIntegralEnabled) integralDuty_ = 0;
    if (nowUs < lastCycleUs_) {
        overrun = true;
        reset(nowUs, plateSequence);
        return 0;
    }
    if (plateSequence == resetPlateSequence_) {
        // A handoff may happen immediately before a newly ordered sensor sweep.
        // Its wait is not an interval in which the controller was integrating.
        if (nowUs - lastCycleUs_ > cfg.piUpdateIntervalMs * usPerMs) lastCycleUs_ = nowUs;
        return 0;
    }
    const uint64_t dtUs = nowUs - lastCycleUs_;
    const uint64_t scheduleUs = cfg.piUpdateIntervalMs * usPerMs;
    if (dtUs < scheduleUs) return requestedDuty_;
    if (dtUs >= 2 * scheduleUs) {
        overrun = true;
        reset(nowUs, plateSequence);
        return 0;
    }
    lastCycleUs_ = nowUs;
    errorK_ = cfg.heatingTargetC - medianC;
    rawDuty_ = cfg.piKpDutyPerK * errorK_ + integralDuty_;
    requestedDuty_ = std::clamp(rawDuty_, 0.0f, 1.0f);
    pendingDeltaS_ = static_cast<double>(dtUs) / usPerS;
    return requestedDuty_;
}

void PIHeatingController::integrate(const Config& cfg, float limitDuty) {
    if (!cfg.piIntegralEnabled || pendingDeltaS_ <= 0) return;
    // Current PWM is based on the old integral. Only apply the next term when
    // the error does not push the actuator further into a saturated region.
    if ((rawDuty_ < limitDuty || errorK_ <= 0) && (rawDuty_ > 0 || errorK_ >= 0)) {
        const auto step = static_cast<double>(cfg.piKpDutyPerK) / cfg.piTiS * errorK_ * pendingDeltaS_;
        integralDuty_ = static_cast<float>(std::clamp(integralDuty_ + step, -1.0, 1.0));
    }
    pendingDeltaS_ = 0;
}

void Controller::resetPi(uint64_t nowUs, uint32_t plateSequence) {
    pi_.reset(nowUs, plateSequence);
    coolingLatched_ = evaporationLatched_ = false;
}

ControlResult Controller::assess(const Config& cfg, const Inputs& in, uint64_t nowUs, float plateLimitC,
                                 const RuntimeFlags& flags) {
    ControlResult result;
    auto& safety = result.safety;
    const bool plateLimitValid = temperatureInRange(plateLimitC);
    const bool currentMapping = cfg.revision == in.configRevision;
    const bool currentPendingMapping = cfg.revision == in.platePendingRevision;
    const auto maxAgeMs = cfg.sensorMaxAgeMs;
    result.plate = plateStatistics(cfg, in, nowUs);
    if (!currentMapping) invalidateAggregates(result.plate);
    auto climateValue = [&](const Sample& sample) { return fresh(sample, nowUs, maxAgeMs) ? sample.value : unset; };
    const std::array<float, 7> climateValues{climateValue(in.room.temperature), climateValue(in.room.humidity),
        climateValue(in.outside.temperature), climateValue(in.outside.humidity), climateValue(in.outside.pressure),
        climateValue(in.stall.temperature), climateValue(in.stall.humidity)};
    if (!climateInitialized_ || !std::equal(climateValues.begin(), climateValues.end(), climateValues_.begin(),
            [](float a, float b) { return a == b || (std::isnan(a) && std::isnan(b)); })) {
        climateValues_ = climateValues;
        climate_ = climateCalculations(cfg, in, nowUs);
        climateInitialized_ = true;
    }
    result.climate = climate_;
    const bool tankValid = freshTemperature(in.tank, nowUs, maxAgeMs);
    const bool outsideValid = freshTemperature(in.outside.temperature, nowUs, maxAgeMs);
    const bool outsideHumidityValid = validHumidity(in.outside.humidity, nowUs, maxAgeMs);
    const bool roomValid = freshTemperature(in.room.temperature, nowUs, maxAgeMs);
    const bool roomHumidityValid = validHumidity(in.room.humidity, nowUs, maxAgeMs);
    const bool stallValid = freshTemperature(in.stall.temperature, nowUs, maxAgeMs);
    const bool stallHumidityValid = validHumidity(in.stall.humidity, nowUs, maxAgeMs);
    const bool vinValid = fresh(in.vin, nowUs, cfg.vinMaxAgeMs) && in.vin.value > 0;
    const bool voltageConfigured = vinConfigured(cfg);
    safety.vinInRange = vinValid && voltageConfigured &&
        in.vin.value >= cfg.vinValidMinV && in.vin.value <= cfg.vinValidMaxV;

    bool plateInvalid = false, plateStale = false, plateOvertemp = false, plateDisabled = false;
    for (std::size_t i = 0; i < cfg.plate.size(); ++i) {
        const auto& position = cfg.plate[i];
        if (!position.enabled) {
            plateDisabled |= position.romId != 0;
            continue;
        }
        const auto& sample = in.plate[i];
        const auto& pending = in.platePending[i];
        auto inspect = [&](const Sample& s) {
            if (!s.valid || !temperatureInRange(s.value) || s.sampleTimeUs > nowUs) plateInvalid = true;
            else if (!fresh(s, nowUs, maxAgeMs)) plateStale = true;
            else if (plateLimitValid && s.value >= plateLimitC) {
                if (!plateOvertemp || s.value > safety.overtempTriggerC) safety.overtempTriggerC = s.value;
                plateOvertemp = true;
            }
        };
        if (currentMapping && in.plateSequence) {
            if (sample.sequence != in.plateSequence || !position.romId) plateInvalid = true;
            else inspect(sample);
        }
        if (currentPendingMapping && pending.sequence && pending.sequence != in.plateSequence) inspect(pending);
    }
    if (plateInvalid || plateStale) invalidateAggregates(result.plate);
    if (currentMapping && in.plateSequence != previousPlateSequence_ && result.plate.complete) {
        plateChangeWarning_ = false;
        for (std::size_t i = 0; i < cfg.plate.size(); ++i) {
            const auto& old = previousPlate_[i];
            const auto& sample = in.plate[i];
            if (!cfg.plate[i].enabled || !std::isfinite(cfg.plateChangeWarningKPerS) ||
                !freshTemperature(old, nowUs, maxAgeMs) || sample.sampleTimeUs <= old.sampleTimeUs) continue;
            const double deltaS = static_cast<double>(sample.sampleTimeUs - old.sampleTimeUs) / usPerS;
            if (std::abs(sample.value - old.value) > cfg.plateChangeWarningKPerS * deltaS) plateChangeWarning_ = true;
        }
        previousPlate_ = in.plate;
        previousPlateSequence_ = in.plateSequence;
    }
    faults_.observe(Code::PlateSensorInvalid, plateInvalid, nowUs);
    faults_.observe(Code::PlateSensorStale, plateStale, nowUs);
    faults_.observe(Code::NoActivePlateSensor, result.plate.enabledCount == 0, nowUs);
    faults_.observe(Code::PlateOvertemp, plateOvertemp, nowUs);
    faults_.observe(Code::VinInvalid, in.vin.sequence != 0 && !vinValid, nowUs);
    faults_.observe(Code::VinOutOfRange, vinValid && voltageConfigured && !safety.vinInRange, nowUs);
    faults_.observe(Code::ConfigInvalid, flags.configInvalid, nowUs);
    faults_.observe(Code::ActuatorInitFailed, flags.actuatorInitFailed, nowUs);
    faults_.observe(Code::ControlOverrun, flags.controlOverrun, nowUs);
    faults_.observe(Code::RoomSensorInvalid, !roomValid || !roomHumidityValid, nowUs);
    faults_.observe(Code::OutsideTemperatureInvalid, !outsideValid, nowUs);
    faults_.observe(Code::OutsideHumidityInvalid, outsideValid && !outsideHumidityValid, nowUs);
    faults_.observe(Code::TankSensorInvalid, !tankValid, nowUs);
    faults_.observe(Code::StallSensorInvalid, !stallValid || !stallHumidityValid, nowUs);
    faults_.observe(Code::RecordIoError, flags.recordIoError, nowUs);
    faults_.observe(Code::RecordQueueFull, flags.recordQueueFull, nowUs);
    faults_.observe(Code::TestInputLimit, flags.testInputLimit, nowUs);
    faults_.observe(Code::WifiOffline, flags.wifiOffline, nowUs);
    faults_.observe(Code::MqttOffline, cfg.mqttEnabled && flags.mqttOffline, nowUs);
    faults_.observe(Code::WebOffline, flags.webOffline, nowUs);
    faults_.observe(Code::EnergyCheckpointError, flags.energyCheckpointError, nowUs);
    faults_.observe(Code::PlateSensorDisabled, plateDisabled, nowUs);
    faults_.observe(Code::PlateSpreadWarning,
                    result.plate.complete && result.plate.spreadK > cfg.plateSpreadWarningK, nowUs);
    faults_.observe(Code::PlateChangeWarning,
                    result.plate.complete && plateChangeWarning_ && std::isfinite(cfg.plateChangeWarningKPerS), nowUs);

    result.faults = faults_.summary();
    const auto blocking = result.faults.activeMask | result.faults.latchedMask;
    const bool critical = blocking & (faultBit(Code::PlateOvertemp) | controllerFaults);
    const bool outsideQualified = outsideValid && std::isfinite(flags.driverQualifiedMinC) &&
        in.outside.temperature.value >= flags.driverQualifiedMinC;
    safety.driverAllowed = cfg.driverHeaterEnabled && driverConfigured(cfg) && outsideValid && !critical;
    if (safety.driverAllowed) {
        const float differenceK = cfg.driverHeaterOnTempC - in.outside.temperature.value;
        result.requested.driverDuty = std::clamp(cfg.driverHeaterSlopePctPerK * differenceK / 100, 0.0f, 1.0f);
    }
    const bool driverEffective = quantizeDriverDuty(result.requested.driverDuty) > 0;
    if (driverEffective && !warmupStarted_) {
        warmupStarted_ = true;
        warmupStartUs_ = nowUs;
    } else if (!driverEffective) {
        warmupStarted_ = false;
    }
    const bool warmupNeeded = cfg.driverWarmupS && driverConfigured(cfg) && outsideValid &&
        in.outside.temperature.value < cfg.driverHeaterOnTempC;
    const auto warmupUs = cfg.driverWarmupS * usPerS;
    const auto warmingUs = warmupStarted_ && nowUs >= warmupStartUs_ ? nowUs - warmupStartUs_ : 0;
    if (warmupNeeded && warmingUs < warmupUs) {
        safety.warmupRemainingS = static_cast<float>(warmupUs - warmingUs) / usPerS;
    }
    const bool mainFault = blocking & (plateFaults | controllerFaults);
    const bool pumpSupplyOk = voltageConfigured && !(blocking & faultBit(Code::VinOutOfRange));
    safety.mainAllowed = !flags.forceMainOff && !mainFault && plateLimitValid && result.plate.complete;
    safety.heaterDutyLimit = heaterDutyLimit(cfg, in, nowUs);
    safety.condensationAllowed = result.plate.complete &&
        result.plate.minC > result.climate.controlDewpointC + cfg.dewpointMarginK;
    const bool evaporationInputsValid = tankValid && outsideValid && outsideHumidityValid &&
        std::isfinite(result.climate.wetbulbOutsideC);
    safety.heaterAllowed = safety.mainAllowed && cfg.heatingEnabled && heatingConfigured(cfg, plateLimitC) &&
        !(blocking & vinFaults) && outsideQualified && safety.warmupRemainingS == 0 &&
        safety.heaterDutyLimit > 0;
    safety.coolingAllowed = safety.mainAllowed && cfg.coolingEnabled && coolingConfigured(cfg, plateLimitC) && pumpSupplyOk;
    safety.evaporationAllowed = safety.mainAllowed && cfg.evaporationEnabled && evaporationConfigured(cfg, plateLimitC) &&
        pumpSupplyOk && evaporationInputsValid;

    if (!plateLimitValid) safety.inhibitReasons |= InhibitPlateLimitUnset;
    if (!result.plate.complete) safety.inhibitReasons |= InhibitPlateMissing;
    if (mainFault) safety.inhibitReasons |= InhibitFault;
    if (!safety.vinInRange || (blocking & vinFaults)) safety.inhibitReasons |= InhibitVin;
    if (!cfg.heatingEnabled) safety.inhibitReasons |= InhibitHeatDisabled;
    if (!cfg.coolingEnabled) safety.inhibitReasons |= InhibitCoolDisabled;
    if (!cfg.evaporationEnabled) safety.inhibitReasons |= InhibitEvapDisabled;
    if (!safety.condensationAllowed) safety.inhibitReasons |= InhibitCondensation;
    if (!outsideQualified) safety.inhibitReasons |= InhibitDriverAmbient;
    if (safety.warmupRemainingS > 0) safety.inhibitReasons |= InhibitWarmup;
    if (!evaporationInputsValid) safety.inhibitReasons |= InhibitEvapSensors;
    if (flags.forceMainOff) safety.inhibitReasons |= InhibitTransition;
    if (flags.configInvalid) safety.inhibitReasons |= InhibitConfig;
    if (cfg.heatingEnabled && safety.heaterDutyLimit <= 0) safety.inhibitReasons |= InhibitHeatingLimit;
    result.mode = mainFault || (blocking & faultBit(Code::VinOutOfRange)) ? OperatingMode::Fault : OperatingMode::Auto;
    return result;
}

ControlResult Controller::evaluate(const Config& cfg, const Inputs& in, uint64_t nowUs, float plateLimitC,
                                   const RuntimeFlags& flags, OperatingMode requestedMode, OutputRequest service) {
    const bool configurationChanged = !controlStarted_ || configRevision_ != cfg.revision;
    if (configurationChanged) {
        coolingLatched_ = evaporationLatched_ = false;
        previousPlateSequence_ = 0;
        previousPlate_ = {};
        plateChangeWarning_ = false;
        configRevision_ = cfg.revision;
    }
    auto state = assess(cfg, in, nowUs, plateLimitC, flags);
    if (state.mode != OperatingMode::Fault) {
        const bool waitingForFirstPlate = state.plate.enabledCount && !in.plateSequence;
        state.mode = waitingForFirstPlate ? OperatingMode::Initializing : requestedMode;
    }

    if (!controlStarted_ || (cfg.heatingEnabled && !heatingEnabled_) ||
        (state.mode == OperatingMode::Auto && previousMode_ != OperatingMode::Auto)) {
        resetPi(nowUs, in.plateSequence);
    }
    controlStarted_ = true;
    heatingEnabled_ = cfg.heatingEnabled;
    previousMode_ = state.mode;
    if (state.mode != OperatingMode::Auto || !state.safety.heaterAllowed) pi_.reset(nowUs, in.plateSequence);

    auto& want = state.requested;
    if (state.mode == OperatingMode::Auto) {
        const bool outsideOk = !freshTemperature(in.outside.temperature, nowUs, cfg.sensorMaxAgeMs) ||
            in.outside.temperature.value >= cfg.coolingMinOutsideC;
        const bool tankOk = !freshTemperature(in.tank, nowUs, cfg.sensorMaxAgeMs) ||
            in.tank.value <= state.plate.minC - cfg.tankPlateMarginK;
        if (!outsideOk) state.safety.inhibitReasons |= InhibitCoolingOutside;
        if (!tankOk) state.safety.inhibitReasons |= InhibitCoolingTank;
        coolingLatched_ = state.safety.coolingAllowed && state.safety.condensationAllowed && outsideOk && tankOk &&
            hysteresis(coolingLatched_, state.plate.maxC, cfg.coolingTargetC,
                       cfg.coolingTargetC + cfg.coolingHysteresisK, false);
        evaporationLatched_ = state.safety.evaporationAllowed &&
            hysteresis(evaporationLatched_, in.tank.value - state.climate.wetbulbOutsideC,
                       cfg.evaporationMinCapacityK - cfg.evaporationHysteresisK, cfg.evaporationMinCapacityK, true);
        want.cooling = coolingLatched_;
        want.evaporation = evaporationLatched_;
        if (want.cooling) pi_.pause(nowUs);
        else if (state.safety.heaterAllowed) {
            bool overrun = false;
            want.heaterDuty = pi_.demand(cfg, state.plate.medianC, nowUs, in.plateSequence, overrun);
            if (overrun) {
                auto stopped = flags;
                stopped.controlOverrun = true;
                state = assess(cfg, in, nowUs, plateLimitC, stopped);
                previousMode_ = state.mode;
                resetPi(nowUs, in.plateSequence);
                return state;
            }
        }
    } else {
        coolingLatched_ = evaporationLatched_ = false;
        if (state.mode == OperatingMode::Manual || state.mode == OperatingMode::Identification) {
            want.heaterDuty = service.heaterDuty;
            want.cooling = service.cooling;
            want.evaporation = service.evaporation;
            want.condensationOverride = state.mode == OperatingMode::Manual && service.condensationOverride;
        }
    }

    auto& output = state.authorized;
    output.driverDuty = want.driverDuty;
    if ((state.mode == OperatingMode::Manual && requestedHeat(want.heaterDuty) && (want.cooling || want.evaporation)) ||
        (state.mode == OperatingMode::Identification && (want.cooling || want.evaporation))) {
        state.safety.inhibitReasons |= InhibitInterlock;
        return state;
    }
    output.cooling = want.cooling && state.safety.coolingAllowed &&
        (state.safety.condensationAllowed || want.condensationOverride);
    output.evaporation = want.evaporation && state.safety.evaporationAllowed;
    if (!output.cooling && state.safety.heaterAllowed && requestedHeat(want.heaterDuty)) {
        if (want.heaterDuty > state.safety.heaterDutyLimit) state.safety.inhibitReasons |= InhibitHeatingLimit;
        if (state.mode == OperatingMode::Identification && want.heaterDuty > state.safety.heaterDutyLimit) return state;
        const auto bounded = std::min(want.heaterDuty, state.safety.heaterDutyLimit);
        if (quantizeHeaterDuty(bounded)) {
            output.heaterDuty = bounded;
            output.evaporation = false;
        }
    }
    if (state.mode == OperatingMode::Auto && state.safety.heaterAllowed && !output.cooling && !output.evaporation) {
        pi_.integrate(cfg, state.safety.heaterDutyLimit);
    }
    if (state.mode == OperatingMode::Auto) {
        if (output.heaterDuty > 0) state.autoState = AutoState::Heating;
        else if (output.cooling && output.evaporation) state.autoState = AutoState::CoolingAndEvaporating;
        else if (output.cooling) state.autoState = AutoState::Cooling;
        else if (output.evaporation) state.autoState = AutoState::Evaporating;
    }
    state.integralDuty = pi_.integralDuty();
    return state;
}

bool Controller::acknowledge(const Config& cfg, const Inputs& in, uint64_t nowUs, float plateLimitC,
                              const RuntimeFlags& flags, FaultCode code) {
    const auto state = assess(cfg, in, nowUs, plateLimitC, flags);
    bool acked = false;
    constexpr Code ackable[] = {Code::PlateSensorInvalid, Code::PlateSensorStale, Code::PlateOvertemp,
        Code::VinInvalid, Code::VinOutOfRange, Code::ConfigInvalid, Code::ActuatorInitFailed, Code::ControlOverrun};
    for (const auto fault : ackable) {
        if (code != Code::None && fault != code) continue;
        if ((faultBit(fault) & plateFaults) && !state.plate.complete) continue;
        if ((faultBit(fault) & vinFaults) && !state.safety.vinInRange) continue;
        if (fault == Code::PlateOvertemp) {
            const auto releaseC = plateLimitC - cfg.overtempReleaseHysteresisK;
            if (!(state.plate.maxC <= releaseC)) continue;
            bool pendingTooWarm = false;
            for (std::size_t i = 0; i < cfg.plate.size(); ++i) {
                const auto& pending = in.platePending[i];
                if (cfg.revision == in.platePendingRevision && cfg.plate[i].enabled &&
                    pending.sequence && pending.sequence != in.plateSequence &&
                    freshTemperature(pending, nowUs, cfg.sensorMaxAgeMs) && pending.value > releaseC) pendingTooWarm = true;
            }
            if (pendingTooWarm) continue;
        }
        acked |= faults_.acknowledge(fault, nowUs);
    }
    if (acked) resetPi(nowUs, in.plateSequence);
    return acked;
}

const char* ManualSession::start(uint64_t id, uint64_t nowUs, uint32_t durationS,
                                 const OutputRequest& request, const SafetyDecision& safety) {
    if (active()) return "MANUAL_ACTIVE";
    if (!id || durationS < 60 || durationS > 3600 || nowUs > UINT64_MAX - durationS * usPerS) return "MANUAL_DURATION";
    if (const auto* error = validateManual(request, safety)) return error;
    request_ = request;
    id_ = id;
    deadlineUs_ = nowUs + durationS * usPerS;
    return nullptr;
}

const char* ManualSession::update(uint64_t id, const OutputRequest& request, const SafetyDecision& safety,
                                  uint64_t nowUs, uint32_t durationS) {
    if (!active() || id != id_) return "MANUAL_ID";
    if (nowUs >= deadlineUs_) return "MANUAL_EXPIRED";
    if (durationS && (durationS < 60 || durationS > 3600 || nowUs > UINT64_MAX - durationS * usPerS)) {
        return "MANUAL_DURATION";
    }
    if (const auto* error = validateManual(request, safety)) return error;
    request_ = request;
    if (durationS) deadlineUs_ = nowUs + durationS * usPerS;
    return nullptr;
}

bool ManualSession::expire(uint64_t nowUs) {
    if (!active() || nowUs < deadlineUs_) return false;
    stop();
    return true;
}

void ManualSession::stop() {
    id_ = 0;
    deadlineUs_ = 0;
    request_ = {};
}
}  // namespace klima
