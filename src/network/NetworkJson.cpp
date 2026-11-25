#include "NetworkJson.h"
#include <algorithm>
#include <cmath>
#include <cstring>

namespace klima {
namespace {
void sample(JsonWriter& json, const char* key, const NetSample& s, uint64_t nowUs) {
    const bool valid = s.valid && !s.stale && std::isfinite(s.value);
    json.object(key);
    json.number("value", valid ? s.value : kNetUnavailable);
    json.boolean("valid", valid);
    json.boolean("stale", s.stale);
    json.integer("error_code", s.error);
    json.integer("sequence", s.sequence);
    json.integer("sample_time_us", s.timeUs);
    if (s.timeUs && s.timeUs <= nowUs) json.integer("age_ms", (nowUs - s.timeUs) / 1000);
    else json.nullable("age_ms");
    json.end();
}

void platePoint(JsonWriter& json, const char* key, const NetworkSnapshot& s, unsigned i) {
    json.object(key);
    json.integer("position", i + 1);
    json.hex("rom_id", s.plateRom[i], true);
    json.boolean("enabled", s.enabledMask & (1u << i));
    sample(json, "temperature_c", s.plate[i], s.timestampUs);
    json.end();
}

void statistics(JsonWriter& json, const char* key, const NetworkSnapshot& s) {
    json.object(key);
    json.number("median_c", s.medianC);
    json.number("mean_c", s.meanC);
    json.number("min_c", s.minC);
    json.number("max_c", s.maxC);
    json.number("spread_k", s.spreadK);
    json.integer("enabled_mask", s.enabledMask);
    json.integer("enabled_count", s.enabledCount);
    json.boolean("complete", s.plateComplete);
    json.end();
}

void climate(JsonWriter& json, const char* key, const NetClimate& c, uint64_t nowUs, bool pressure) {
    json.object(key);
    sample(json, "temperature_c", c.temperature, nowUs);
    sample(json, "humidity_percent", c.humidity, nowUs);
    sample(json, "dewpoint_c", c.dewPoint, nowUs);
    if (pressure) sample(json, "pressure_pa", c.pressure, nowUs);
    json.end();
}

void heater(JsonWriter& json, const char* key, const NetworkSnapshot& s) {
    json.object(key);
    json.boolean("enabled", s.heatingEnabled);
    json.number("target_c", s.heatingTargetC);
    json.number("pwm_requested_percent", s.dutyRequested * 100.0);
    json.number("pwm_limit_percent", s.dutyLimit * 100.0);
    json.number("pwm_effective_percent", s.dutyEffective * 100.0);
    json.number("current_est_a", s.heaterCurrentA);
    json.number("power_est_w", s.heaterPowerW);
    json.number("power_limit_est_w", s.powerLimitW);
    json.number("reserve_a", s.reserveA);
    json.number("energy_est_wh", s.energyUWh / 1000000.0);
    json.integer("energy_checkpoint_us", s.energyCheckpointUs);
    json.boolean("energy_incomplete", s.energyIncomplete);
    json.boolean("energy_checkpoint_error", s.energyCheckpointError);
    json.number("pi_error_k", s.piErrorK);
    json.number("pi_integrator", s.piIntegrator);
    json.integer("inhibit_reasons", s.inhibitReasons);
    json.end();
}

void pump(JsonWriter& json, const char* key, bool enabled, bool commanded) {
    json.object(key);
    json.boolean("enabled", enabled);
    json.boolean("commanded", commanded);
    json.end();
}

void driver(JsonWriter& json, const char* key, const NetworkSnapshot& s) {
    json.object(key);
    json.boolean("enabled", s.driverEnabled);
    json.number("pwm_effective_percent", s.driverDuty * 100.0);
    json.boolean("curve_valid", s.driverCurveValid);
    json.boolean("warming", s.driverWarming);
    json.end();
}

void test(JsonWriter& json, const char* key, const NetworkSnapshot& s) {
    const RecorderStatus& r = s.recorder;
    const TestMetadata& m = r.metadata;
    json.object(key);
    json.boolean("available", r.available);
    json.boolean("running", r.running);
    json.boolean("download_active", r.downloadActive);
    json.boolean("io_error", r.ioError);
    json.integer("points", r.points);
    json.integer("durable_points", r.durablePoints);
    json.integer("schema_version", kTestSchemaVersion);
    json.text("firmware_version", m.firmwareVersion.data());
    json.hex("test_id", m.testId, true);
    json.hex("boot_id", m.bootId, true);
    json.integer("config_revision", m.configRevision);
    json.integer("start_uptime_us", m.startUs);
    if (m.startUtcSeconds) json.integer("start_utc_s", m.startUtcSeconds);
    else json.nullable("start_utc_s");
    json.boolean("utc_synchronized", m.startUtcSeconds != 0);
    json.integer("elapsed_s", s.testElapsedS);
    json.integer("remaining_s", s.testRemainingS);
    json.text("phase", testPhaseCode(m.phase));
    json.text("reason", testReasonCode(m.reason));
    json.integer("baseline_minutes", m.plan.baselineMinutes);
    json.integer("heat_minutes", m.plan.heatMinutes);
    json.integer("cooldown_minutes", m.plan.cooldownMinutes);
    json.number("pwm_percent", m.plan.pwmPercent);
    json.number("confirmed_pwm_percent", m.confirmedDuty * 100.0);
    json.integer("enabled_mask", m.activeMask);
    json.number("heater_resistance_ohm", m.resistanceOhm);
    json.number("current_limit_a", m.currentLimitA);
    json.number("max_plate_temp_c", m.plateLimitC);
    json.number("trigger", m.trigger);
    json.number("vin_gain", m.vinGain);
    json.number("vin_offset_v", m.vinOffsetV);
    json.number("vin_valid_min_v", m.vinMinV);
    json.number("vin_valid_max_v", m.vinMaxV);
    json.number("supply_rated_current_a", m.supplyRatedCurrentA);
    json.number("pi_kp_duty_per_k", m.piKp);
    json.number("pi_ti_s", m.piTiS);
    json.integer("pi_interval_ms", m.piIntervalMs);
    json.boolean("pi_integral_enabled", m.piIntegralEnabled);
    json.number("heating_target_c", m.heatingTargetC);
    json.number("cooling_target_c", m.coolingTargetC);
    if (m.heatStartMs != kNoTestTime) json.integer("heat_start_ms", m.heatStartMs);
    else json.nullable("heat_start_ms");
    if (m.heatStopMs != kNoTestTime) json.integer("heat_stop_ms", m.heatStopMs);
    else json.nullable("heat_stop_ms");
    if (m.endMs != kNoTestTime) json.integer("end_ms", m.endMs);
    else json.nullable("end_ms");
    json.integer("interval_s", kTestIntervalMs / 1000);
    json.integer("max_flush_interval_s", kTestFlushMs / 1000);
    const bool lossUnknown = m.reason == TestReason::Reset || m.reason == TestReason::Io ||
                             m.reason == TestReason::Incomplete;
    if (lossUnknown) json.nullable("unflushed_loss");
    else json.integer("unflushed_loss", 0);
    json.text("sample_order", "t=0 vor dem Sprung; weitere Phasengrenzen nach Ausgangswechsel");
    json.array("plate_rom_ids");
    for (uint64_t rom : m.plateRomIds) json.hex(nullptr, rom, true);
    json.end();
    json.end();
}

void faults(JsonWriter& json, const char* key, const NetworkSnapshot& s) {
    json.array(key);
    for (unsigned i = 0; i < std::min<size_t>(s.faultCount, s.faults.size()); ++i) writeFault(json, s.faults[i]);
    json.end();
}

void parameterValue(JsonWriter& json, const char* key, const Config& config, const ParameterDescriptor& p) {
    if (p.type == ParameterType::String) {
        json.text(key, readText(config, p));
    } else if (p.type == ParameterType::Boolean) {
        json.boolean(key, readNumber(config, p) != 0);
    } else {
        json.number(key, readNumber(config, p));
    }
}

const char* typeName(ParameterType type) {
    switch (type) {
    case ParameterType::Boolean: return "bool";
    case ParameterType::Float: return "float";
    case ParameterType::Unsigned: return "uint";
    case ParameterType::String: return "string";
    }
    return "unknown";
}

const char* policyName(ApplyPolicy policy) {
    switch (policy) {
    case ApplyPolicy::Immediate: return "IMMEDIATE";
    case ApplyPolicy::PiRestart: return "PI_RESTART";
    case ApplyPolicy::OutputsOff: return "OUTPUTS_OFF";
    case ApplyPolicy::NetworkRestart: return "NETWORK_RESTART";
    }
    return "unknown";
}
} // namespace

void writeFault(JsonWriter& json, const NetFault& f) {
    json.object();
    json.text("code", f.code);
    json.text("text", f.text);
    json.text("severity", f.severity);
    json.integer("first_uptime_us", f.firstUs);
    json.integer("last_uptime_us", f.lastUs);
    json.integer("repeats", f.repeats);
    json.integer("inhibit_mask", f.inhibitMask);
    json.boolean("active", f.active);
    json.boolean("latched", f.latched);
    json.end();
}

void writeHeader(JsonWriter& json, const NetworkSnapshot& s) {
    json.integer("schema_version", 1);
    json.integer("snapshot_sequence", s.sequence);
    json.hex("boot_id", s.bootId);
    json.integer("uptime_us", s.timestampUs);
    if (s.utcSeconds) json.integer("utc_s", s.utcSeconds);
    else json.nullable("utc_s");
}

void writeSnapshot(JsonWriter& json, const NetworkSnapshot& s, const NetworkStatus& network) {
    json.object();
    writeHeader(json, s);
    json.integer("config_revision", s.revision);
    json.text("mode", s.mode);
    json.text("auto_state", s.autoMode);
    json.boolean("error_flag", s.errorFlag);
    json.boolean("warning_flag", s.warningFlag);
    json.text("error_text", s.errorText);
    json.boolean("wifi_online", network.wifiOnline);
    json.boolean("mqtt_online", network.mqttOnline);
    json.array("plate");
    for (unsigned i = 0; i < s.plate.size(); ++i) platePoint(json, nullptr, s, i);
    json.end();
    statistics(json, "plate_statistics", s);
    sample(json, "tank_c", s.tank, s.timestampUs);
    climate(json, "room", s.room, s.timestampUs, false);
    climate(json, "stall", s.stall, s.timestampUs, true);
    climate(json, "outside", s.outside, s.timestampUs, true);
    sample(json, "vin_v", s.vin, s.timestampUs);
    json.boolean("vin_calibrated", s.vinCalibrated);
    json.number("dewpoint_control_c", s.dewPointControlC);
    json.text("dewpoint_source", s.dewPointSource);
    json.number("wetbulb_outside_c", s.wetBulbOutsideC);
    json.boolean("outside_pressure_fallback", s.pressureFallback);
    json.number("evaporation_capacity_k", s.evaporationCapacityK);
    json.number("cooling_target_c", s.coolingTargetC);
    heater(json, "heater", s);
    pump(json, "cooling_pump", s.coolingEnabled, s.coolingCommanded);
    pump(json, "evaporation_pump", s.evaporationEnabled, s.evaporationCommanded);
    driver(json, "driver_heater", s);
    json.object("manual");
    json.integer("session_id", s.manualSessionId);
    json.integer("remaining_s", s.manualRemainingS);
    json.boolean("condensation_override", s.manualDewPointOverride);
    json.end();
    test(json, "identification", s);
    faults(json, "faults", s);
    json.end();
}

void writeTest(JsonWriter& json, const NetworkSnapshot& s) {
    json.object();
    writeHeader(json, s);
    test(json, "identification", s);
    json.end();
}

void writeMqttPayload(JsonWriter& json, const char* topic, const NetworkSnapshot& s,
                      const NetworkStatus& network, unsigned plateIndex) {
    if (!strcmp(topic, "telemetry")) { writeSnapshot(json, s, network); return; }
    if (plateIndex > 0 && plateIndex <= s.plate.size()) { platePoint(json, nullptr, s, plateIndex - 1); return; }
    if (!strcmp(topic, "sensor/plate/statistics")) { statistics(json, nullptr, s); return; }
    if (!strcmp(topic, "sensor/tank")) { sample(json, nullptr, s.tank, s.timestampUs); return; }
    if (!strcmp(topic, "sensor/vin")) { sample(json, nullptr, s.vin, s.timestampUs); return; }
    if (!strcmp(topic, "sensor/room")) { climate(json, nullptr, s.room, s.timestampUs, false); return; }
    if (!strcmp(topic, "sensor/stall")) { climate(json, nullptr, s.stall, s.timestampUs, true); return; }
    if (!strcmp(topic, "sensor/outside")) { climate(json, nullptr, s.outside, s.timestampUs, true); return; }
    if (!strcmp(topic, "heater/state")) { heater(json, nullptr, s); return; }
    if (!strcmp(topic, "pump/cooling")) { pump(json, nullptr, s.coolingEnabled, s.coolingCommanded); return; }
    if (!strcmp(topic, "pump/evaporation")) { pump(json, nullptr, s.evaporationEnabled, s.evaporationCommanded); return; }
    if (!strcmp(topic, "driver_heater/state")) { driver(json, nullptr, s); return; }
    if (!strcmp(topic, "status/identification")) { test(json, nullptr, s); return; }
    if (!strcmp(topic, "status/faults")) { faults(json, nullptr, s); return; }
    json.object();
    writeHeader(json, s);
    json.text("mode", s.mode);
    json.text("auto_state", s.autoMode);
    json.integer("inhibit_reasons", s.inhibitReasons);
    json.end();
}

void writeConfig(JsonWriter& json, const Config& config) {
    json.object();
    json.integer("schema_version", 1);
    json.integer("revision", config.revision);
    json.object("values");
    for (size_t i = 0; i < parameterCount; ++i) {
        const auto& p = parameters[i];
        if (!p.secret) parameterValue(json, p.id, config, p);
    }
    json.end();
    json.object("secrets_set");
    json.boolean("wifi_password", config.wifiPassword[0]);
    json.boolean("mqtt_password", config.mqttPassword[0]);
    json.boolean("device_password", config.devicePasswordHash[0]);
    json.end();
    json.array("plate");
    for (size_t i = 0; i < config.plate.size(); ++i) {
        json.object();
        json.hex("rom_id", config.plate[i].romId, true);
        json.boolean("enabled", config.plate[i].enabled);
        json.end();
    }
    json.end();
    json.hex("tank_rom_id", config.tankRomId, true);
    json.end();
}

void writeParameters(JsonWriter& json) {
    json.object();
    json.integer("schema_version", 1);
    json.array("parameters");
    const Config defaults{};
    for (size_t i = 0; i < parameterCount; ++i) {
        const auto& p = parameters[i];
        const bool devicePassword = !strcmp(p.id, "device_password_hash");
        json.object();
        json.text("id", devicePassword ? "device_password" : p.id);
        json.text("type", typeName(p.type));
        json.text("unit", p.unit);
        json.boolean("persistent", p.persistent);
        json.boolean("secret", p.secret);
        json.boolean("writable", p.writable);
        json.boolean("allow_unset", p.allowUnset);
        json.text("apply_policy", policyName(p.applyPolicy));
        if (p.type == ParameterType::String) json.integer("max_length", devicePassword ? 128 : p.maxLength);
        if (p.type == ParameterType::Float || p.type == ParameterType::Unsigned) {
            json.number("minimum", p.minimum);
            json.number("maximum", p.maximum);
            json.boolean("minimum_exclusive", p.minimumExclusive);
        }
        if (!p.secret) parameterValue(json, "default", defaults, p);
        json.end();
    }
    json.end();
    json.object("sensor_mapping");
    json.integer("plate_positions", plateCount);
    json.text("rom_id_type", "uint64");
    json.text("rom_id_json", "16 hex digits or null");
    json.text("enabled_type", "bool");
    json.boolean("persistent", true);
    json.text("apply_policy", "OUTPUTS_OFF");
    json.end();
    json.end();
}

} // namespace klima
