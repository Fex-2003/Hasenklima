#include "Runtime.h"
#include "HardwareLimits.h"
#include <Arduino.h>
#include <algorithm>
#include <cmath>
#include <ctime>

namespace klima {
namespace {
NetSample netSample(const Sample& sample, uint64_t now, uint32_t maxAgeMs) {
    const bool valid = fresh(sample, now, maxAgeMs);
    return {valid ? sample.value : NAN, sample.sampleTimeUs, sample.sequence, sample.errorCode,
            valid, sample.valid && !valid};
}
NetClimate netClimate(const ClimateSamples& climate, float dewpoint, uint64_t now, uint32_t ageMs) {
    const bool valid = std::isfinite(dewpoint);
    return {netSample(climate.temperature, now, ageMs), netSample(climate.humidity, now, ageMs),
            netSample(climate.pressure, now, ageMs),
            {dewpoint, std::min(climate.temperature.sampleTimeUs, climate.humidity.sampleTimeUs),
             climate.temperature.sequence, 0, valid, false}};
}
const char* severityName(Severity severity) {
    switch (severity) {
    case Severity::Info: return "INFO";
    case Severity::Warning: return "WARNING";
    case Severity::Error: return "ERROR";
    case Severity::Critical: return "CRITICAL";
    }
    return "UNKNOWN";
}
const char* eventName(FaultChange change) {
    switch (change) {
    case FaultChange::Raised: return "Aufgetreten";
    case FaultChange::Cleared: return "Ursache behoben";
    case FaultChange::Acknowledged: return "Quittiert";
    }
    return "";
}
AutoState actualState(const OutputState& out) {
    if (out.heaterDuty > 0) return AutoState::Heating;
    if (out.cooling && out.evaporation) return AutoState::CoolingAndEvaporating;
    if (out.cooling) return AutoState::Cooling;
    if (out.evaporation) return AutoState::Evaporating;
    return AutoState::Neutral;
}
}

void Runtime::applyOutputs(OutputRequest permitted, uint64_t now) {
    const auto old = outputs_;
    outputs_ = actuators_.apply(permitted, now);
    const auto power = estimatePower(config_, inputs_, now, outputs_.heaterDuty);
    energy_.update(now, power.powerW, outputs_.heaterDuty > 0);
    requestCheckpoint(now, old.heaterDuty > 0 && outputs_.heaterDuty == 0);
}

void Runtime::mainOutputsOff(uint8_t mask, uint64_t now) {
    OutputRequest permitted{outputs_.heaterDuty, outputs_.driverDuty, outputs_.cooling, outputs_.evaporation};
    if (mask & 1) permitted.heaterDuty = 0;
    if (mask & 2) permitted.cooling = false;
    if (mask & 4) permitted.evaporation = false;
    applyOutputs(permitted, now);
}

void Runtime::requestCheckpoint(uint64_t now, bool force) {
    if (energyResetPending_ || energy_.microWh() == persistedEnergy_) return;
    if (!force && now - lastCheckpointAttemptUs_ < config_.energyCheckpointS * usPerS) return;
    const EnergyRequest request{energy_.microWh(), ++energyGeneration_};
    xQueueOverwrite(energyRequests_, &request);
    lastCheckpointAttemptUs_ = now;
}

void Runtime::collectSnapshot(uint64_t now) {
    auto& s = snapshot_;
    const auto& plate = result_.plate;
    const auto& climate = result_.climate;
    const auto& decision = result_.safety;
    const auto& faults = result_.faults;
    const auto power = estimatePower(config_, inputs_, now, outputs_.heaterDuty);
    s.timestampUs = now;
    ++s.sequence;
    s.bootId = bootId_;
    const auto utc = time(nullptr);
    s.utcSeconds = utc >= 1704067200 ? utc : 0;
    s.revision = config_.revision;
    for (unsigned i = 0; i < s.plate.size(); ++i) {
        const auto& pending = inputs_.platePending[i];
        const bool newPending = pending.sequence && inputs_.platePendingRevision == config_.revision;
        const auto sample = newPending ? pending : inputs_.configRevision == config_.revision ? inputs_.plate[i] : Sample{};
        s.plate[i] = netSample(sample, now, config_.sensorMaxAgeMs);
        s.plateRom[i] = config_.plate[i].romId;
        if (sample.sequence && now >= sample.sampleTimeUs) {
            maxSensorAge_ = std::max<uint64_t>(maxSensorAge_, std::min<uint64_t>(UINT32_MAX, now - sample.sampleTimeUs));
        }
    }
    s.tank = netSample(inputs_.configRevision == config_.revision ? inputs_.tank : Sample{},
                      now, config_.sensorMaxAgeMs);
    s.vin = netSample(inputs_.vin, now, config_.vinMaxAgeMs);
    s.room = netClimate(inputs_.room, climate.roomDewpointC, now, config_.sensorMaxAgeMs);
    s.stall = netClimate(inputs_.stall, climate.stallDewpointC, now, config_.sensorMaxAgeMs);
    s.outside = netClimate(inputs_.outside, climate.outsideDewpointC, now, config_.sensorMaxAgeMs);
    s.vinCalibrated = s.vin.valid && config_.vinCalibrationConfirmed;
    s.enabledCount = plate.enabledCount;
    s.enabledMask = plate.enabledMask;
    s.plateComplete = plate.complete;
    s.medianC = plate.medianC; s.meanC = plate.meanC;
    s.minC = plate.minC; s.maxC = plate.maxC; s.spreadK = plate.spreadK;
    s.dewPointControlC = climate.controlDewpointC;
    s.dewPointSource = dewpointSourceName(climate.dewpointSource);
    s.pressureFallback = climate.pressureFallback;
    s.wetBulbOutsideC = climate.wetbulbOutsideC;
    s.evaporationCapacityK = s.tank.valid ? s.tank.value - s.wetBulbOutsideC : NAN;
    s.heatingTargetC = config_.heatingTargetC;
    s.coolingTargetC = config_.coolingTargetC;
    s.heatingEnabled = config_.heatingEnabled;
    s.coolingEnabled = config_.coolingEnabled;
    s.evaporationEnabled = config_.evaporationEnabled;
    s.driverEnabled = config_.driverHeaterEnabled;
    s.coolingCommanded = outputs_.cooling;
    s.evaporationCommanded = outputs_.evaporation;
    s.driverCurveValid = driverConfigured(config_);
    s.driverWarming = decision.warmupRemainingS > 0;
    s.dutyRequested = result_.requested.heaterDuty;
    s.dutyLimit = decision.heaterDutyLimit;
    s.dutyEffective = outputs_.heaterDuty;
    s.driverDuty = outputs_.driverDuty;
    s.heaterCurrentA = power.averageCurrentA;
    s.heaterPowerW = power.powerW;
    s.powerLimitW = power.limitPowerW;
    s.reserveA = config_.supplyRatedCurrentA - config_.heaterInputCurrentLimitA;
    s.piErrorK = config_.heatingTargetC - plate.medianC;
    s.piIntegrator = result_.integralDuty;
    s.inhibitReasons = decision.inhibitReasons;
    s.energyUWh = energy_.microWh();
    s.energyCheckpointUs = lastCheckpointUs_;
    s.energyIncomplete = energy_.incomplete();
    s.energyCheckpointError = energyIo_;
    s.mode = modeName(result_.mode);
    s.autoMode = autoStateName(actualState(outputs_));
    s.manualSessionId = manual_.id();
    s.manualRemainingS = manual_.active() && manual_.deadlineUs() > now
        ? (manual_.deadlineUs() - now + usPerS - 1) / usPerS : 0;
    s.manualDewPointOverride = manual_.active() && manual_.request().condensationOverride;
    s.errorFlag = faults.errorFlag;
    s.warningFlag = faults.warningFlag;
    s.errorText[0] = 0;
    if (faults.errorFlag) {
        if (faults.errorCount > 1) snprintf(s.errorText, sizeof(s.errorText), "%s (%u Fehler)",
                                            faultText(faults.topCode), faults.errorCount);
        else snprintf(s.errorText, sizeof(s.errorText), "%s", faultText(faults.topCode));
    }
    s.faultCount = 0;
    for (unsigned i = 1; i < static_cast<unsigned>(FaultCode::Count); ++i) {
        const auto code = static_cast<FaultCode>(i);
        const auto& f = controller_.faults().status(code);
        if (f.active || f.latched) {
            s.faults[s.faultCount++] = {faultCodeName(code), faultText(code), severityName(f.severity),
                                      f.firstTimeUs, f.lastTimeUs, f.count, f.inhibitReasons,
                                      f.active, f.latched};
        }
    }
    s.recorder = recorder_.status();
    if (identification_.active() && !testArming_) s.recorder.metadata = identification_.metadata();
    const auto& meta = s.recorder.metadata;
    const bool live = (s.recorder.running || testFinishing_) && meta.startUs && now >= meta.startUs;
    s.testElapsedS = live ? (now - meta.startUs) / usPerS : meta.endMs != kNoTestTime ? meta.endMs / 1000 : 0;
    const uint32_t totalS = meta.plan.totalMs() / 1000;
    s.testRemainingS = live && totalS >= s.testElapsedS ? totalS - s.testElapsedS : 0;
}

void Runtime::publish(uint64_t now) {
    nextPublishUs_ = now + 200000;
    collectSnapshot(now);
    xSemaphoreTake(shared_, portMAX_DELAY);
    visibleConfig_ = config_;
    visibleSnapshot_ = snapshot_;
    const auto& faults = controller_.faults();
    faultHistoryCount_ = faults.size();
    for (size_t i = 0; i < faultHistoryCount_; ++i) {
        const auto& event = faults.at(i);
        const auto& status = faults.status(event.code);
        faultHistory_[i] = {faultCodeName(event.code), eventName(event.change),
                            severityName(event.severity), event.timeUs, event.timeUs, event.count,
                            status.inhibitReasons, event.change == FaultChange::Raised,
                            event.change == FaultChange::Raised && status.latched};
    }
    if (inputs_.room.temperature.sequence || inputs_.plateSequence) history_.update(now, config_, inputs_, result_.plate);
    xSemaphoreGive(shared_);
}

Config Runtime::config() {
    xSemaphoreTake(shared_, portMAX_DELAY);
    const auto value = visibleConfig_;
    xSemaphoreGive(shared_);
    return value;
}

void Runtime::snapshot(NetworkSnapshot& out) {
    xSemaphoreTake(shared_, portMAX_DELAY);
    out = visibleSnapshot_;
    xSemaphoreGive(shared_);
}

size_t Runtime::history(NetHistoryPoint* out, size_t cap) {
    xSemaphoreTake(shared_, portMAX_DELAY);
    const auto count = std::min(cap, history_.size());
    for (size_t i = 0; i < count; ++i) {
        const auto& point = history_.at(history_.size() - count + i);
        const auto& t = point.temperatureCenti;
        const auto mask = point.validMask;
        out[i] = {point.timeUs, {t[0], t[1], t[2], t[4], t[3]},
                  static_cast<uint8_t>((mask & 7) | ((mask & 8) << 1) | ((mask & 16) >> 1))};
    }
    xSemaphoreGive(shared_);
    return count;
}

size_t Runtime::faultHistory(NetFault* out, size_t cap) {
    xSemaphoreTake(shared_, portMAX_DELAY);
    const auto count = std::min(cap, faultHistoryCount_);
    std::copy_n(faultHistory_.data() + faultHistoryCount_ - count, count, out);
    xSemaphoreGive(shared_);
    return count;
}

NetScan Runtime::scan() {
    xSemaphoreTake(shared_, portMAX_DELAY);
    const auto out = visibleScan_;
    xSemaphoreGive(shared_);
    return out;
}

NetDiagnostics Runtime::diagnostics() {
    NetDiagnostics d;
    d.plateLimitC = MAX_PLATE_TEMP_C;
    d.maxControlGapUs = maxControlGap_; d.maxControlRunUs = maxControlRun_;
    d.maxSensorAgeUs = maxSensorAge_; d.maxFlashOpUs = maxFlashOp_;
    d.maxSensorRunUs = maxSensorRun_; d.maxNetworkRunUs = maxNetworkRun_; d.maxStorageRunUs = maxStorageRun_;
    d.resetReason = esp_reset_reason(); d.rebootCount = rebootCount_;
    if (controlHandle_) d.controlStackBytes = uxTaskGetStackHighWaterMark(controlHandle_);
    if (sensorHandle_) d.sensorStackBytes = uxTaskGetStackHighWaterMark(sensorHandle_);
    if (networkHandle_) d.networkStackBytes = uxTaskGetStackHighWaterMark(networkHandle_);
    if (storageHandle_) d.storageStackBytes = uxTaskGetStackHighWaterMark(storageHandle_);
    d.commandQueueUsed = uxQueueMessagesWaiting(commands_);
    const auto recorder = recorder_.status();
    d.recordQueueUsed = recorder.queueDepth;
    d.watchdogActive = watchdogActive_;
    d.filesystemMounted = recorder.available;
    return d;
}
}
