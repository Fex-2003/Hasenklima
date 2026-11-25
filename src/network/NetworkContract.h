#pragma once

#include "../domain/Config.h"
#include "../storage/Recorder.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace klima {

// Fixed-size copies cross the task boundary. The runtime retains ownership of all
// commands, control states and storage leases. No HTTP code touches actuators.
enum class CommandKind : uint8_t {
    PatchConfig, Functions, Scan, PlateSensor, TankSensor,
    ManualStart, ManualOutput, ManualStop, TestStart, TestStop, Acknowledge,
    StopAll, ResetEnergy, Restart
};

struct Command {
    CommandKind kind{};
    char requestId[41]{};
    uint32_t expectedRevision{};
    bool checkRevision{};
    Config config{};
    uint8_t functionMask{}, functionValues{}; // Heating, cooling, evaporation, bits 0–2.
    uint8_t plateIndex{};
    uint64_t romId{};
    bool enabled{}, confirmDisable{}, confirmed{};
    uint32_t ownerId{}, sessionId{}, durationSeconds{};
    float heaterDuty{};
    bool cooling{}, evaporation{}, ignoreDewPoint{};
    IdentificationPlan test{};
    char faultCode[41]{};
};

enum class OperationState : uint8_t { Pending, Applied, Failed };
struct OperationResult {
    uint32_t id{}, revision{}, sessionId{};
    OperationState state{OperationState::Pending};
    int status{202};
    const char* code{"PENDING"}; // String literals owned by the runtime.
    const char* message{"Auftrag angenommen"};
    float heaterDutyLimit{std::numeric_limits<float>::quiet_NaN()};
    float heaterDutyEffective{std::numeric_limits<float>::quiet_NaN()};
    uint32_t inhibitReasons{};
};

struct NetSample {
    float value{};
    uint64_t timeUs{};
    uint32_t sequence{};
    uint16_t error{};
    bool valid{}, stale{};
};

struct NetClimate { NetSample temperature, humidity, pressure, dewPoint; };

struct NetFault {
    const char* code{};
    const char* text{};
    const char* severity{};
    uint64_t firstUs{}, lastUs{};
    uint32_t repeats{}, inhibitMask{};
    bool active{}, latched{};
};

constexpr float kNetUnavailable = std::numeric_limits<float>::quiet_NaN();
struct NetworkSnapshot {
    uint64_t timestampUs{}, bootId{}, utcSeconds{};
    uint32_t sequence{}, revision{};
    std::array<NetSample, 9> plate{};
    std::array<uint64_t, 9> plateRom{};
    NetSample tank, vin;
    NetClimate room, stall, outside;
    float medianC{kNetUnavailable}, meanC{kNetUnavailable}, minC{kNetUnavailable},
        maxC{kNetUnavailable}, spreadK{kNetUnavailable};
    uint16_t enabledMask{};
    uint8_t enabledCount{};
    bool plateComplete{};
    float dewPointControlC{kNetUnavailable}, wetBulbOutsideC{kNetUnavailable},
        evaporationCapacityK{kNetUnavailable};
    const char* dewPointSource{"FIXED_23"};
    bool pressureFallback{}, vinCalibrated{};
    bool heatingEnabled{}, coolingEnabled{}, evaporationEnabled{}, driverEnabled{};
    bool coolingCommanded{}, evaporationCommanded{}, driverCurveValid{}, driverWarming{};
    float dutyRequested{}, dutyLimit{}, dutyEffective{}, driverDuty{}, heaterCurrentA{kNetUnavailable},
        heaterPowerW{kNetUnavailable}, powerLimitW{kNetUnavailable}, reserveA{kNetUnavailable},
        heatingTargetC{kNetUnavailable}, coolingTargetC{kNetUnavailable},
        piErrorK{kNetUnavailable}, piIntegrator{};
    uint32_t inhibitReasons{};
    uint64_t energyUWh{}, energyCheckpointUs{};
    bool energyIncomplete{}, energyCheckpointError{};
    const char* mode{"INITIALIZING"};
    const char* autoMode{"NEUTRAL"};
    uint32_t manualSessionId{}, manualRemainingS{};
    bool manualDewPointOverride{};
    bool errorFlag{}, warningFlag{};
    char errorText[180]{};
    std::array<NetFault, 32> faults{};
    uint8_t faultCount{};
    uint32_t testRemainingS{}, testElapsedS{};
    RecorderStatus recorder{};
};

struct NetHistoryPoint {
    uint64_t timeUs{};
    std::array<int16_t, 5> centiC{}; // room, outside, tank, plate median, plate mean
    uint8_t validMask{};
};

struct NetScan {
    std::array<uint64_t, 24> plate{}, tank{};
    uint8_t plateCount{}, tankCount{};
    bool overflow{}, ready{};
};

struct NetDiagnostics {
    float plateLimitC{kNetUnavailable};
    uint32_t maxControlGapUs{}, maxControlRunUs{}, maxSensorAgeUs{}, maxFlashOpUs{};
    uint32_t maxSensorRunUs{}, maxNetworkRunUs{}, maxStorageRunUs{}, resetReason{}, rebootCount{};
    uint32_t controlStackBytes{}, sensorStackBytes{}, networkStackBytes{}, storageStackBytes{};
    uint16_t commandQueueUsed{}, recordQueueUsed{};
    bool watchdogActive{}, filesystemMounted{};
};

class NetworkPort {
public:
    virtual ~NetworkPort() = default;
    virtual Config config() = 0;
    virtual void snapshot(NetworkSnapshot& out) = 0;
    virtual size_t history(NetHistoryPoint* out, size_t capacity) = 0;
    virtual size_t faultHistory(NetFault* out, size_t capacity) = 0;
    virtual NetScan scan() = 0;
    virtual NetDiagnostics diagnostics() = 0;
    virtual OperationResult submit(const Command& command) = 0;
    virtual bool operation(uint32_t id, OperationResult& out) = 0;
    virtual bool beginDownload() = 0;
    virtual int nextCsv(char* buffer, size_t capacity) = 0;
    virtual void endDownload() = 0;
};

// Suitable for the private compile-time bootstrap password as well as updates
// from an authenticated Setup session. Generates a fresh salt per invocation.
bool makePasswordHash(const char* password, char* out, size_t capacity);

} // namespace klima

