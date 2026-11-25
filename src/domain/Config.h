#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string_view>

namespace klima {
inline constexpr float unset = std::numeric_limits<float>::quiet_NaN();
inline constexpr std::size_t plateCount = 9;

struct PlateSensorConfig {
    uint64_t romId = 0;
    bool enabled = false;
};

struct Config {
    uint32_t revision = 0;
    std::array<PlateSensorConfig, plateCount> plate{};
    uint64_t tankRomId = 0;
    bool heatingEnabled = false;
    bool coolingEnabled = false;
    bool evaporationEnabled = false;
    float heatingTargetC = unset;
    float coolingTargetC = unset;
    float coolingHysteresisK = 1;
    float coolingMinOutsideC = 15;
    float dewpointMarginK = 2;
    float tankPlateMarginK = 1;
    float evaporationMinCapacityK = 2;
    float evaporationHysteresisK = 0.5f;
    float piKpDutyPerK = 0.027f;
    float piTiS = 4980;
    bool piIntegralEnabled = true;
    uint32_t piUpdateIntervalMs = 5000;
    float heaterResistanceOhm = unset;
    float supplyRatedCurrentA = 3;
    float heaterInputCurrentLimitA = 1.5f;
    float vinValidMinV = unset;
    float vinValidMaxV = unset;
    float vinGain = 16;
    float vinOffsetV = 0;
    bool vinCalibrationConfirmed = false;
    float overtempReleaseHysteresisK = 2;
    uint32_t sensorMaxAgeMs = 6000;
    uint32_t vinMaxAgeMs = 1000;
    uint32_t manualTimeoutS = 300;
    bool driverHeaterEnabled = false;
    float driverHeaterOnTempC = unset;
    float driverHeaterSlopePctPerK = unset;
    uint32_t driverWarmupS = 0;
    uint32_t energyCheckpointS = 900;
    float plateSpreadWarningK = unset;
    float plateChangeWarningKPerS = unset;
    char wifiSsid[33]{};
    char wifiPassword[65]{};
    char wifiIp[16]{};
    char wifiMask[16]{};
    char wifiGateway[16]{};
    char wifiDns[16]{};
    bool mqttEnabled = false;
    char mqttHost[254]{};
    uint32_t mqttPort = 1883;
    char mqttUser[65]{};
    char mqttPassword[129]{};
    char mqttClientId[65]{};
    char mqttPrefix[97] = "kaninchenstall/klima";
    uint32_t mqttIntervalS = 10;
    char devicePasswordHash[129]{};
    char ntpServer[254]{};
    char timezone[65] = "Europe/Berlin";
};

enum class ParameterType : uint8_t { Boolean, Float, Unsigned, String };
enum class ApplyPolicy : uint8_t { Immediate, PiRestart, OutputsOff, NetworkRestart };

struct ParameterDescriptor {
    const char* id;
    ParameterType type;
    const char* unit;
    bool persistent;
    bool secret;
    ApplyPolicy applyPolicy;
    double minimum;
    double maximum;
    bool allowUnset;
    std::size_t maxLength;
    std::size_t offset;
    bool minimumExclusive = false;
    bool writable = true;
};

extern const ParameterDescriptor parameters[];
extern const std::size_t parameterCount;
const ParameterDescriptor* findParameter(std::string_view id);
double readNumber(const Config& cfg, const ParameterDescriptor& parameter);
const char* readText(const Config& cfg, const ParameterDescriptor& parameter);
bool writeNumber(Config& cfg, const ParameterDescriptor& parameter, double value);
bool writeText(Config& cfg, const ParameterDescriptor& parameter, std::string_view value);
const char* validateConfig(const Config& cfg, float plateLimitC);
bool validRom(uint64_t romId);
bool vinConfigured(const Config& cfg);
bool heatingConfigured(const Config& cfg, float plateLimitC);
bool coolingConfigured(const Config& cfg, float plateLimitC);
bool evaporationConfigured(const Config& cfg, float plateLimitC);
bool driverConfigured(const Config& cfg);
}  // namespace klima
