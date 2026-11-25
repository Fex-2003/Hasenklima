#include "Model.h"

#include <algorithm>
#include <cmath>

namespace klima {
namespace {
bool inRange(float value, float min, float max) { return std::isfinite(value) && value >= min && value <= max; }

float localDewpoint(const ClimateSamples& s, uint64_t nowUs, uint32_t ageMs) {
    if (!freshTemperature(s.temperature, nowUs, ageMs) || !fresh(s.humidity, nowUs, ageMs)) return unset;
    return dewpointC(s.temperature.value, s.humidity.value);
}
}  // namespace

const char* modeName(OperatingMode mode) {
    switch (mode) {
        case OperatingMode::Initializing: return "INITIALIZING";
        case OperatingMode::Auto: return "AUTO";
        case OperatingMode::Manual: return "MANUAL";
        case OperatingMode::Identification: return "IDENTIFICATION";
        case OperatingMode::Fault: return "FAULT";
    }
    return "UNKNOWN";
}
const char* autoStateName(AutoState state) {
    switch (state) {
        case AutoState::Neutral: return "NEUTRAL";
        case AutoState::Heating: return "HEATING";
        case AutoState::Cooling: return "COOLING";
        case AutoState::Evaporating: return "EVAPORATING";
        case AutoState::CoolingAndEvaporating: return "COOLING_AND_EVAPORATING";
    }
    return "UNKNOWN";
}
const char* dewpointSourceName(DewpointSource source) {
    switch (source) {
        case DewpointSource::Room: return "ROOM";
        case DewpointSource::Outside: return "OUTSIDE";
        case DewpointSource::Fixed23: return "FIXED_23";
    }
    return "UNKNOWN";
}
bool fresh(const Sample& sample, uint64_t nowUs, uint32_t maxAgeMs) {
    return sample.valid && std::isfinite(sample.value) && sample.sampleTimeUs <= nowUs &&
        nowUs - sample.sampleTimeUs <= maxAgeMs * usPerMs;
}
bool freshTemperature(const Sample& sample, uint64_t nowUs, uint32_t maxAgeMs) {
    return fresh(sample, nowUs, maxAgeMs) && inRange(sample.value, minDsC, maxDsC);
}

PlateStatistics plateStatistics(const Config& cfg, const Inputs& in, uint64_t nowUs) {
    PlateStatistics stats;
    std::array<float, plateCount> values{};
    std::size_t validCount = 0;
    float sum = 0;
    for (std::size_t i = 0; i < cfg.plate.size(); ++i) {
        if (!cfg.plate[i].enabled) continue;
        stats.enabledMask |= uint16_t(1u << i);
        ++stats.enabledCount;
        const auto& sample = in.plate[i];
        if (!cfg.plate[i].romId || !in.plateSequence || sample.sequence != in.plateSequence ||
            !freshTemperature(sample, nowUs, cfg.sensorMaxAgeMs)) continue;
        values[validCount++] = sample.value;
        sum += sample.value;
    }
    stats.complete = stats.enabledCount && validCount == stats.enabledCount;
    if (!stats.complete) return stats;
    std::sort(values.begin(), values.begin() + validCount);
    stats.medianC = (values[(validCount - 1) / 2] + values[validCount / 2]) / 2;
    stats.meanC = sum / static_cast<float>(validCount);
    stats.minC = values[0];
    stats.maxC = values[validCount - 1];
    stats.spreadK = stats.maxC - stats.minC;
    return stats;
}

float dewpointC(float temperatureC, float humidityPct) {
    if (!inRange(temperatureC, -55, 125) || !inRange(humidityPct, 0, 100) || humidityPct == 0) return unset;
    constexpr float b = 243.12f;
    constexpr float a = 17.62f;
    const float gamma = std::log(humidityPct / 100) + a * temperatureC / (b + temperatureC);
    const float result = b * gamma / (a - gamma);
    return std::isfinite(result) ? std::min(temperatureC, result) : unset;
}

DerivedClimate climateCalculations(const Config& cfg, const Inputs& in, uint64_t nowUs) {
    DerivedClimate result;
    const auto ageMs = cfg.sensorMaxAgeMs;
    result.roomDewpointC = localDewpoint(in.room, nowUs, ageMs);
    result.stallDewpointC = localDewpoint(in.stall, nowUs, ageMs);
    result.outsideDewpointC = localDewpoint(in.outside, nowUs, ageMs);
    if (std::isfinite(result.roomDewpointC)) {
        result.controlDewpointC = result.roomDewpointC;
        result.dewpointSource = DewpointSource::Room;
    } else if (std::isfinite(result.outsideDewpointC)) {
        result.controlDewpointC = result.outsideDewpointC;
        result.dewpointSource = DewpointSource::Outside;
    }
    if (freshTemperature(in.outside.temperature, nowUs, ageMs) && fresh(in.outside.humidity, nowUs, ageMs)) {
        const bool pressureValid = fresh(in.outside.pressure, nowUs, ageMs) &&
            inRange(in.outside.pressure.value, 30000, 110000);
        result.pressureFallback = !pressureValid;
        result.wetbulbOutsideC = wetbulbC(in.outside.temperature.value, in.outside.humidity.value,
                                         pressureValid ? in.outside.pressure.value : 101325.0f);
    }
    return result;
}

uint16_t quantizeHeaterDuty(float duty) {
    if (!std::isfinite(duty) || duty <= 0) return 0;
    if (duty >= 1) return heaterPwmPeriodCounts;
    // Arduino-ESP32 2.x translates argument 1023 to full-on. Count 1023 is unavailable.
    return std::min(static_cast<uint16_t>(duty * heaterPwmPeriodCounts), heaterPwmMaxPartialCount);
}
float heaterDutyFromCount(uint16_t count) {
    return count >= heaterPwmPeriodCounts - 1 ? 1.0f :
        static_cast<float>(count) / static_cast<float>(heaterPwmPeriodCounts);
}
float quantizeDriverDuty(float duty) {
    if (!std::isfinite(duty) || duty <= 0) return 0;
    if (duty >= 1) return 1;
    const auto steps = static_cast<uint16_t>(duty * driverPwmSteps);
    return static_cast<float>(steps) / driverPwmSteps;
}

float heaterDutyLimit(const Config& cfg, const Inputs& in, uint64_t nowUs) {
    if (!vinConfigured(cfg) || !fresh(in.vin, nowUs, cfg.vinMaxAgeMs) ||
        !inRange(in.vin.value, cfg.vinValidMinV, cfg.vinValidMaxV) ||
        !std::isfinite(cfg.heaterResistanceOhm) || cfg.heaterResistanceOhm <= 0 ||
        !std::isfinite(cfg.supplyRatedCurrentA) || cfg.supplyRatedCurrentA <= 0 ||
        !inRange(cfg.heaterInputCurrentLimitA, 0, cfg.supplyRatedCurrentA)) return 0;
    const double idealLimit = static_cast<double>(cfg.heaterInputCurrentLimitA) *
        cfg.heaterResistanceOhm / in.vin.value;
    const auto ceiling = std::min(1.0, idealLimit);
    float result = static_cast<float>(ceiling);
    // A rounded float at a quantization boundary must not authorize a count above the model limit.
    if (static_cast<double>(result) > ceiling) result = std::nextafter(result, 0.0f);
    return result;
}

PowerEstimate estimatePower(const Config& cfg, const Inputs& in, uint64_t nowUs, float duty) {
    PowerEstimate out;
    if (!fresh(in.vin, nowUs, cfg.vinMaxAgeMs) || in.vin.value <= 0 || !vinConfigured(cfg) ||
        !std::isfinite(cfg.heaterResistanceOhm) || cfg.heaterResistanceOhm <= 0 || !inRange(duty, 0, 1)) return out;
    const double onCurrent = static_cast<double>(in.vin.value) / cfg.heaterResistanceOhm;
    out.onCurrentA = static_cast<float>(onCurrent);
    out.averageCurrentA = static_cast<float>(duty * onCurrent);
    out.powerW = static_cast<float>(duty * onCurrent * in.vin.value);
    out.limitPowerW = static_cast<float>(heaterDutyLimit(cfg, in, nowUs) * onCurrent * in.vin.value);
    out.valid = std::isfinite(out.powerW) && std::isfinite(out.onCurrentA) && std::isfinite(out.limitPowerW);
    return out.valid ? out : PowerEstimate{};
}
}  // namespace klima
