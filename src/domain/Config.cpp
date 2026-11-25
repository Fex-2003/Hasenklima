#include "Config.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace klima {
namespace {
constexpr double finiteFloat = std::numeric_limits<float>::max();
constexpr auto boolType = ParameterType::Boolean;
constexpr auto floatType = ParameterType::Float;
constexpr auto unsignedType = ParameterType::Unsigned;
constexpr auto stringType = ParameterType::String;
constexpr auto immediate = ApplyPolicy::Immediate;
constexpr auto restart = ApplyPolicy::PiRestart;
constexpr auto off = ApplyPolicy::OutputsOff;
constexpr auto network = ApplyPolicy::NetworkRestart;
}

#define BOOL(id, field, policy) {id, boolType, "", true, false, policy, 0, 1, false, 0, offsetof(Config, field)}
#define FLOAT(id, field, unit, policy, min, max, unset, exclusive) \
    {id, floatType, unit, true, false, policy, min, max, unset, 0, offsetof(Config, field), exclusive}
#define UINT(id, field, unit, policy, min, max) \
    {id, unsignedType, unit, true, false, policy, min, max, false, 0, offsetof(Config, field)}
#define STR(id, field, secret) \
    {id, stringType, "", true, secret, network, 0, 0, false, sizeof(Config::field) - 1, offsetof(Config, field)}

const ParameterDescriptor parameters[] = {
    BOOL("heating_enabled", heatingEnabled, restart),
    BOOL("cooling_enabled", coolingEnabled, immediate),
    BOOL("evaporation_enabled", evaporationEnabled, immediate),
    FLOAT("heating_target_c", heatingTargetC, "°C", restart, -55, 125, true, false),
    FLOAT("cooling_target_c", coolingTargetC, "°C", off, -55, 125, true, false),
    FLOAT("cooling_hysteresis_k", coolingHysteresisK, "K", immediate, 0, 20, false, true),
    FLOAT("cooling_min_outside_c", coolingMinOutsideC, "°C", immediate, -40, 60, false, false),
    FLOAT("dewpoint_margin_k", dewpointMarginK, "K", off, 0, 20, false, false),
    FLOAT("tank_plate_margin_k", tankPlateMarginK, "K", immediate, 0, 30, false, false),
    FLOAT("evaporation_min_capacity_k", evaporationMinCapacityK, "K", immediate, 0, 30, false, true),
    FLOAT("evaporation_hysteresis_k", evaporationHysteresisK, "K", immediate, 0, 30, false, true),
    FLOAT("pi_kp_duty_per_k", piKpDutyPerK, "1/K", restart, 0, 1, false, false),
    FLOAT("pi_ti_s", piTiS, "s", restart, 1, 86400, false, false),
    BOOL("pi_integral_enabled", piIntegralEnabled, restart),
    UINT("pi_update_interval_ms", piUpdateIntervalMs, "ms", restart, 1000, 10000),
    FLOAT("heater_resistance_ohm", heaterResistanceOhm, "Ω", off, 0.1, 1000, true, false),
    FLOAT("supply_rated_current_a", supplyRatedCurrentA, "A", off, 0, finiteFloat, false, true),
    FLOAT("heater_input_current_limit_a", heaterInputCurrentLimitA, "A", off, 0, finiteFloat, false, false),
    FLOAT("vin_valid_min_v", vinValidMinV, "V", off, 0, finiteFloat, true, true),
    FLOAT("vin_valid_max_v", vinValidMaxV, "V", off, 0, finiteFloat, true, true),
    FLOAT("vin_gain", vinGain, "ratio", off, 0, finiteFloat, false, true),
    FLOAT("vin_offset_v", vinOffsetV, "V", off, -finiteFloat, finiteFloat, false, false),
    BOOL("vin_calibration_confirmed", vinCalibrationConfirmed, off),
    FLOAT("overtemp_release_hysteresis_k", overtempReleaseHysteresisK, "K", off, 0, 20, false, true),
    UINT("sensor_max_age_ms", sensorMaxAgeMs, "ms", off, 3000, 15000),
    UINT("vin_max_age_ms", vinMaxAgeMs, "ms", off, 200, 5000),
    UINT("manual_timeout_s", manualTimeoutS, "s", immediate, 60, 3600),
    BOOL("driver_heater_enabled", driverHeaterEnabled, immediate),
    FLOAT("driver_heater_on_temp_c", driverHeaterOnTempC, "°C", off, -40, 60, true, false),
    FLOAT("driver_heater_slope_pct_per_k", driverHeaterSlopePctPerK, "%/K", off, 0, 100, true, false),
    UINT("driver_warmup_s", driverWarmupS, "s", off, 0, 3600),
    UINT("energy_checkpoint_s", energyCheckpointS, "s", immediate, 60, 3600),
    FLOAT("plate_spread_warning_k", plateSpreadWarningK, "K", immediate, 0, 180, true, true),
    FLOAT("plate_change_warning_k_per_s", plateChangeWarningKPerS, "K/s", immediate, 0, finiteFloat, true, true),
    STR("wifi_ssid", wifiSsid, false),
    STR("wifi_password", wifiPassword, true),
    STR("wifi_ip", wifiIp, false),
    STR("wifi_mask", wifiMask, false),
    STR("wifi_gateway", wifiGateway, false),
    STR("wifi_dns", wifiDns, false),
    BOOL("mqtt_enabled", mqttEnabled, network),
    STR("mqtt_host", mqttHost, false),
    UINT("mqtt_port", mqttPort, "", network, 1, 65535),
    STR("mqtt_user", mqttUser, false),
    STR("mqtt_password", mqttPassword, true),
    STR("mqtt_client_id", mqttClientId, false),
    STR("mqtt_prefix", mqttPrefix, false),
    UINT("mqtt_interval_s", mqttIntervalS, "s", network, 2, 600),
    STR("device_password_hash", devicePasswordHash, true),
    STR("ntp_server", ntpServer, false),
    STR("timezone", timezone, false),
};

