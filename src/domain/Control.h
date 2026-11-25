#pragma once

#include "Model.h"

namespace klima {
enum class Severity : uint8_t { Info, Warning, Error, Critical };
enum class FaultCode : uint8_t {
    None,
    PlateSensorInvalid, PlateSensorStale, NoActivePlateSensor, PlateOvertemp,
    VinInvalid, VinOutOfRange, ConfigInvalid, ActuatorInitFailed, ControlOverrun,
    RoomSensorInvalid, OutsideTemperatureInvalid, OutsideHumidityInvalid,
    TankSensorInvalid, StallSensorInvalid, RecordIoError, RecordQueueFull, TestInputLimit,
    WifiOffline, MqttOffline, PlateSensorDisabled, PlateSpreadWarning, PlateChangeWarning,
    EnergyCheckpointError, WebOffline, Count
};

constexpr uint32_t faultBit(FaultCode code) {
    return code == FaultCode::None || code >= FaultCode::Count ? 0u : 1u << (static_cast<unsigned>(code) - 1);
}
const char* faultCodeName(FaultCode code);
const char* faultText(FaultCode code);
Severity faultSeverity(FaultCode code);

enum Inhibit : uint32_t {
    InhibitPlateLimitUnset = 1u << 0,
    InhibitPlateMissing = 1u << 1,
    InhibitFault = 1u << 2,
    InhibitVin = 1u << 3,
    InhibitHeatDisabled = 1u << 4,
    InhibitCoolDisabled = 1u << 5,
    InhibitEvapDisabled = 1u << 6,
    InhibitWarmup = 1u << 7,
    InhibitDriverAmbient = 1u << 8,
    InhibitCondensation = 1u << 9,
    InhibitEvapSensors = 1u << 10,
    InhibitCoolingOutside = 1u << 11,
    InhibitCoolingTank = 1u << 12,
    InhibitHeatingLimit = 1u << 13,
    InhibitInterlock = 1u << 14,
    InhibitTransition = 1u << 15,
    InhibitConfig = 1u << 16,
};

struct FaultSummary {
    uint32_t activeMask = 0;
    uint32_t latchedMask = 0;
    FaultCode topCode = FaultCode::None;
    uint8_t errorCount = 0;
    bool errorFlag = false;
    bool warningFlag = false;
};

struct FaultStatus {
    uint64_t firstTimeUs = 0;
    uint64_t lastTimeUs = 0;
    uint32_t count = 0;
    uint32_t inhibitReasons = 0;
    Severity severity = Severity::Info;
    bool active = false;
    bool latched = false;
};

enum class FaultChange : uint8_t { Raised, Cleared, Acknowledged };
struct FaultEvent {
    uint64_t timeUs = 0;
    uint32_t count = 0;
    FaultCode code = FaultCode::None;
    Severity severity = Severity::Info;
    FaultChange change = FaultChange::Raised;
};

class FaultManager {
public:
    static constexpr std::size_t capacity = 100;
    void observe(FaultCode code, bool active, uint64_t nowUs);
    bool acknowledge(FaultCode code, uint64_t nowUs);
    FaultSummary summary() const;
    std::size_t size() const { return size_; }
    const FaultEvent& at(std::size_t index) const;
    const FaultStatus& status(FaultCode code) const;
private:
    void addEvent(FaultCode code, FaultChange change, uint64_t nowUs);
    std::array<FaultStatus, static_cast<std::size_t>(FaultCode::Count)> statuses_{};
    std::array<FaultEvent, capacity> history_{};
    std::size_t next_ = 0;
    std::size_t size_ = 0;
};

struct RuntimeFlags {
    bool configInvalid = false;
    bool actuatorInitFailed = false;
    bool controlOverrun = false;
    bool wifiOffline = false;
    bool mqttOffline = false;
    bool recordIoError = false;
    bool recordQueueFull = false;
    bool testInputLimit = false;
    bool webOffline = false;
    bool energyCheckpointError = false;
    bool forceMainOff = false;
    float driverQualifiedMinC = 0;
};

struct SafetyDecision {
    bool mainAllowed = false;
    bool heaterAllowed = false;
    bool coolingAllowed = false;
    bool evaporationAllowed = false;
    bool condensationAllowed = false;
    bool driverAllowed = false;
    bool vinInRange = false;
    float heaterDutyLimit = 0;
    uint32_t inhibitReasons = 0;
    float warmupRemainingS = 0;
    float overtempTriggerC = unset;
};

struct ControlResult {
    OutputRequest requested;
    OutputRequest authorized;
    SafetyDecision safety;
    PlateStatistics plate;
    DerivedClimate climate;
    FaultSummary faults;
    OperatingMode mode = OperatingMode::Initializing;
    AutoState autoState = AutoState::Neutral;
    float integralDuty = 0;
};

class PIHeatingController {
public:
    void reset(uint64_t nowUs, uint32_t plateSequence);
    float demand(const Config& cfg, float medianC, uint64_t nowUs, uint32_t plateSequence, bool& overrun);
    void integrate(const Config& cfg, float limitDuty);
    void pause(uint64_t nowUs);
    float integralDuty() const { return integralDuty_; }
private:
    uint32_t resetPlateSequence_ = 0;
    uint64_t lastCycleUs_ = 0;
    double pendingDeltaS_ = 0;
    float integralDuty_ = 0;
    float errorK_ = 0;
    float rawDuty_ = 0;
    float requestedDuty_ = 0;
};

class Controller {
public:
    ControlResult evaluate(const Config& cfg, const Inputs& in, uint64_t nowUs, float plateLimitC,
                           const RuntimeFlags& flags, OperatingMode mode, OutputRequest service = {});
    void resetPi(uint64_t nowUs, uint32_t plateSequence);
    bool acknowledge(const Config& cfg, const Inputs& in, uint64_t nowUs, float plateLimitC,
                     const RuntimeFlags& flags, FaultCode code = FaultCode::None);
    const FaultManager& faults() const { return faults_; }
private:
    ControlResult assess(const Config& cfg, const Inputs& in, uint64_t nowUs, float plateLimitC,
                         const RuntimeFlags& flags);
    FaultManager faults_;
    PIHeatingController pi_;
    bool coolingLatched_ = false;
    bool evaporationLatched_ = false;
    bool warmupStarted_ = false;
    uint64_t warmupStartUs_ = 0;
    bool controlStarted_ = false;
    bool heatingEnabled_ = false;
    uint32_t configRevision_ = 0;
    OperatingMode previousMode_ = OperatingMode::Initializing;
    uint32_t previousPlateSequence_ = 0;
    std::array<Sample, plateCount> previousPlate_;
    bool plateChangeWarning_ = false;
    DerivedClimate climate_;
    std::array<float, 7> climateValues_{};
    bool climateInitialized_ = false;
};

class ManualSession {
public:
    const char* start(uint64_t id, uint64_t nowUs, uint32_t durationS,
                      const OutputRequest& request, const SafetyDecision& safety);
    const char* update(uint64_t id, const OutputRequest& request, const SafetyDecision& safety,
                       uint64_t nowUs = 0, uint32_t durationS = 0);
    bool expire(uint64_t nowUs);
    void stop();
    bool active() const { return id_ != 0; }
    uint64_t id() const { return id_; }
    uint64_t deadlineUs() const { return deadlineUs_; }
    OutputRequest request() const { return request_; }
private:
    uint64_t id_ = 0;
    uint64_t deadlineUs_ = 0;
    OutputRequest request_;
};
}  // namespace klima
