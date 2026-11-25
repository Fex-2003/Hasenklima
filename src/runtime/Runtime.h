#pragma once

#include "domain/Control.h"
#include "domain/History.h"
#include "hardware/ActuatorManager.h"
#include "network/NetworkContract.h"
#include "network/NetworkManager.h"
#include "sensors/SensorManager.h"
#include "storage/PersistentStore.h"
#include <atomic>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

namespace klima {
class Runtime final : public NetworkPort {
public:
    void begin();
    Config config() override;
    void snapshot(NetworkSnapshot&) override;
    size_t history(NetHistoryPoint*, size_t) override;
    size_t faultHistory(NetFault*, size_t) override;
    NetScan scan() override;
    NetDiagnostics diagnostics() override;
    OperationResult submit(const Command&) override;
    bool operation(uint32_t, OperationResult&) override;
    bool beginDownload() override { return recorder_.beginDownload(); }
    int nextCsv(char* out, size_t cap) override { return recorder_.nextCsv(out, cap); }
    void endDownload() override { recorder_.endDownload(); }

private:
    enum Priority : unsigned { StopAll, StopManual, StopTest, DisableHeating, DisableCooling, DisableEvaporation, PriorityCount };
    struct OperationEntry {
        OperationResult result;
        char requestId[41]{};
        uint32_t ownerId{};
    };
    struct QueuedCommand { Command value; uint32_t operationId{}; };
    struct StorageRequest {
        enum Kind : uint8_t { ConfigWrite, EnergyReset, Restart } kind{};
        Config config;
        uint32_t operationId{};
    };
    struct StorageReply { bool success{}; uint32_t operationId{}; };
    struct EnergyRequest { uint64_t microWh{}; uint32_t generation{}; };
    struct EnergyReply { uint64_t timeUs{}, microWh{}; uint32_t generation{}; bool success{}; };
    static void controlEntry(void*);
    static void sensorEntry(void*);
    static void storageEntry(void*);
    static void networkEntry(void*);
    void controlTask();
    void sensorTask();
    void storageTask();
    void networkTask();
    void dispatch(const QueuedCommand&, uint64_t);
    void processPriority(uint64_t);
    void processStorageReplies(uint64_t);
    void processIdentification(uint64_t);
    void captureIdentification(uint64_t);
    void applyOutputs(OutputRequest, uint64_t);
    void mainOutputsOff(uint8_t mask, uint64_t);
    void beginConfigWrite(Config, uint32_t, uint64_t);
    void applyOffMask(Config&) const;
    void collectSnapshot(uint64_t);
    void publish(uint64_t);
    void endService(uint64_t);
    void finish(uint32_t id, int status, const char* code, const char* message, uint32_t sessionId = 0);
    void requestCheckpoint(uint64_t now, bool force = false);
    TestReadings testReadings(uint64_t now) const;
    OperatingMode requestedMode() const;

    ActuatorManager actuators_;
    SensorManager sensors_;
    PersistentStore nvs_;
    Recorder recorder_;
    NetworkManager network_{*this};
    Controller controller_;
    ManualSession manual_;
    Identification identification_;
    EnergyCounter energy_;
    Config config_{};
    RuntimeFlags flags_{};
    Inputs inputs_{};
    ControlResult result_{};
    OutputState outputs_{};
    NetworkSnapshot snapshot_{};
    Config visibleConfig_{};
    NetworkSnapshot visibleSnapshot_{};
    NetScan visibleScan_{};
    History history_;
    std::array<NetFault, 100> faultHistory_{};
    size_t faultHistoryCount_{};
    std::array<OperationEntry, 24> operations_{};
    std::array<uint32_t, PriorityCount> priorityIds_{};
    std::array<uint32_t, PriorityCount> offOperations_{}, writingOffOperations_{};
    SemaphoreHandle_t shared_ = nullptr;
    QueueHandle_t commands_ = nullptr, sensorConfig_ = nullptr, sensorInputs_ = nullptr;
    QueueHandle_t storageRequests_ = nullptr, storageReplies_ = nullptr, energyRequests_ = nullptr, energyReplies_ = nullptr;
    QueueHandle_t serialPoints_ = nullptr;
    TaskHandle_t controlHandle_{}, sensorHandle_{}, storageHandle_{}, networkHandle_{};
    StorageRequest pendingWrite_{};
    bool writePending_{}, configInvalid_{}, ready_{}, testArming_{}, testFinishing_{}, restartRequested_{};
    uint8_t offMask_{}, dirtyOffMask_{}, writeOffMask_{};
    uint32_t priorityBits_{}, nextOperation_{}, testOperation_{}, scanOperation_{}, manualOperation_{};
    uint32_t priorityCutoff_{}, manualStopCutoff_{}, testStopCutoff_{};
    std::array<uint32_t, 3> enableCutoff_{};
    uint32_t nextSessionId_{}, energyGeneration_{}, persistedEnergyGeneration_{}, scanGeneration_{};
    uint32_t manualOwner_{};
    std::atomic<uint32_t> scanRequest_{0}, scanCompleted_{0}, rebootCount_{0};
    std::atomic<uint32_t> maxControlGap_{0}, maxControlRun_{0}, maxSensorAge_{0}, maxFlashOp_{0};
    std::atomic<uint32_t> maxSensorRun_{0}, maxStorageRun_{0}, maxNetworkRun_{0};
    std::atomic<bool> storageReady_{false}, watchdogActive_{false};
    std::atomic<bool> serialLive_{false};
    std::atomic<uint32_t> serialPointsDropped_{0};
    uint64_t bootId_{}, testCounter_{}, readyNotBeforeUs_{}, nextPublishUs_{}, manualEnableAtUs_{};
    uint64_t lastCheckpointUs_{}, lastCheckpointAttemptUs_{}, persistedEnergy_{};
    bool energyIo_{}, energyResetPending_{};
};
}