#undef BOOL
#undef FLOAT
#undef UINT
#undef STR

const std::size_t parameterCount = sizeof(parameters) / sizeof(parameters[0]);

namespace {
bool validNumber(const ParameterDescriptor& p, double n) {
    if (std::isnan(n)) return p.type == floatType && p.allowUnset;
    if (!std::isfinite(n) || n > p.maximum ||
        (p.minimumExclusive ? n <= p.minimum : n < p.minimum)) return false;
    return p.type == floatType || (p.type != stringType && std::trunc(n) == n);
}

bool validText(const ParameterDescriptor& p, std::string_view value) {
    return value.size() <= p.maxLength &&
        std::none_of(value.begin(), value.end(), [](unsigned char c) { return c < 32 || c == 127; });
}

bool integerIpv4(std::string_view text, uint32_t& result) {
    result = 0;
    for (unsigned i = 0; i < 4; ++i) {
        const auto dot = text.find('.');
        if ((i == 3) != (dot == std::string_view::npos)) return false;
        const auto part = text.substr(0, dot);
        if (part.empty() || part.size() > 3 || (part.size() != 1 && part.front() == '0')) return false;
        unsigned value = 0;
        for (char digit : part) {
            if (digit < '0' || digit > '9') return false;
            value = 10 * value + (digit - '0');
        }
        if (value > 255) return false;
        result = (result << 8) | value;
        if (i != 3) text.remove_prefix(dot + 1);
    }
    return true;
}

bool unicast(uint32_t ip) {
    const auto net = ip >> 24;
    return net != 0 && net != 127 && net < 224;
}

bool validHost(std::string_view host) {
    if (host.empty() || host.size() > 253) return false;
    if (host.back() == '.') host.remove_suffix(1);
    if (host.empty()) return false;
    auto isAlnum = [](unsigned char c) {
        return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
    };
    if (host.find('.') != std::string_view::npos &&
        host.find_first_not_of("0123456789.") == std::string_view::npos) {
        uint32_t ip = 0;
        return integerIpv4(host, ip) && unicast(ip);
    }
    while (!host.empty()) {
        const auto dot = host.find('.');
        const auto label = host.substr(0, dot);
        if (label.empty() || label.size() > 63 || !isAlnum(label.front()) || !isAlnum(label.back())) return false;
        if (!std::all_of(label.begin(), label.end(), [&](char c) { return c == '-' || isAlnum(c); })) return false;
        if (dot == std::string_view::npos) return true;
        host.remove_prefix(dot + 1);
    }
    return false;
}

const char* validateNetwork(const Config& cfg) {
    uint32_t ip = 0, mask = 0, gateway = 0, dns = 0;
    const auto hasSsid = cfg.wifiSsid[0] != 0;
    if ((hasSsid || cfg.wifiIp[0]) && (!integerIpv4(cfg.wifiIp, ip) || !unicast(ip))) return "WIFI_IP";
    if ((hasSsid || cfg.wifiMask[0]) && !integerIpv4(cfg.wifiMask, mask)) return "WIFI_MASK";
    if (mask && ((~mask & (~mask + 1u)) || ~mask < 3u)) return "WIFI_MASK";
    if ((hasSsid || cfg.wifiMask[0]) && !mask) return "WIFI_MASK";
    if ((hasSsid || cfg.wifiGateway[0]) && (!integerIpv4(cfg.wifiGateway, gateway) || !unicast(gateway))) return "WIFI_GATEWAY";
    if ((hasSsid || cfg.wifiDns[0]) && (!integerIpv4(cfg.wifiDns, dns) || !unicast(dns))) return "WIFI_DNS";
    if (ip && mask && ((ip & ~mask) == 0u || (ip & ~mask) == ~mask)) return "WIFI_SUBNET";
    if (ip && mask && gateway && ((ip & mask) != (gateway & mask) || gateway == ip ||
            (gateway & ~mask) == 0u || (gateway & ~mask) == ~mask)) return "WIFI_SUBNET";
    const std::string_view password(cfg.wifiPassword);
    if (!password.empty() && (password.size() < 8 || (password.size() == 64 &&
            password.find_first_not_of("0123456789abcdefABCDEF") != std::string_view::npos))) return "WIFI_PASSWORD";
    if ((cfg.mqttEnabled || cfg.mqttHost[0]) && !validHost(cfg.mqttHost)) return "MQTT_HOST";
    if (!cfg.mqttUser[0] && cfg.mqttPassword[0]) return "MQTT_PASSWORD_WITHOUT_USER";
    const std::string_view prefix(cfg.mqttPrefix);
    if (prefix.empty() || prefix.front() == '/' || prefix.back() == '/' ||
        prefix.find_first_of("+#") != std::string_view::npos) return "MQTT_PREFIX";
    if (cfg.ntpServer[0] && !validHost(cfg.ntpServer)) return "NTP_SERVER";
    if (!cfg.timezone[0]) return "TIMEZONE";
    return nullptr;
}

bool plateLimitSet(float limitC) {
    return std::isfinite(limitC) && limitC >= -55 && limitC <= 125;
}
}  // namespace

