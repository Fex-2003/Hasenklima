#include "Runtime.h"
#include "HardwareLimits.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <ctime>

namespace klima {
namespace {
bool mainOn(const OutputState& s) { return s.heaterDuty > 0 || s.cooling || s.evaporation; }
uint8_t functionBits(const Config& cfg) {
    return cfg.heatingEnabled | (cfg.coolingEnabled << 1) | (cfg.evaporationEnabled << 2);
}
void setFunctions(Config& cfg, unsigned mask, unsigned values) {
    if (mask & 1) cfg.heatingEnabled = values & 1;
    if (mask & 2) cfg.coolingEnabled = values & 2;
    if (mask & 4) cfg.evaporationEnabled = values & 4;
}
OperationResult immediate(int status, const char* code, const char* message) {
    OperationResult out;
    out.state = OperationState::Failed;
    out.status = status;
    out.code = code;
    out.message = message;
    return out;
}
}

OperationResult Runtime::submit(const Command& command) {
    if (!shared_) return immediate(503, "NOT_READY", "Befehlsdienst nicht verfügbar");
    xSemaphoreTake(shared_, portMAX_DELAY);
    OperationEntry* slot = nullptr;
    for (auto& entry : operations_) {
        if (entry.result.id && command.requestId[0] && !strcmp(entry.requestId, command.requestId)) {
            const auto out = entry.result;
            xSemaphoreGive(shared_);
            return out;
        }
        if (!entry.result.id || (entry.result.state != OperationState::Pending &&
            (!slot || (slot->result.id && entry.result.id < slot->result.id)))) slot = &entry;
    }
    uint32_t bits = 0;
    if (command.kind == CommandKind::StopAll) bits = 1u << StopAll;
    else if (command.kind == CommandKind::ManualStop) bits = 1u << StopManual;
    else if (command.kind == CommandKind::TestStop) bits = 1u << StopTest;
    else if (command.kind == CommandKind::Functions) {
        bits = (command.functionMask & ~command.functionValues & 7u) << DisableHeating;
    }
    const bool queueNeeded = !bits || (command.kind == CommandKind::Functions &&
                             (command.functionMask & command.functionValues));
    const bool queueFull = queueNeeded && !uxQueueSpacesAvailable(commands_);
    if (bits) {
        priorityBits_ |= bits;
        priorityCutoff_ = nextOperation_;
        for (unsigned i = 0; i < PriorityCount; ++i) {
            if (!(bits & (1u << i))) continue;
            const auto oldId = priorityIds_[i];
            if (oldId) {
                for (auto& entry : operations_) {
                    if (entry.result.id == oldId && entry.result.state == OperationState::Pending) {
                        const auto failed = immediate(409, "SUPERSEDED", "Befehl durch neueren Auftrag ersetzt");
                        entry.result = failed;
                        entry.result.id = oldId;
                    }
                }
            }
            priorityIds_[i] = 0;
        }
    }
    if (!slot || queueFull) {
        xSemaphoreGive(shared_);
        return bits ? immediate(503, "OFF_ACCEPTED_QUEUE_FULL", "AUS angenommen, Rückmeldung oder EIN derzeit nicht verfügbar")
                    : immediate(503, "COMMAND_QUEUE_FULL", "Befehlswarteschlange voll");
    }
    *slot = {};
    slot->result.id = ++nextOperation_;
    slot->result.revision = visibleConfig_.revision;
    slot->ownerId = command.ownerId;
    memcpy(slot->requestId, command.requestId, sizeof(slot->requestId));
    if (bits && !queueNeeded) {
        // Each multi-function command has one result, associated with its first bit.
        for (unsigned i = 0; i < PriorityCount; ++i) {
            if (bits & (1u << i)) { priorityIds_[i] = slot->result.id; break; }
        }
    }
    const auto out = slot->result;
    if (queueNeeded) {
        QueuedCommand event{command, out.id};
        if (bits) event.value.functionMask &= event.value.functionValues;
        xQueueSend(commands_, &event, 0);
    }
    xSemaphoreGive(shared_);
    return out;
}

bool Runtime::operation(uint32_t id, OperationResult& out) {
    xSemaphoreTake(shared_, portMAX_DELAY);
    bool found = false;
    for (const auto& entry : operations_) {
        if (entry.result.id == id) { found = true; out = entry.result; break; }
    }
    xSemaphoreGive(shared_);
    return found;
}

void Runtime::finish(uint32_t id, int status, const char* code, const char* message, uint32_t sessionId) {
    if (!id) return;
    xSemaphoreTake(shared_, portMAX_DELAY);
    for (auto& entry : operations_) {
        if (entry.result.id != id) continue;
        auto& out = entry.result;
        if (out.state != OperationState::Pending) break;
        out.state = status < 400 ? OperationState::Applied : OperationState::Failed;
        out.status = status;
        out.revision = config_.revision;
        out.code = code;
        out.message = message;
        out.sessionId = sessionId;
        out.heaterDutyLimit = result_.safety.heaterDutyLimit;
        out.heaterDutyEffective = outputs_.heaterDuty;
        out.inhibitReasons = result_.safety.inhibitReasons;
        break;
    }
    xSemaphoreGive(shared_);
}

void Runtime::applyOffMask(Config& cfg) const { setFunctions(cfg, offMask_, 0); }

void Runtime::endService(uint64_t now) {
    manual_.stop();
    manualOwner_ = 0;
    controller_.resetPi(now, inputs_.plateSequence);
}

void Runtime::processPriority(uint64_t now) {
    xSemaphoreTake(shared_, portMAX_DELAY);
    const auto bits = priorityBits_;
    const auto ids = priorityIds_;
    const auto cutoff = priorityCutoff_;
    priorityBits_ = 0;
    priorityIds_ = {};
    xSemaphoreGive(shared_);
    if (!bits) return;
    const bool all = bits & (1u << StopAll);
    const unsigned mask = all ? 7u : (bits >> DisableHeating) & 7u;
    if (mask) {
        mainOutputsOff(mask, now);
        offMask_ |= mask;
        dirtyOffMask_ |= mask;
        applyOffMask(config_);
        for (unsigned i = 0; i < enableCutoff_.size(); ++i) {
            if (mask & (1u << i)) enableCutoff_[i] = cutoff;
        }
        for (unsigned i = 0; i < PriorityCount; ++i) {
            if (!(i == StopAll || i >= DisableHeating) || !ids[i]) continue;
            finish(offOperations_[i], 409, "SUPERSEDED", "AUS-Auftrag durch neueren Auftrag ersetzt");
            offOperations_[i] = ids[i];
        }
        // A later enable must carry a new applied revision. Clear a suspended
        // manual request so it cannot reappear when the function is re-enabled.
        if (manual_.active()) {
            const auto request = manual_.request();
            if (((mask & 1) && request.heaterDuty > 0) || ((mask & 2) && request.cooling) ||
                ((mask & 4) && request.evaporation)) endService(now);
        }
    }
    if (all || (bits & (1u << StopManual))) {
        manualStopCutoff_ = cutoff;
        if (manual_.active()) mainOutputsOff(7, now);
        endService(now);
        finish(manualOperation_, 409, "STOPPED", "Manuelle Anforderung durch Stop beendet");
        manualOperation_ = 0;
        finish(ids[StopManual], 200, "OK", "Manuelle Sitzung beendet");
    }
    if (all || (bits & (1u << StopTest)) || (mask & 1u)) {
        testStopCutoff_ = cutoff;
        if (identification_.active()) {
            mainOutputsOff(7, now);
            identification_.abort(now, (mask & 1) && !all ? TestReason::HeatingDisabled : TestReason::User);
        }
        finish(ids[StopTest], 200, "OK", "Sprungantwort gestoppt");
        endService(now);
    }
}

void Runtime::beginConfigWrite(Config candidate, uint32_t id, uint64_t now) {
    if (writePending_ || energyResetPending_) {
        finish(id, 409, "CONFIG_BUSY", "Speicherauftrag läuft bereits");
        return;
    }
    if (const auto* error = validateConfig(candidate, MAX_PLATE_TEMP_C)) {
        finish(id, 422, error, "Konfiguration enthält ungültige oder fehlende Werte");
        return;
    }
    if (config_.revision == UINT32_MAX) {
        finish(id, 503, "REVISION_EXHAUSTED", "Konfigurationsrevision ausgeschöpft");
        return;
    }
    candidate.revision = config_.revision + 1;
    mainOutputsOff(7, now);
    pendingWrite_ = {StorageRequest::ConfigWrite, candidate, id};
    if (xQueueSend(storageRequests_, &pendingWrite_, 0) != pdPASS) {
        finish(id, 503, "STORAGE_QUEUE_FULL", "Speicherwarteschlange belegt");
        return;
    }
    writePending_ = true;
    writeOffMask_ = offMask_ & ~functionBits(candidate);
    dirtyOffMask_ = 0;
    writingOffOperations_ = offOperations_;
    offOperations_ = {};
    endService(now);
}

void Runtime::processStorageReplies(uint64_t now) {
    EnergyReply energyReply;
    if (xQueueReceive(energyReplies_, &energyReply, 0) == pdPASS &&
        energyReply.generation > persistedEnergyGeneration_) {
        energyIo_ = !energyReply.success;
        if (energyReply.success) {
            persistedEnergyGeneration_ = energyReply.generation;
            persistedEnergy_ = energyReply.microWh;
            lastCheckpointUs_ = energyReply.timeUs;
        }
    }
    StorageReply response;
    if (xQueueReceive(storageReplies_, &response, 0) == pdPASS) {
        if (energyResetPending_) {
            energyResetPending_ = false;
            if (response.success) {
                energy_.restore(0);
                persistedEnergy_ = 0;
                persistedEnergyGeneration_ = energyGeneration_;
                lastCheckpointUs_ = now;
            }
            energyIo_ = !response.success;
            finish(response.operationId, response.success ? 200 : 503, response.success ? "OK" : "ENERGY_IO",
                   response.success ? "Energiezähler zurückgesetzt" : "Energie konnte nicht gespeichert werden");
        } else if (writePending_ && pendingWrite_.operationId == response.operationId) {
            writePending_ = false;
            if (response.success) {
                offMask_ = writeOffMask_ | dirtyOffMask_;
                config_ = pendingWrite_.config;
                applyOffMask(config_);
                configInvalid_ = false;
                xQueueOverwrite(sensorConfig_, &config_);
                xSemaphoreTake(shared_, portMAX_DELAY);
                visibleConfig_ = config_;
                xSemaphoreGive(shared_);
            } else {
                // Keep the older durable revision, but hold outputs until a
                // successful configuration write and explicit acknowledgement.
                configInvalid_ = true;
            }
            finish(response.operationId, response.success ? 200 : 503, response.success ? "OK" : "CONFIG_IO",
                   response.success ? "Konfiguration gespeichert" : "Konfiguration konnte nicht gespeichert werden");
            for (uint32_t id : writingOffOperations_) {
                finish(id, response.success ? 200 : 503, response.success ? "OK" : "CONFIG_IO",
                       response.success ? "AUS dauerhaft gespeichert" : "AUS wirksam, Speicherung fehlgeschlagen");
            }
            writingOffOperations_ = {};
            endService(now);
        }
    }
    if (dirtyOffMask_ && !writePending_ && !energyResetPending_ && !testArming_ &&
        !identification_.active()) {
        beginConfigWrite(config_, 0, now);
        if (!writePending_) {
            for (uint32_t id : offOperations_) finish(id, 503, "CONFIG_IO", "AUS wirksam, Speicherung fehlgeschlagen");
            offOperations_ = {};
            dirtyOffMask_ = 0;
        }
    }
}

void Runtime::dispatch(const QueuedCommand& event, uint64_t now) {
    const auto& cmd = event.value;
    const auto id = event.operationId;
    if (((cmd.kind == CommandKind::ManualStart || cmd.kind == CommandKind::ManualOutput) &&
         id <= manualStopCutoff_) || (cmd.kind == CommandKind::TestStart && id <= testStopCutoff_)) {
        finish(id, 409, "STOPPED", "Älterer Stellbefehl nach Stop verworfen");
        return;
    }
    if (cmd.checkRevision && cmd.expectedRevision != config_.revision) {
        finish(id, 409, "REVISION_CONFLICT", "Konfiguration wurde zwischenzeitlich geändert");
        return;
    }
    if (writePending_ || energyResetPending_ || restartRequested_ || scanOperation_) {
        finish(id, 409, "BUSY", "Ein anderer Wartungsauftrag läuft");
        return;
    }
    if (identification_.active() || testArming_) {
        finish(id, 409, "TEST_RUNNING", "Sprungantwort zuerst beenden");
        return;
    }
    switch (cmd.kind) {
    case CommandKind::PatchConfig:
        if (functionBits(cmd.config) != functionBits(config_)) {
            finish(id, 422, "USE_FUNCTIONS", "Hauptfreigaben über die Funktionsschalter ändern");
        } else beginConfigWrite(cmd.config, id, now);
        return;
    case CommandKind::Functions: {
        for (unsigned i = 0; i < enableCutoff_.size(); ++i) {
            if ((cmd.functionMask & cmd.functionValues & (1u << i)) && id <= enableCutoff_[i]) {
                finish(id, 409, "STOPPED", "Ältere Freigabe nach AUS verworfen");
                return;
            }
        }
        Config candidate = config_;
        setFunctions(candidate, cmd.functionMask, cmd.functionValues);
        beginConfigWrite(candidate, id, now);
        return;
    }
    case CommandKind::Scan:
        if (mainOn(outputs_) || manual_.active()) {
            finish(id, 409, "OUTPUTS_ACTIVE", "Hauptaktoren für die Sensorsuche ausschalten");
        } else {
            scanOperation_ = id;
            ++scanGeneration_;
            scanRequest_ = scanGeneration_;
        }
        return;
    case CommandKind::PlateSensor:
    case CommandKind::TankSensor: {
        if (mainOn(outputs_) || manual_.active()) {
            finish(id, 409, "OUTPUTS_ACTIVE", "Hauptaktoren für die Sensorzuordnung ausschalten");
            return;
        }
        Config candidate = config_;
        if (cmd.kind == CommandKind::TankSensor) candidate.tankRomId = cmd.romId;
        else {
            if (cmd.plateIndex >= candidate.plate.size()) {
                finish(id, 422, "POSITION_INVALID", "Plattenposition ungültig");
                return;
            }
            auto& plate = candidate.plate[cmd.plateIndex];
            if (plate.enabled && !cmd.enabled && !cmd.confirmDisable) {
                finish(id, 422, "CONFIRM_DISABLE", "Wegfall der Temperaturüberwachung bestätigen");
                return;
            }
            plate = {cmd.romId, cmd.enabled};
        }
        beginConfigWrite(candidate, id, now);
        return;
    }
    case CommandKind::Acknowledge: {
        FaultCode target = FaultCode::None;
        for (unsigned n = 1; n < static_cast<unsigned>(FaultCode::Count); ++n) {
            const auto code = static_cast<FaultCode>(n);
            if (!strcmp(cmd.faultCode, faultCodeName(code))) { target = code; break; }
        }
        if (target == FaultCode::None) finish(id, 422, "FAULT_UNKNOWN", "Fehlercode unbekannt");
        else if (!controller_.acknowledge(config_, inputs_, now, MAX_PLATE_TEMP_C, flags_, target)) {
            finish(id, 409, "FAULT_ACTIVE", "Fehlerursache besteht noch oder Fehler ist nicht quittierbar");
        } else {
            endService(now);
            finish(id, 200, "OK", "Störung quittiert");
        }
        return;
    }
    case CommandKind::ManualStart:
    case CommandKind::ManualOutput: {
        const OutputRequest request{cmd.heaterDuty, 0, cmd.cooling, cmd.evaporation, cmd.ignoreDewPoint};
        if (cmd.kind == CommandKind::ManualOutput && manualOwner_ != cmd.ownerId) {
            finish(id, 403, "SESSION_OWNER", "Sitzung gehört einem anderen Gerät");
            return;
        }
        if (!result_.safety.mainAllowed) {
            finish(id, 422, "MANUAL_INHIBITED", "Hauptaktoren sind durch die Sicherheitsprüfung gesperrt");
            return;
        }
        const uint32_t duration = cmd.durationSeconds ? cmd.durationSeconds : config_.manualTimeoutS;
        const char* error = cmd.kind == CommandKind::ManualStart
            ? manual_.start(++nextSessionId_, now, duration, request, result_.safety)
            : manual_.update(cmd.sessionId, request, result_.safety, now, cmd.durationSeconds);
        if (error) finish(id, 422, error, "Manuelle Anforderung durch Betriebsgrenzen abgewiesen");
        else {
            if (cmd.kind == CommandKind::ManualStart) {
                manualOwner_ = cmd.ownerId;
                controller_.resetPi(now, inputs_.plateSequence);
                mainOutputsOff(7, now);
                manualEnableAtUs_ = now + CONTROL_TICK_US;
            }
            finish(manualOperation_, 409, "SUPERSEDED", "Manuelle Anforderung durch neueren Auftrag ersetzt");
            manualOperation_ = id;
        }
        return;
    }
    case CommandKind::TestStart: {
        if (manual_.active() || !ready_ || mainOn(outputs_) || result_.mode != OperatingMode::Auto) {
            finish(id, 409, "NOT_AUTO_NEUTRAL", "Start nur bei Automatik in Ruhe und ausgeschalteten Hauptaktoren");
            return;
        }
        const float request = cmd.test.pwmPercent / 100;
        const float confirmed = heaterDutyFromCount(quantizeHeaterDuty(request));
        if (!cmd.confirmed || !cmd.test.valid() || !result_.safety.heaterAllowed ||
            request > result_.safety.heaterDutyLimit || confirmed <= 0 || !testReadings(now).valid()) {
            finish(id, 422, "TEST_PRECHECK", "Testparameter, erforderliche Messwerte oder Heizfreigabe prüfen");
            return;
        }
        TestMetadata metadata{};
        metadata.plan = cmd.test;
        metadata.testId = (bootId_ & 0xffffffff00000000ULL) | ++testCounter_;
        metadata.bootId = bootId_;
        metadata.configRevision = config_.revision;
        metadata.startUs = now;
        const auto utc = time(nullptr);
        metadata.startUtcSeconds = utc >= 1704067200 ? utc : 0;
        metadata.activeMask = result_.plate.enabledMask;
        for (unsigned i = 0; i < config_.plate.size(); ++i) metadata.plateRomIds[i] = config_.plate[i].romId;
        metadata.confirmedDuty = confirmed;
        metadata.resistanceOhm = config_.heaterResistanceOhm;
        metadata.currentLimitA = config_.heaterInputCurrentLimitA;
        metadata.plateLimitC = MAX_PLATE_TEMP_C;
        metadata.vinGain = config_.vinGain;
        metadata.vinOffsetV = config_.vinOffsetV;
        metadata.vinMinV = config_.vinValidMinV;
        metadata.vinMaxV = config_.vinValidMaxV;
        metadata.piKp = config_.piKpDutyPerK;
        metadata.piTiS = config_.piTiS;
        metadata.piIntervalMs = config_.piUpdateIntervalMs;
        metadata.piIntegralEnabled = config_.piIntegralEnabled;
        metadata.heatingTargetC = config_.heatingTargetC;
        metadata.coolingTargetC = config_.coolingTargetC;
        metadata.supplyRatedCurrentA = config_.supplyRatedCurrentA;
        if (!identification_.prepare(metadata)) {
            finish(id, 422, "TEST_PRECHECK", "Testdaten ungültig");
            return;
        }
        const auto queued = recorder_.requestStart(identification_.metadata());
        if (queued != RecorderStart::Queued) {
            identification_.abort(now, TestReason::Io);
            identification_.resolveFinish(false);
            const bool downloading = queued == RecorderStart::DownloadActive;
            finish(id, downloading ? 409 : 503, downloading ? "DOWNLOAD_ACTIVE" : "RECORD_UNAVAILABLE",
                   downloading ? "CSV-Download läuft" : "Testspeicher derzeit nicht verfügbar");
            return;
        }
        flags_.recordQueueFull = flags_.testInputLimit = false;
        testArming_ = true;
        testOperation_ = id;
        endService(now);
        return;
    }
    case CommandKind::ResetEnergy:
    case CommandKind::Restart: {
        if (!cmd.confirmed || mainOn(outputs_) || manual_.active() || functionBits(config_)) {
            finish(id, 409, "MAINTENANCE_BLOCKED", "Bestätigung und ausgeschaltete Hauptfunktionen erforderlich");
            return;
        }
        StorageRequest storageRequest;
        storageRequest.operationId = id;
        storageRequest.kind = cmd.kind == CommandKind::Restart ? StorageRequest::Restart : StorageRequest::EnergyReset;
        if (cmd.kind == CommandKind::Restart) requestCheckpoint(now, true);
        if (xQueueSend(storageRequests_, &storageRequest, 0) != pdPASS) {
            finish(id, 503, "STORAGE_QUEUE_FULL", "Speicherwarteschlange belegt");
        } else if (cmd.kind == CommandKind::Restart) restartRequested_ = true;
        else energyResetPending_ = true;
        return;
    }
    default:
        finish(id, 400, "COMMAND_UNKNOWN", "Unbekannter Auftrag");
    }
}
}
