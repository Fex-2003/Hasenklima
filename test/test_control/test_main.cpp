#include "domain/Config.h"
#include "domain/Control.h"
#include "domain/History.h"

#include <cmath>
#include <cstdio>
#include <cstring>

namespace {
using namespace klima;
constexpr float fixedLimitC = 42;  // Native test fixture only.
unsigned passed = 0;
unsigned failed = 0;

void check(bool condition, const char* expression, int line) {
    if (condition) ++passed;
    else {
        ++failed;
        std::printf("FAIL line %d: %s\n", line, expression);
    }
}
#define CHECK(expression) check((expression), #expression, __LINE__)

bool near(double a, double b, double tolerance = 0.0001) { return std::abs(a - b) <= tolerance; }

uint64_t rom(unsigned identifier) {
    uint64_t prefix = 0x28u | (uint64_t{identifier} << 8);
    for (unsigned crc = 0; crc <= 255; ++crc) {
        const auto candidate = prefix | (uint64_t{crc} << 56);
        if (validRom(candidate)) return candidate;
    }
    return 0;
}

Config configured() {
    Config cfg;
    cfg.revision = 3;
    cfg.plate[0] = {rom(1), true};
    cfg.plate[1] = {rom(2), true};
    cfg.plate[2] = {rom(3), true};
    cfg.tankRomId = rom(4);
    cfg.heatingTargetC = 25;
    cfg.coolingTargetC = 30;
    cfg.heatingEnabled = cfg.coolingEnabled = cfg.evaporationEnabled = true;
    cfg.heaterResistanceOhm = 4;
    cfg.heaterInputCurrentLimitA = 2.5f;
    cfg.vinValidMinV = 20;
    cfg.vinValidMaxV = 28;
    cfg.vinCalibrationConfirmed = true;
    return cfg;
}

Sample measured(float value, uint64_t nowUs, uint32_t sequence = 1) { return {value, nowUs, sequence, true, 0}; }

Inputs readings(const Config& cfg, uint64_t nowUs, uint32_t sequence = 1, float p1 = 25, float p2 = 27, float p3 = 28) {
    Inputs in;
    in.configRevision = in.platePendingRevision = cfg.revision;
    in.plateSequence = sequence;
    in.plate[0] = measured(p1, nowUs, sequence);
    in.plate[1] = measured(p2, nowUs, sequence);
    in.plate[2] = measured(p3, nowUs, sequence);
    in.room = {measured(21, nowUs), measured(44, nowUs), {}};
    in.outside = {measured(20, nowUs), measured(50, nowUs), measured(101325, nowUs)};
    in.stall = {measured(22, nowUs), measured(55, nowUs), measured(100801, nowUs)};
    in.tank = measured(22, nowUs);
    in.vin = measured(24, nowUs);
    return in;
}

void parametersAndPlate() {
    Config cfg;
    CHECK(validateConfig(cfg, unset) == nullptr);
    CHECK(!findParameter("MAX_PLATE_TEMP_C") && !findParameter("max_plate_temp_c"));
    CHECK(!findParameter("unbekannt"));
    const auto* r = findParameter("heater_resistance_ohm");
    CHECK(r && r->allowUnset && std::isnan(readNumber(cfg, *r)));
    CHECK(!writeNumber(cfg, *r, 0) && !writeNumber(cfg, *r, INFINITY));
    CHECK(writeNumber(cfg, *r, 4.31) && near(readNumber(cfg, *r), 4.31));
    CHECK(writeNumber(cfg, *r, 0.1) && !writeNumber(cfg, *r, 0.099));
    CHECK(!writeNumber(cfg, *findParameter("manual_timeout_s"), 60.5));
    CHECK(!writeNumber(cfg, *findParameter("cooling_hysteresis_k"), 0));
    CHECK(!writeNumber(cfg, *findParameter("cooling_hysteresis_k"), 20.00000001));
    CHECK(!writeNumber(cfg, *findParameter("heating_enabled"), 2));
    CHECK(!writeText(cfg, *findParameter("wifi_ssid"), "Fehler\n"));
    cfg = configured();
    CHECK(validateConfig(cfg, fixedLimitC) == nullptr);
    CHECK(validateConfig(cfg, unset) != nullptr);
    cfg.vinCalibrationConfirmed = false;
    CHECK(!vinConfigured(cfg) && validateConfig(cfg, fixedLimitC));
    cfg.vinCalibrationConfirmed = true;
    cfg.heaterInputCurrentLimitA = 3.1f;
    CHECK(validateConfig(cfg, fixedLimitC));
    cfg.heaterInputCurrentLimitA = 2.5f;
    cfg.plate[1].romId = cfg.plate[0].romId;
    CHECK(validateConfig(cfg, fixedLimitC));
    cfg.plate[1].romId = 0;
    CHECK(validateConfig(cfg, fixedLimitC));
    cfg.plate[1].enabled = false;
    CHECK(validateConfig(cfg, fixedLimitC) == nullptr);
    cfg.heatingTargetC = cfg.coolingTargetC;
    CHECK(validateConfig(cfg, fixedLimitC));
    cfg.heatingTargetC = 25;
    std::strcpy(cfg.wifiSsid, "klima");
    std::strcpy(cfg.wifiIp, "192.168.1.20");
    std::strcpy(cfg.wifiMask, "255.255.255.0");
    std::strcpy(cfg.wifiGateway, "192.168.2.1");
    std::strcpy(cfg.wifiDns, "192.168.1.1");
    CHECK(validateConfig(cfg, fixedLimitC));
    std::strcpy(cfg.wifiGateway, "192.168.1.1");
    CHECK(validateConfig(cfg, fixedLimitC) == nullptr);

    cfg = configured();
    auto in = readings(cfg, usPerS, 1, 30, 20, 10);
    auto stats = plateStatistics(cfg, in, usPerS);
    CHECK(stats.complete && stats.enabledCount == 3 && stats.enabledMask == 7);
    CHECK(stats.medianC == 20 && stats.meanC == 20 && stats.minC == 10 && stats.maxC == 30 && stats.spreadK == 20);
    in.plate[1].value = 31;
    stats = plateStatistics(cfg, in, usPerS);
    CHECK(stats.medianC == 30);
    in.plate[1].valid = false;
    stats = plateStatistics(cfg, in, usPerS);
    CHECK(!stats.complete && stats.enabledCount == 3 && std::isnan(stats.medianC));
    cfg.plate[1].enabled = false;
    stats = plateStatistics(cfg, in, usPerS);
    CHECK(stats.complete && stats.enabledCount == 2 && stats.enabledMask == 5 && stats.medianC == 20);
    CHECK(plateStatistics(cfg, in, 7 * usPerS).complete);
    CHECK(!plateStatistics(cfg, in, 7 * usPerS + 1).complete);
    in.plate[0].sequence = 2;
    CHECK(!plateStatistics(cfg, in, usPerS).complete);
    for (auto& p : cfg.plate) p.enabled = false;
    CHECK(!plateStatistics(cfg, in, usPerS).complete && !plateStatistics(cfg, in, usPerS).enabledCount);
    for (std::size_t i = 0; i < plateCount; ++i) {
        cfg.plate[i] = {rom(i + 10), true};
        in.plate[i] = measured(static_cast<float>(20 - static_cast<int>(i)), usPerS, in.plateSequence);
        stats = plateStatistics(cfg, in, usPerS);
        CHECK(stats.complete && stats.enabledCount == i + 1 && stats.minC == 20 - i &&
              stats.medianC == 20 - 0.5f * i && stats.maxC == 20);
    }
}

void powerAndPi() {
    auto cfg = configured();
    auto in = readings(cfg, usPerS);
    const auto limit = heaterDutyLimit(cfg, in, usPerS);
    CHECK(near(limit, 2.5 / 6));
    CHECK(quantizeHeaterDuty(limit) == 426);
    const auto duty = heaterDutyFromCount(quantizeHeaterDuty(limit));
    const auto power = estimatePower(cfg, in, usPerS, duty);
    CHECK(power.valid && power.onCurrentA == 6 && power.averageCurrentA <= 2.5f &&
          power.powerW < 60 && near(power.limitPowerW, 60));
    in.vin.value = 26;
    CHECK(heaterDutyLimit(cfg, in, usPerS) < limit);
    in.vin.value = 0;
    CHECK(heaterDutyLimit(cfg, in, usPerS) == 0);
    in.vin.value = 24;
    CHECK(heaterDutyLimit(cfg, in, 2 * usPerS + 1) == 0);
    cfg.heaterResistanceOhm = unset;
    CHECK(heaterDutyLimit(cfg, in, usPerS) == 0);
    CHECK(quantizeHeaterDuty(1) == 1024 && heaterDutyFromCount(1024) == 1);
    CHECK(quantizeHeaterDuty(std::nextafter(1.0f, 0.0f)) == 1022);
    CHECK(quantizeHeaterDuty(0.5f) == 512 && quantizeHeaterDuty(-1) == 0 && quantizeHeaterDuty(unset) == 0);
    for (unsigned i = 0; i < 1001; ++i) {
        const float target = static_cast<float>(i) / 1000;
        CHECK(heaterDutyFromCount(quantizeHeaterDuty(target)) <= target);
    }

    cfg.piKpDutyPerK = 0.1f;
    cfg.piTiS = 10;
    PIHeatingController pi;
    bool overrun = false;
    pi.reset(0, 1);
    CHECK(pi.demand(cfg, 20, 4 * usPerS, 2, overrun) == 0);
    CHECK(near(pi.demand(cfg, 20, 5 * usPerS, 2, overrun), 0.5) && !overrun);
    pi.integrate(cfg, 0.4f);
    CHECK(pi.integralDuty() == 0);
    CHECK(near(pi.demand(cfg, 24, 10 * usPerS, 3, overrun), 0.1));
    pi.integrate(cfg, 0.4f);
    CHECK(near(pi.integralDuty(), 0.05));
    CHECK(near(pi.demand(cfg, 25, 15 * usPerS, 4, overrun), 0.05));
    CHECK(!overrun);
    CHECK(pi.demand(cfg, 25, 25 * usPerS, 5, overrun) == 0 && overrun && pi.integralDuty() == 0);
    pi.reset(29 * usPerS, 5);
    CHECK(pi.demand(cfg, 23, 34 * usPerS, 5, overrun) == 0);
    CHECK(near(pi.demand(cfg, 23, 34 * usPerS + 1000, 6, overrun), 0.2));
    cfg.piIntegralEnabled = false;
    pi.integrate(cfg, 0.4f);
    CHECK(pi.integralDuty() == 0);
}

void tripsAndManual() {
    auto cfg = configured();
    const uint64_t now = 7 * usPerS;
    Controller ctl;
    RuntimeFlags flags;
    Inputs boot;
    auto res = ctl.evaluate(cfg, boot, 1, fixedLimitC, flags, OperatingMode::Auto);
    CHECK(res.mode == OperatingMode::Initializing && res.faults.latchedMask == 0 && !res.safety.mainAllowed);

    auto in = readings(cfg, now);
    res = ctl.evaluate(cfg, in, now, unset, flags, OperatingMode::Manual, {0.2f});
    CHECK(res.authorized.heaterDuty == 0 && (res.safety.inhibitReasons & InhibitPlateLimitUnset));
    res = ctl.evaluate(cfg, in, now, fixedLimitC, flags, OperatingMode::Manual, {0.2f});
    CHECK(res.safety.heaterAllowed && res.authorized.heaterDuty > 0);
    in.platePending[0] = measured(26, now, 2);
    res = ctl.evaluate(cfg, in, now, fixedLimitC, flags, OperatingMode::Manual, {0.2f});
    CHECK(res.plate.complete && res.authorized.heaterDuty > 0);
    in.platePending[0].value = fixedLimitC;
    res = ctl.evaluate(cfg, in, now, fixedLimitC, flags, OperatingMode::Manual, {0.2f});
    CHECK(res.mode == OperatingMode::Fault && res.authorized.heaterDuty == 0 &&
          res.safety.overtempTriggerC == fixedLimitC && (res.faults.latchedMask & faultBit(FaultCode::PlateOvertemp)));
    CHECK(res.plate.medianC < fixedLimitC);
    in.platePending[0].value = fixedLimitC - 1;
    CHECK(!ctl.acknowledge(cfg, in, now, fixedLimitC, flags, FaultCode::PlateOvertemp));
    in.platePending[0].value = fixedLimitC - cfg.overtempReleaseHysteresisK;
    CHECK(ctl.acknowledge(cfg, in, now, fixedLimitC, flags, FaultCode::PlateOvertemp));
    in.platePending[0].valid = false;
    in.platePending[0].errorCode = 1;
    res = ctl.evaluate(cfg, in, now, fixedLimitC, flags, OperatingMode::Manual, {.0f, .0f, true, true});
    CHECK(!res.plate.complete && std::isnan(res.plate.medianC) && !res.authorized.cooling && !res.authorized.evaporation);
    CHECK(res.faults.latchedMask & faultBit(FaultCode::PlateSensorInvalid));
    CHECK(!ctl.acknowledge(cfg, in, now, fixedLimitC, flags, FaultCode::PlateSensorInvalid));
    cfg.plate[0].enabled = false;
    ++cfg.revision;
    CHECK(!ctl.acknowledge(cfg, in, now, fixedLimitC, flags, FaultCode::PlateSensorInvalid));
    in = readings(cfg, now + 1, 2);
    CHECK(ctl.acknowledge(cfg, in, now + 1, fixedLimitC, flags, FaultCode::PlateSensorInvalid));
    res = ctl.evaluate(cfg, in, now + 1, fixedLimitC, flags, OperatingMode::Manual, {0.2f, 0, true});
    CHECK(res.safety.mainAllowed && !res.authorized.cooling && res.authorized.heaterDuty == 0);
    CHECK(res.faults.activeMask & faultBit(FaultCode::PlateSensorDisabled));
    CHECK(ctl.faults().size() > 4);
    CHECK(res.safety.inhibitReasons & InhibitInterlock);
    res = ctl.evaluate(cfg, in, now + 1, fixedLimitC, flags, OperatingMode::Identification, {0.9f});
    CHECK(res.authorized.heaterDuty == 0 && (res.safety.inhibitReasons & InhibitHeatingLimit));

    ManualSession manual;
    CHECK(manual.start(2, now, 59, {}, res.safety));
    CHECK(manual.start(2, now, 60, {0.9f}, res.safety));
    CHECK(manual.start(2, now, 60, {0.2f, 0, true}, res.safety));
    CHECK(!manual.start(2, now, 60, {0.2f}, res.safety));
    CHECK(manual.active() && manual.id() == 2 && manual.deadlineUs() == now + 60 * usPerS);
    CHECK(manual.update(3, {}, res.safety) && manual.request().heaterDuty == 0.2f);
    CHECK(manual.update(2, {0.2f, 0, false, true}, res.safety) && manual.request().heaterDuty == 0.2f);
    CHECK(manual.update(2, {}, res.safety, now, 45) && manual.request().heaterDuty == 0.2f);
    CHECK(!manual.update(2, {}, res.safety, now + 30 * usPerS, 60));
    CHECK(manual.request().heaterDuty == 0 && manual.deadlineUs() == now + 90 * usPerS);
    CHECK(manual.update(2, {}, res.safety, now + 90 * usPerS, 60) != nullptr);
    CHECK(!manual.expire(now + 90 * usPerS - 1) && manual.expire(now + 90 * usPerS));
    CHECK(!manual.active() && manual.request().heaterDuty == 0);

    cfg.heatingEnabled = false;
    res = ctl.evaluate(cfg, in, now + 1, fixedLimitC, flags, OperatingMode::Manual, {0.2f});
    CHECK(res.authorized.heaterDuty == 0);
    in.vin.value = 29;
    res = ctl.evaluate(cfg, in, now + 1, fixedLimitC, flags, OperatingMode::Manual, {0, 0, true, true});
    CHECK(res.mode == OperatingMode::Fault && !res.authorized.cooling && !res.authorized.evaporation);
    in.vin.value = 24;
    CHECK(ctl.acknowledge(cfg, in, now + 1, fixedLimitC, flags, FaultCode::VinOutOfRange));
    in.vin.valid = false;
    res = ctl.evaluate(cfg, in, now + 1, fixedLimitC, flags, OperatingMode::Manual, {0, 0, true});
    CHECK(res.authorized.cooling && !res.safety.heaterAllowed);
    in.vin.valid = true;
    CHECK(ctl.acknowledge(cfg, in, now + 1, fixedLimitC, flags, FaultCode::VinInvalid));
    for (auto& seat : cfg.plate) seat.enabled = false;
    res = ctl.evaluate(cfg, in, now + 1, fixedLimitC, flags, OperatingMode::Manual, {0, 0, true});
    CHECK(!res.safety.mainAllowed && res.faults.topCode == FaultCode::NoActivePlateSensor);

    FaultManager log;
    for (uint64_t i = 0; i < 120; ++i) log.observe(FaultCode::WifiOffline, i % 2 == 0, i);
    CHECK(log.size() == 100 && log.at(0).timeUs == 20 && log.at(99).timeUs == 119);
    CHECK(log.status(FaultCode::WifiOffline).count == 60 && !log.summary().warningFlag);
}

void climateAndPumps() {
    CHECK(near(dewpointC(25, 80), 21.31, 0.04));
    CHECK(std::isnan(dewpointC(20, 0)) && std::isnan(dewpointC(20, 101)));
    CHECK(near(wetbulbC(20, 50, 101325), 13.78, 0.04));
    CHECK(near(wetbulbC(-10, 70, 101325), -10.93, 0.10));
    CHECK(wetbulbC(26, 100, 101325) == 26 && std::isfinite(wetbulbC(30, 0, 101325)));
    CHECK(std::isnan(wetbulbC(30, -1, 101325)) && std::isnan(wetbulbC(30, 50, 0)));

    auto cfg = configured();
    constexpr uint64_t now = usPerS;
    auto in = readings(cfg, now, 1, 26, 27, 31);
    in.outside.humidity.value = 100;
    in.tank.value = 22;
    RuntimeFlags flags;
    Controller ctl;
    auto step = [&]() { return ctl.evaluate(cfg, in, now, fixedLimitC, flags, OperatingMode::Auto); };
    auto res = step();
    CHECK(res.climate.dewpointSource == DewpointSource::Room && !res.authorized.cooling && res.authorized.evaporation);
    in.plate[2].value = 31.1f;
    CHECK(step().authorized.cooling);
    in.plate[2].value = 31;
    CHECK(step().authorized.cooling);
    in.plate[2].value = 30;
    CHECK(!step().authorized.cooling);
    in.plate[2].value = 32;
    in.tank.value = 21.75f;
    CHECK(step().authorized.evaporation);
    in.tank.value = 21.5f;
    CHECK(!step().authorized.evaporation);
    in.tank.value = 22;
    CHECK(step().authorized.evaporation);
    in.outside.humidity.valid = false;
    CHECK(!step().authorized.evaporation);
    in.room.humidity.valid = false;
    CHECK(step().climate.dewpointSource == DewpointSource::Fixed23 && step().authorized.cooling);
    in.outside.humidity.valid = true;
    in.outside.pressure.valid = false;
    CHECK(step().climate.dewpointSource == DewpointSource::Outside && step().climate.pressureFallback);
    in.outside.temperature.value = 14.99f;
    CHECK(!step().authorized.cooling);
    in.outside.temperature.value = 15;
    CHECK(step().authorized.cooling);
    in.outside.temperature.valid = false;
    in.tank.valid = false;
    res = step();
    CHECK(res.authorized.cooling && !res.authorized.evaporation && !res.safety.heaterAllowed);
    in.outside.temperature.valid = true;
    in.tank.valid = true;
    in.tank.value = 25;
    CHECK(step().authorized.cooling);
    in.tank.value = 25.1f;
    CHECK(!step().authorized.cooling);
    in.tank.value = 22;
    in.room.humidity.valid = true;
    in.room.temperature.value = 24;
    in.room.humidity.value = 100;
    CHECK(!step().authorized.cooling);
    res = ctl.evaluate(cfg, in, now, fixedLimitC, flags, OperatingMode::Manual, {0, 0, true, false, true});
    CHECK(res.authorized.cooling);
    cfg.coolingEnabled = false;
    res = ctl.evaluate(cfg, in, now, fixedLimitC, flags, OperatingMode::Manual, {0, 0, true, false, true});
    CHECK(!res.authorized.cooling);
}

void automaticAndDriver() {
    auto cfg = configured();
    cfg.driverHeaterEnabled = true;
    cfg.driverHeaterOnTempC = 16;
    cfg.driverHeaterSlopePctPerK = 10;
    cfg.driverWarmupS = 5;
    auto in = readings(cfg, usPerS, 1, 20, 22, 24);
    in.outside.temperature.value = 16;
    RuntimeFlags flags;
    Controller ctl;
    auto eval = [&](uint64_t now, OperatingMode mode = OperatingMode::Auto) {
        return ctl.evaluate(cfg, in, now, fixedLimitC, flags, mode);
    };
    CHECK(eval(usPerS).authorized.driverDuty == 0);
    in.outside.temperature.value = 10;
    auto res = eval(usPerS);
    CHECK(near(res.authorized.driverDuty, 0.6) && !res.safety.heaterAllowed && near(res.safety.warmupRemainingS, 5));
    in = readings(cfg, 6 * usPerS, 2, 20, 22, 24);
    in.outside.temperature.value = 10;
    res = eval(6 * usPerS);
    CHECK(res.safety.heaterAllowed && res.safety.warmupRemainingS == 0);
    in = readings(cfg, 11 * usPerS, 3, 20, 22, 24);
    in.outside.temperature.value = 10;
    res = eval(11 * usPerS);
    CHECK(res.authorized.heaterDuty > 0 && !res.authorized.evaporation);
    in.outside.temperature.value = 20;
    in.tank.value = 18;
    in.plate[2].value = 32;
    res = eval(11 * usPerS);
    CHECK(res.authorized.heaterDuty == 0 && res.authorized.cooling);
    in.outside.temperature.value = -4;
    in.outside.humidity.valid = false;
    res = eval(11 * usPerS, OperatingMode::Identification);
    CHECK(res.authorized.driverDuty == 1 && !res.safety.heaterAllowed && !res.authorized.evaporation);
    in.outside.temperature.valid = false;
    res = eval(11 * usPerS);
    CHECK(res.authorized.driverDuty == 0 && !res.safety.heaterAllowed);
    in.outside.temperature.valid = true;
    in.outside.temperature.value = 10;
    res = eval(11 * usPerS);
    CHECK(near(res.safety.warmupRemainingS, 5));
    flags.controlOverrun = true;
    res = eval(11 * usPerS);
    CHECK(res.mode == OperatingMode::Fault && res.authorized.driverDuty == 0);
}

void historianAndEnergy() {
    const auto cfg = configured();
    History history;
    CHECK(history.size() == 0);
    for (std::size_t n = 0; n < 146; ++n) {
        const auto now = n * History::intervalUs;
        auto in = readings(cfg, now);
        in.tank.valid = false;
        CHECK(history.update(now, cfg, in, plateStatistics(cfg, in, now)));
        CHECK(!history.update(now + 1000, cfg, in, {}));
    }
    CHECK(history.size() == 145 && history.at(0).timeUs == History::intervalUs &&
          history.at(144).timeUs == 145 * History::intervalUs);
    CHECK(history.at(144).validMask == 0x1b && history.at(144).temperatureCenti[3] == 2667 &&
          history.at(144).temperatureCenti[4] == 2700);

    EnergyCounter energy;
    energy.restore(100);
    energy.update(0, 36, true);
    energy.update(10 * usPerS, 60, true);
    CHECK(energy.microWh() == 100100);
    energy.update(25 * usPerS, 0, false);
    CHECK(energy.microWh() == 350100 && !energy.incomplete());
    energy.update(30 * usPerS, 60, true);
    energy.update(31 * usPerS, unset, false);
    CHECK(energy.microWh() == 350100 && energy.incomplete());
    energy.restore(0);
    energy.update(0, 1, true);
    for (uint64_t n = 1; n <= 3600; ++n) energy.update(n * 10, 1, true);
    CHECK(energy.microWh() == 10);
}
}  // namespace

int main() {
    parametersAndPlate();
    powerAndPi();
    tripsAndManual();
    climateAndPumps();
    automaticAndDriver();
    historianAndEnergy();
    std::printf("CONTROL: %u PASS, %u FAIL\n", passed, failed);
    return failed ? 1 : 0;
}