const ParameterDescriptor* findParameter(std::string_view id) {
    for (const auto& p : parameters) if (id == p.id) return &p;
    return nullptr;
}

double readNumber(const Config& cfg, const ParameterDescriptor& p) {
    const auto* field = reinterpret_cast<const unsigned char*>(&cfg) + p.offset;
    switch (p.type) {
        case boolType: return *reinterpret_cast<const bool*>(field);
        case floatType: return *reinterpret_cast<const float*>(field);
        case unsignedType: return *reinterpret_cast<const uint32_t*>(field);
        case stringType: break;
    }
    return unset;
}

const char* readText(const Config& cfg, const ParameterDescriptor& p) {
    return p.type == stringType ? reinterpret_cast<const char*>(&cfg) + p.offset : nullptr;
}

bool writeNumber(Config& cfg, const ParameterDescriptor& p, double value) {
    // Validate the representable value as well as the input (in particular exclusive bounds at zero).
    if (!validNumber(p, value)) return false;
    if (p.type == floatType) {
        value = static_cast<float>(value);
        if (!validNumber(p, value)) return false;
    }
    auto* field = reinterpret_cast<unsigned char*>(&cfg) + p.offset;
    switch (p.type) {
        case boolType: *reinterpret_cast<bool*>(field) = value != 0; return true;
        case floatType: *reinterpret_cast<float*>(field) = static_cast<float>(value); return true;
        case unsignedType: *reinterpret_cast<uint32_t*>(field) = static_cast<uint32_t>(value); return true;
        case stringType: return false;
    }
    return false;
}

bool writeText(Config& cfg, const ParameterDescriptor& p, std::string_view value) {
    if (p.type != stringType || !validText(p, value)) return false;
    auto* field = reinterpret_cast<char*>(&cfg) + p.offset;
    if (!value.empty()) std::memmove(field, value.data(), value.size());
    std::memset(field + value.size(), 0, p.maxLength + 1 - value.size());
    return true;
}

bool validRom(uint64_t romId) {
    if ((romId & 0xffu) != 0x28u) return false;
    uint8_t crc = 0;
    for (unsigned i = 0; i < 8; ++i, romId >>= 8) {
        auto byte = static_cast<uint8_t>(romId);
        for (unsigned bit = 0; bit < 8; ++bit, byte >>= 1) {
            const bool mix = (crc ^ byte) & 1;
            crc >>= 1;
            if (mix) crc ^= 0x8c;
        }
    }
    return crc == 0;
}

bool vinConfigured(const Config& cfg) {
    return cfg.vinCalibrationConfirmed && std::isfinite(cfg.vinValidMinV) && std::isfinite(cfg.vinValidMaxV) &&
        cfg.vinValidMinV > 0 && cfg.vinValidMaxV > cfg.vinValidMinV &&
        std::isfinite(cfg.vinGain) && cfg.vinGain > 0 && std::isfinite(cfg.vinOffsetV);
}

