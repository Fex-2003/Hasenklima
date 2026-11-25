#pragma once

#include "Config.h"

namespace klima {
constexpr uint64_t usPerMs = 1000;
constexpr uint64_t usPerS = 1000000;
constexpr uint16_t heaterPwmPeriodCounts = 1024;
constexpr uint16_t heaterPwmMaxPartialCount = heaterPwmPeriodCounts - 2;
constexpr uint16_t driverPwmSteps = 100;
constexpr float minDsC = -55;
constexpr float maxDsC = 125;

struct Sample {
    float value = unset;
    uint64_t sampleTimeUs = 0;
    uint32_t sequence = 0;
    bool valid = false;
    uint16_t errorCode = 0;
};

struct ClimateSamples {
    Sample temperature;
    Sample humidity;
    Sample pressure;
};

struct Inputs {
    // Fully committed sweep for statistics, newer independently read samples for safety.
    std::array<Sample, plateCount> plate;
    std::array<Sample, plateCount> platePending;
    Sample tank;
    Sample vin;
    ClimateSamples room;
    ClimateSamples outside;
    ClimateSamples stall;
    uint32_t plateSequence = 0;
    uint32_t configRevision = 0;
    uint32_t platePendingRevision = 0;
};

struct PlateStatistics {
    float medianC = unset;
    float meanC = unset;
    float minC = unset;
    float maxC = unset;
    float spreadK = unset;
    uint16_t enabledMask = 0;
    uint8_t enabledCount = 0;
    bool complete = false;
};

enum class DewpointSource : uint8_t { Room, Outside, Fixed23 };
struct DerivedClimate {
    float roomDewpointC = unset;
    float stallDewpointC = unset;
    float outsideDewpointC = unset;
    float controlDewpointC = 23;
    float wetbulbOutsideC = unset;
    DewpointSource dewpointSource = DewpointSource::Fixed23;
    bool pressureFallback = false;
};

struct OutputRequest {
    float heaterDuty = 0;
    float driverDuty = 0;
    bool cooling = false;
    bool evaporation = false;
    bool condensationOverride = false;
};

struct OutputState {
    float heaterDuty = 0;
    float driverDuty = 0;
    bool cooling = false;
    bool evaporation = false;
    uint16_t heaterCount = 0;
};

enum class OperatingMode : uint8_t { Initializing, Auto, Manual, Identification, Fault };
enum class AutoState : uint8_t { Neutral, Heating, Cooling, Evaporating, CoolingAndEvaporating };

const char* modeName(OperatingMode mode);
const char* autoStateName(AutoState state);
const char* dewpointSourceName(DewpointSource source);
bool fresh(const Sample& sample, uint64_t nowUs, uint32_t maxAgeMs);
bool freshTemperature(const Sample& sample, uint64_t nowUs, uint32_t maxAgeMs);
PlateStatistics plateStatistics(const Config& cfg, const Inputs& in, uint64_t nowUs);
DerivedClimate climateCalculations(const Config& cfg, const Inputs& in, uint64_t nowUs);
float dewpointC(float temperatureC, float humidityPct);
float wetbulbC(float temperatureC, float humidityPct, float pressurePa);
uint16_t quantizeHeaterDuty(float duty);
float heaterDutyFromCount(uint16_t count);
float quantizeDriverDuty(float duty);

struct PowerEstimate {
    bool valid = false;
    float onCurrentA = unset;
    float averageCurrentA = unset;
    float powerW = unset;
    float limitPowerW = unset;
};
float heaterDutyLimit(const Config& cfg, const Inputs& in, uint64_t nowUs);
PowerEstimate estimatePower(const Config& cfg, const Inputs& in, uint64_t nowUs, float heaterDuty);
}  // namespace klima
