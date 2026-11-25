#include "Runtime.h"
#include "HardwareLimits.h"
#include "network/NetworkJson.h"
#include <Arduino.h>
#include <esp_system.h>
#include <esp_task_wdt.h>
#include <algorithm>
#include <cstring>
#ifdef __has_include
#if __has_include("LocalSecrets.h")
#include "LocalSecrets.h"
#define KLIMA_HAS_LOCAL_SECRETS
#endif
#endif

namespace klima {
namespace {
uint64_t clockUs() { return esp_timer_get_time(); }

void maximum(std::atomic<uint32_t>& metric, uint64_t value) {
    const auto bounded = static_cast<uint32_t>(std::min<uint64_t>(value, UINT32_MAX));
    auto previous = metric.load(std::memory_order_relaxed);
    while (bounded > previous && !metric.compare_exchange_weak(previous, bounded)) {}
}

void feed(bool monitored) {
    if (monitored) esp_task_wdt_reset();
}
}

void Runtime::begin() {
    flags_.actuatorInitFailed = !actuators_.begin();
    Serial.begin(115200);
    bootId_ = (uint64_t{esp_random()} << 32) | esp_random();
    shared_ = xSemaphoreCreateMutex();
    commands_ = xQueueCreate(4, sizeof(QueuedCommand));
    sensorConfig_ = xQueueCreate(1, sizeof(Config));
    sensorInputs_ = xQueueCreate(1, sizeof(Inputs));
    storageRequests_ = xQueueCreate(1, sizeof(StorageRequest));
    storageReplies_ = xQueueCreate(1, sizeof(StorageReply));
    energyRequests_ = xQueueCreate(1, sizeof(EnergyRequest));
    energyReplies_ = xQueueCreate(1, sizeof(EnergyReply));
    serialPoints_ = xQueueCreate(4, sizeof(TestPoint));
    if (!shared_ || !commands_ || !sensorConfig_ || !sensorInputs_ || !storageRequests_ ||
        !storageReplies_ || !energyRequests_ || !energyReplies_) {
        Serial.println("Nicht genug RAM für die Task-Infrastruktur. Ausgänge AUS.");
        return;
    }

    const bool nvsReady = nvs_.begin();
    const auto configLoad = nvs_.loadConfig(config_, MAX_PLATE_TEMP_C);
    configInvalid_ = !nvsReady || configLoad == LoadResult::Invalid || configLoad == LoadResult::Io;
#ifdef KLIMA_HAS_LOCAL_SECRETS
    if (configLoad == LoadResult::Missing && nvsReady) {
        Config candidate{};
        snprintf(candidate.wifiSsid, sizeof(candidate.wifiSsid), "%s", KLIMA_INITIAL_SSID);
        snprintf(candidate.wifiPassword, sizeof(candidate.wifiPassword), "%s", KLIMA_INITIAL_WIFI_PASSWORD);
        snprintf(candidate.wifiIp, sizeof(candidate.wifiIp), "%s", KLIMA_INITIAL_IP);
        snprintf(candidate.wifiMask, sizeof(candidate.wifiMask), "%s", KLIMA_INITIAL_NETMASK);
        snprintf(candidate.wifiGateway, sizeof(candidate.wifiGateway), "%s", KLIMA_INITIAL_GATEWAY);
        snprintf(candidate.wifiDns, sizeof(candidate.wifiDns), "%s", KLIMA_INITIAL_DNS);
        candidate.revision = 1;
        if (makePasswordHash(KLIMA_INITIAL_DEVICE_PASSWORD, candidate.devicePasswordHash,
                             sizeof(candidate.devicePasswordHash)) &&
            !validateConfig(candidate, MAX_PLATE_TEMP_C) && nvs_.saveConfig(candidate)) {
            config_ = candidate;
        } else {
            configInvalid_ = true;
            Serial.println("Lokale Erstkonfiguration ungültig oder nicht speicherbar.");
        }
    }
#endif
    EnergyCheckpoint checkpoint{};
    const auto energyLoad = nvs_.loadEnergy(checkpoint);
    energyIo_ = energyLoad == LoadResult::Invalid || energyLoad == LoadResult::Io;
    persistedEnergy_ = checkpoint.microWh;
    energy_.restore(persistedEnergy_);
    rebootCount_ = nvs_.bootCount();
    visibleConfig_ = config_;
    visibleSnapshot_.bootId = bootId_;
    xQueueOverwrite(sensorConfig_, &config_);

    watchdogActive_ = esp_task_wdt_init(8, true) == ESP_OK;
    readyNotBeforeUs_ = clockUs() + DS_CONVERSION_US;
    const bool ok =
        xTaskCreatePinnedToCore(storageEntry, "klima-storage", 6144, this, 1, &storageHandle_, 0) == pdPASS &&
        xTaskCreatePinnedToCore(sensorEntry, "klima-sensors", 5376, this, 3, &sensorHandle_, 1) == pdPASS &&
        xTaskCreatePinnedToCore(networkEntry, "klima-network", 9216, this, 1, &networkHandle_, 0) == pdPASS &&
        xTaskCreatePinnedToCore(controlEntry, "klima-control", 10240, this, 5, &controlHandle_, 1) == pdPASS;
    if (!ok) Serial.println("Task konnte nicht erstellt werden. Ausgänge AUS.");
}

void Runtime::controlEntry(void* self) { static_cast<Runtime*>(self)->controlTask(); }
void Runtime::sensorEntry(void* self) { static_cast<Runtime*>(self)->sensorTask(); }
void Runtime::storageEntry(void* self) { static_cast<Runtime*>(self)->storageTask(); }
void Runtime::networkEntry(void* self) { static_cast<Runtime*>(self)->networkTask(); }

OperatingMode Runtime::requestedMode() const {
    if (!ready_ || writePending_ || scanOperation_ || restartRequested_) return OperatingMode::Initializing;
    if (testArming_ || identification_.active()) return OperatingMode::Identification;
    if (manual_.active()) return OperatingMode::Manual;
    return OperatingMode::Auto;
}

void Runtime::controlTask() {
    const bool monitor = esp_task_wdt_add(nullptr) == ESP_OK;
    TickType_t scheduled = xTaskGetTickCount();
    uint64_t previousUs = clockUs();
    for (;;) {
        Inputs update{};
        if (xQueueReceive(sensorInputs_, &update, 0) == pdPASS) inputs_ = update;
        const uint64_t now = clockUs();
        maximum(maxControlGap_, now - previousUs);
        flags_.controlOverrun = !monitor || now - previousUs >= CONTROL_GAP_LIMIT_US;
        previousUs = now;
        flags_.configInvalid = configInvalid_;
        flags_.actuatorInitFailed = !actuators_.healthy();
        flags_.wifiOffline = config_.wifiSsid[0] && !network_.wifiOnline();
        flags_.mqttOffline = config_.mqttEnabled && !network_.mqttOnline();
        flags_.webOffline = network_.wifiOnline() && !network_.httpReady();
        flags_.energyCheckpointError = energyIo_;
        flags_.recordIoError = recorder_.status().ioError;
        ready_ |= now >= readyNotBeforeUs_ && storageReady_;

        processPriority(now);
        processStorageReplies(now);
        if (manual_.expire(now)) endService(now);
        if (scanOperation_ && scanCompleted_ == scanGeneration_) {
            finish(scanOperation_, 200, "OK", "Sensorsuche abgeschlossen");
            scanOperation_ = 0;
            endService(now);
        }

        QueuedCommand command;
        // Refresh safety before admission of a new service command. A command
        // uses measured conditions from this tick, not from an HTTP snapshot.
        if (uxQueueMessagesWaiting(commands_)) {
            const auto mode = requestedMode();
            result_ = controller_.evaluate(config_, inputs_, now, MAX_PLATE_TEMP_C, flags_,
                                           mode, mode == OperatingMode::Manual ? manual_.request() : OutputRequest{});
            if (xQueueReceive(commands_, &command, 0) == pdPASS) dispatch(command, now);
        }
        const auto mode = requestedMode();
        if (mode == OperatingMode::Identification) processIdentification(now);
        else {
            result_ = controller_.evaluate(config_, inputs_, now, MAX_PLATE_TEMP_C, flags_, mode,
                                           mode == OperatingMode::Manual ? manual_.request() : OutputRequest{});
        }
        if (manual_.active() && result_.mode == OperatingMode::Fault) endService(now);
        auto permitted = result_.authorized;
        if (testArming_ || testFinishing_ || scanOperation_ || writePending_ || restartRequested_ ||
            !ready_ || !std::isfinite(MAX_PLATE_TEMP_C) || now < manualEnableAtUs_) {
            permitted.heaterDuty = 0;
            permitted.cooling = permitted.evaporation = false;
        }
        applyOutputs(permitted, now);
        if (manualOperation_ && now >= manualEnableAtUs_) {
            if (manual_.active()) finish(manualOperation_, 200, "OK", "Manuelle Sitzung aktualisiert", manual_.id());
            else finish(manualOperation_, 409, "MANUAL_STOPPED", "Manuelle Sitzung beendet");
            manualOperation_ = 0;
        }
        if (identification_.active()) captureIdentification(now);
        if (now >= nextPublishUs_) publish(now);
        maximum(maxControlRun_, clockUs() - now);
        feed(monitor);
        vTaskDelayUntil(&scheduled, pdMS_TO_TICKS(CONTROL_TICK_US / 1000));
    }
}

void Runtime::sensorTask() {
    const bool monitor = esp_task_wdt_add(nullptr) == ESP_OK;
    Config sensorConfig{};
    uint32_t scanningId = 0;
    sensors_.begin();
    for (;;) {
        const auto now = clockUs();
        xQueuePeek(sensorConfig_, &sensorConfig, 0);
        const auto target = scanRequest_.load();
        if (target != scanningId) {
            scanningId = target;
            sensors_.startScan();
        }
        if (sensors_.tick(sensorConfig, now)) {
            const auto& current = sensors_.inputs();
            xQueueOverwrite(sensorInputs_, &current);
        }
        if (scanningId && scanningId != scanCompleted_ && !sensors_.scanning()) {
            xSemaphoreTake(shared_, portMAX_DELAY);
            visibleScan_ = sensors_.scan();
            xSemaphoreGive(shared_);
            scanCompleted_ = scanningId;
        }
        maximum(maxSensorRun_, clockUs() - now);
        feed(monitor);
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

void Runtime::storageTask() {
    const bool monitor = esp_task_wdt_add(nullptr) == ESP_OK;
    const auto mountStart = clockUs();
    recorder_.begin();
    maximum(maxFlashOp_, clockUs() - mountStart);
    storageReady_ = true;
    for (;;) {
        const auto now = clockUs();
        recorder_.service(now);
        StorageRequest request;
        if (xQueueReceive(storageRequests_, &request, 0) == pdPASS) {
            bool ok = false;
            if (request.kind == StorageRequest::ConfigWrite) ok = nvs_.saveConfig(request.config);
            else if (request.kind == StorageRequest::EnergyReset) {
                EnergyRequest discarded;
                xQueueReceive(energyRequests_, &discarded, 0);
                ok = nvs_.resetEnergy();
            } else {
                // Control already stopped the loads and queued its final energy segment.
                EnergyRequest finalEnergy;
                if (xQueueReceive(energyRequests_, &finalEnergy, 0) == pdPASS) nvs_.saveEnergy(finalEnergy.microWh);
                esp_restart();
            }
            const StorageReply reply{ok, request.operationId};
            xQueueOverwrite(storageReplies_, &reply);
        }
        EnergyRequest energy;
        if (xQueueReceive(energyRequests_, &energy, 0) == pdPASS) {
            const bool ok = nvs_.saveEnergy(energy.microWh);
            const EnergyReply response{clockUs(), energy.microWh, energy.generation, ok};
            xQueueOverwrite(energyReplies_, &response);
        }
        const auto duration = clockUs() - now;
        maximum(maxStorageRun_, duration);
        maximum(maxFlashOp_, duration);
        feed(monitor);
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void Runtime::networkTask() {
    network_.begin();
    uint64_t nextUartUs = 0;
    NetworkSnapshot serialSnapshot;
    char uartCommand[12]{};
    char csvLine[640]{};
    unsigned commandLength = 0, csvUsed = 0, csvSent = 0;
    bool serialCsv = false;
    for (;;) {
        const auto now = clockUs();
        network_.tick(now);
        for (unsigned read = 0; read < sizeof(uartCommand) && Serial.available(); ++read) {
            const int ch = Serial.read();
            if (ch == '\r') continue;
            if (ch != '\n') {
                if (commandLength < sizeof(uartCommand) - 1) uartCommand[commandLength++] = ch;
                continue;
            }
            uartCommand[commandLength] = '\0';
            commandLength = 0;
            if (!serialCsv && !serialLive_ && !strcmp(uartCommand, "csv")) {
                serialCsv = beginDownload();
                if (!serialCsv) Serial.println("CSV nicht verfügbar.");
                csvUsed = csvSent = 0;
            } else if (serialPoints_ && !serialCsv && !serialLive_ && !strcmp(uartCommand, "csvlive")) {
                xQueueReset(serialPoints_);
                serialPointsDropped_ = 0;
                Serial.print(kTestCsvColumns);
                serialLive_ = true;
                csvUsed = csvSent = 0;
            } else if ((serialCsv || serialLive_) && !strcmp(uartCommand, "stopcsv")) {
                if (serialCsv) endDownload();
                serialCsv = false;
                serialLive_ = false;
                nextUartUs = now + 10 * usPerS;
            } else if (!serialCsv && !serialLive_ && !strcmp(uartCommand, "status")) nextUartUs = now;
        }
        if (serialCsv || serialLive_) {
            if (csvSent == csvUsed) {
                int size = 0;
                if (serialCsv) size = nextCsv(csvLine, sizeof(csvLine));
                else if (const auto lost = serialPointsDropped_.exchange(0)) {
                    size = snprintf(csvLine, sizeof(csvLine), "# UART-Puffer: %u Punkte ausgelassen\n", lost);
                } else {
                    TestPoint point;
                    if (xQueueReceive(serialPoints_, &point, 0) == pdPASS) {
                        size = formatCsvPoint(csvLine, sizeof(csvLine), point);
                    }
                }
                if (size <= 0 && serialCsv) {
                    endDownload();
                    serialCsv = false;
                    nextUartUs = now + 10 * usPerS;
                    if (size < 0) Serial.println("# CSV-Lesefehler");
                } else if (size > 0) {
                    csvUsed = size;
                    csvSent = 0;
                }
            }
            if (serialCsv || serialLive_) {
                const auto room = static_cast<unsigned>(std::max(0, Serial.availableForWrite()));
                const auto count = std::min(room, csvUsed - csvSent);
                if (count) csvSent += Serial.write(reinterpret_cast<const uint8_t*>(csvLine + csvSent), count);
            }
        } else if (now >= nextUartUs) {
            nextUartUs = now + 10 * usPerS;
            snapshot(serialSnapshot);
            const auto activeConfig = config();
            const auto& s = serialSnapshot;
            Serial.printf("t=%llu s reset=%u mode=%s/%s WiFi=%d MQTT=%d VIN=%.2f valid=%d "
                          "PWM=%.4f/%.4f/%.4f Q4=%.2f Pumpen=%d/%d Leistung~%.2f W "
                          "PI-Soll=%.2f Kp=%.5f Ti=%.1f I=%.4f Maske=%03x median/mean/min/max/spread=%.2f/%.2f/%.2f/%.2f/%.2f "
                          "Sperren=%08x %s\n", now / usPerS, unsigned(esp_reset_reason()), s.mode, s.autoMode,
                          network_.wifiOnline(), network_.mqttOnline(), s.vin.value, s.vin.valid,
                          s.dutyRequested, s.dutyLimit, s.dutyEffective, s.driverDuty, s.coolingCommanded,
                          s.evaporationCommanded, s.heaterPowerW, s.heatingTargetC, activeConfig.piKpDutyPerK,
                          activeConfig.piTiS, s.piIntegrator, s.enabledMask,
                          s.medianC, s.meanC, s.minC, s.maxC, s.spreadK, s.inhibitReasons, s.errorText);
            for (unsigned i = 0; i < s.plate.size(); ++i) {
                Serial.printf(" P%u=%g[%u,%u]", i + 1, s.plate[i].value, s.plate[i].valid, s.plate[i].error);
            }
            Serial.printf(" Tank=%g[%u] Raum=%g/%g Tau=%g Stall=%g/%g Tau=%g Außen=%g/%g Tau=%g "
                          "Regeltau=%g(%s) Feuchtkugel=%g\n", s.tank.value, s.tank.valid,
                          s.room.temperature.value, s.room.humidity.value, s.room.dewPoint.value,
                          s.stall.temperature.value, s.stall.humidity.value, s.stall.dewPoint.value,
                          s.outside.temperature.value, s.outside.humidity.value, s.outside.dewPoint.value,
                          s.dewPointControlC, s.dewPointSource, s.wetBulbOutsideC);
        }
        maximum(maxNetworkRun_, clockUs() - now);
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}
}