bool heatingConfigured(const Config& cfg, float plateLimitC) {
    return plateLimitSet(plateLimitC) && vinConfigured(cfg) &&
        std::isfinite(cfg.heatingTargetC) && cfg.heatingTargetC < plateLimitC - cfg.overtempReleaseHysteresisK &&
        std::isfinite(cfg.heaterResistanceOhm) && cfg.heaterResistanceOhm > 0 &&
        cfg.piKpDutyPerK > 0 && cfg.piTiS >= 1;
}
bool coolingConfigured(const Config& cfg, float plateLimitC) {
    return plateLimitSet(plateLimitC) && vinConfigured(cfg) &&
        std::isfinite(cfg.coolingTargetC) && cfg.coolingTargetC + cfg.coolingHysteresisK < plateLimitC;
}
bool evaporationConfigured(const Config& cfg, float plateLimitC) {
    return plateLimitSet(plateLimitC) && vinConfigured(cfg) && cfg.tankRomId;
}
bool driverConfigured(const Config& cfg) {
    return std::isfinite(cfg.driverHeaterOnTempC) && cfg.driverHeaterOnTempC >= -40 && cfg.driverHeaterOnTempC <= 60 &&
        std::isfinite(cfg.driverHeaterSlopePctPerK) && cfg.driverHeaterSlopePctPerK >= 0 && cfg.driverHeaterSlopePctPerK <= 100;
}

const char* validateConfig(const Config& cfg, float plateLimitC) {
    for (const auto& p : parameters) {
        if (p.type == stringType) {
            const auto* chars = readText(cfg, p);
            const auto* end = static_cast<const char*>(std::memchr(chars, 0, p.maxLength + 1));
            if (!end || !validText(p, {chars, static_cast<std::size_t>(end - chars)})) return p.id;
        } else if (!validNumber(p, readNumber(cfg, p))) {
            return p.id;
        }
    }
    if (cfg.tankRomId && !validRom(cfg.tankRomId)) return "TANK_ROM";
    for (std::size_t i = 0; i < cfg.plate.size(); ++i) {
        const auto& p = cfg.plate[i];
        if (!p.romId) {
            if (p.enabled) return "PLATE_ROM_MISSING";
            continue;
        }
        if (!validRom(p.romId)) return "PLATE_ROM";
        if (p.romId == cfg.tankRomId) return "ROM_DUPLICATE";
        for (std::size_t j = 0; j < i; ++j) if (cfg.plate[j].romId == p.romId) return "ROM_DUPLICATE";
    }
    if (std::isfinite(cfg.heatingTargetC) && std::isfinite(cfg.coolingTargetC) &&
        cfg.heatingTargetC >= cfg.coolingTargetC) return "TARGET_ORDER";
    if (plateLimitSet(plateLimitC)) {
        if (std::isfinite(cfg.heatingTargetC) &&
            cfg.heatingTargetC >= plateLimitC - cfg.overtempReleaseHysteresisK) return "HEATING_TARGET_LIMIT";
        if (std::isfinite(cfg.coolingTargetC) &&
            cfg.coolingTargetC + cfg.coolingHysteresisK >= plateLimitC) return "COOLING_TARGET_LIMIT";
    }
    if (cfg.heaterInputCurrentLimitA > cfg.supplyRatedCurrentA) return "HEATER_CURRENT_LIMIT";
    if (std::isfinite(cfg.vinValidMinV) && std::isfinite(cfg.vinValidMaxV) &&
        cfg.vinValidMinV >= cfg.vinValidMaxV) return "VIN_RANGE";
    if (cfg.evaporationHysteresisK > cfg.evaporationMinCapacityK) return "EVAPORATION_HYSTERESIS";
    if (cfg.piKpDutyPerK <= 0) return "PI_NO_GAIN";
    if (cfg.driverHeaterEnabled && !driverConfigured(cfg)) return "DRIVER_CONFIGURATION_UNSET";
    if (cfg.driverWarmupS && !driverConfigured(cfg)) return "DRIVER_WARMUP_CONFIGURATION_UNSET";
    if (cfg.heatingEnabled && !heatingConfigured(cfg, plateLimitC)) return "HEATING_CONFIGURATION_UNSET";
    if (cfg.coolingEnabled && !coolingConfigured(cfg, plateLimitC)) return "COOLING_CONFIGURATION_UNSET";
    if (cfg.evaporationEnabled && !evaporationConfigured(cfg, plateLimitC)) return "EVAPORATION_CONFIGURATION_UNSET";
    return validateNetwork(cfg);
}
}  // namespace klima
