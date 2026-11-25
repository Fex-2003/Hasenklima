#include "SensorManager.h"
#include "BoardConfig.h"
#include "HardwareLimits.h"
#include <Arduino.h>
#include <Wire.h>
#include <algorithm>
#include <cmath>

namespace klima {
namespace {
enum Quality : uint16_t { Good, Unassigned, NoResponse, BadCrc, Conversion, OutOfRange };
constexpr unsigned vinSamples = 16;
constexpr uint64_t climateConversionUs = 12000;

Sample measured(float value, uint64_t now, uint32_t seq,
                float min, float max, Quality error = Good) {
    if (error == Good && (!std::isfinite(value) || value < min || value > max)) error = OutOfRange;
    Sample sample{};
    sample.value = error == Good ? value : NAN;
    sample.sampleTimeUs = now;
    sample.sequence = seq;
    sample.valid = error == Good;
    sample.errorCode = error;
    return sample;
}

bool writeReg(uint8_t address, uint8_t reg, uint8_t value) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    Wire.write(value);
    return Wire.endTransmission() == 0;
}

bool readBytes(uint8_t address, uint8_t* out, uint8_t count) {
    if (Wire.requestFrom(address, count) != count) return false;
    for (unsigned i = 0; i < count; ++i) out[i] = Wire.read();
    return true;
}

bool readReg(uint8_t address, uint8_t reg, uint8_t* out, uint8_t count) {
    Wire.beginTransmission(address);
    Wire.write(reg);
    return Wire.endTransmission(false) == 0 && readBytes(address, out, count);
}

uint16_t le16(const uint8_t* bytes) { return bytes[0] | (bytes[1] << 8); }
int16_t s16(const uint8_t* bytes) { return static_cast<int16_t>(le16(bytes)); }
int16_t s12(unsigned code) { return static_cast<int16_t>(code & 0x800 ? int(code) - 4096 : code); }
unsigned be20(const uint8_t* bytes) { return (bytes[0] << 12) | (bytes[1] << 4) | (bytes[2] >> 4); }
unsigned be16(const uint8_t* bytes) { return (bytes[0] << 8) | bytes[1]; }

ClimateSamples noClimate(uint64_t now, uint32_t seq) {
    const auto missing = measured(NAN, now, seq, 0, 0, NoResponse);
    return {missing, missing, missing};
}

bool shtTrigger() {
    Wire.beginTransmission(board::roomAddress);
    Wire.write(0xfd);
    return Wire.endTransmission() == 0;
}

bool shtCrc(const uint8_t* bytes) {
    uint8_t crc = 0xff;
    for (unsigned i = 0; i < 2; ++i) {
        crc ^= bytes[i];
        for (unsigned bit = 0; bit < 8; ++bit) {
            crc = crc & 0x80 ? (crc << 1) ^ 0x31 : crc << 1;
        }
    }
    return crc == bytes[2];
}

ClimateSamples shtRead(uint64_t now, uint32_t seq, bool triggered) {
    uint8_t bytes[6];
    if (!triggered || !readBytes(board::roomAddress, bytes, sizeof(bytes))) return noClimate(now, seq);
    auto result = noClimate(now, seq);
    result.temperature = measured(-45.0f + 175.0f * be16(bytes) / 65535.0f, now, seq,
                                  -40, 125, shtCrc(bytes) ? Good : BadCrc);
    // Sensirion specifies clamping computed relative humidity to 0…100 %.
    const float rh = std::clamp(-6.0f + 125.0f * be16(bytes + 3) / 65535.0f, 0.0f, 100.0f);
    result.humidity = measured(rh, now, seq, 0, 100, shtCrc(bytes + 3) ? Good : BadCrc);
    return result;
}

bool dsTrigger(OneWire& bus) {
    if (!bus.reset()) return false;
    bus.skip();
    bus.write(0x44);
    return true;
}

Sample dsRead(OneWire& bus, uint64_t rom, bool converted, uint64_t now, uint32_t seq) {
    Quality error = Good;
    float temperature = NAN;
    if (!rom) error = Unassigned;
    else if (!converted) error = Conversion;
    else if (!bus.reset()) error = NoResponse;
    else {
        uint8_t id[8], data[9];
        for (unsigned i = 0; i < 8; ++i) id[i] = rom >> (8 * i);
        bus.select(id);
        bus.write(0xbe);
        bus.read_bytes(data, sizeof(data));
        bool allZero = true;
        for (uint8_t byte : data) allZero &= byte == 0;
        if (allZero) error = NoResponse;
        else if (OneWire::crc8(data, 8) != data[8]) error = BadCrc;
        else {
            temperature = s16(data) / 16.0f;
            if (data[4] != 0x7f && bus.reset()) {
                // Set scratchpad resolution without wearing the DS18B20 EEPROM.
                bus.select(id);
                bus.write(0x4e);
                bus.write(data[2]); bus.write(data[3]); bus.write(0x7f);
            }
        }
    }
    return measured(temperature, now, seq, -55, 125, error);
}
}

bool SensorManager::Bme::trigger(uint8_t address) {
    triggered = false;
    if (!configured) {
        uint8_t id{}, status{}, coefficients[26], humidity[7];
        if (!readReg(address, 0xd0, &id, 1) || id != 0x60 ||
            !readReg(address, 0xf3, &status, 1) || (status & 1) ||
            !readReg(address, 0x88, coefficients, sizeof(coefficients)) ||
            !readReg(address, 0xe1, humidity, sizeof(humidity))) return false;
        t1 = le16(coefficients); t2 = s16(coefficients + 2); t3 = s16(coefficients + 4);
        p1 = le16(coefficients + 6); p2 = s16(coefficients + 8); p3 = s16(coefficients + 10);
        p4 = s16(coefficients + 12); p5 = s16(coefficients + 14); p6 = s16(coefficients + 16);
        p7 = s16(coefficients + 18); p8 = s16(coefficients + 20); p9 = s16(coefficients + 22);
        h1 = coefficients[25]; h2 = s16(humidity); h3 = humidity[2];
        h4 = s12((humidity[3] << 4) | (humidity[4] & 15));
        h5 = s12((humidity[5] << 4) | (humidity[4] >> 4));
        h6 = static_cast<int8_t>(humidity[6]);
        if (!t1 || !p1 || !writeReg(address, 0xf2, 0x01)) return false;
        configured = true;
    }
    // Forced mode, 1× oversampling on all three channels. Read later.
    triggered = writeReg(address, 0xf4, 0x25);
    if (!triggered) configured = false;
    return triggered;
}

ClimateSamples SensorManager::Bme::read(uint8_t address, uint64_t now, uint32_t seq) {
    auto result = noClimate(now, seq);
    uint8_t status{}, raw[8];
    if (!triggered || !readReg(address, 0xf3, &status, 1) || (status & 9) ||
        !readReg(address, 0xf7, raw, sizeof(raw))) {
        configured = false;
        return result;
    }
    const auto t = be20(raw + 3), p = be20(raw), h = be16(raw + 6);
    if (t == 0x80000) { configured = false; return result; }
    // Bosch datasheet, double precision compensation; all channels share t_fine.
    const double delta = t / 131072.0 - t1 / 8192.0;
    const double small = t / 524288.0 - t1 / 16384.0;
    const double fine = delta * t2 + small * small * t3;
    result.temperature = measured(fine / 5120.0, now, seq, -40, 85);
    if (!result.temperature.valid) return result;
    if (h != 0x8000) {
        const double x = fine - 76800.0;
        const double rh = (h - (h4 * 64.0 + h5 / 16384.0 * x)) *
                          (h2 / 65536.0 * (1.0 + h6 / 67108864.0 * x *
                                           (1.0 + h3 / 67108864.0 * x)));
        result.humidity = measured(std::clamp(rh * (1.0 - h1 * rh / 524288.0), 0.0, 100.0),
                                   now, seq, 0, 100);
    }
    if (p != 0x80000) {
        const double x = fine / 2.0 - 64000.0;
        const double y = (x * x * p6 / 32768.0 + x * p5 * 2.0) / 4.0 + p4 * 65536.0;
        const double divisor = (1.0 + (p3 * x * x / 524288.0 + p2 * x) /
                                (524288.0 * 32768.0)) * p1;
        if (divisor > 0) {
            const double pa = (1048576.0 - p - y / 4096.0) * 6250.0 / divisor;
            result.pressure = measured(pa + (p9 * pa * pa / 2147483648.0 +
                                              pa * p8 / 32768.0 + p7) / 16.0,
                                       now, seq, 30000, 110000);
        }
    }
    return result;
}

void SensorManager::begin() {
    plateBus_.begin(board::plateBusPin);
    tankBus_.begin(board::tankBusPin);
    wireReady_ = Wire.begin(board::i2cSda, board::i2cScl, board::i2cFrequencyHz);
    Wire.setTimeOut(12);
    analogReadResolution(12);
    analogSetPinAttenuation(board::vinPin, ADC_11db);
}

void SensorManager::startScan() {
    scan_ = {};
    scanBus_ = 0;
    scanExamined_ = 0;
    plateBus_.reset_search();
    tankBus_.reset_search();
    fetchDsUs_ = 0;
}

void SensorManager::stepScan() {
    auto& bus = scanBus_ ? tankBus_ : plateBus_;
    auto& ids = scanBus_ ? scan_.tank : scan_.plate;
    auto& count = scanBus_ ? scan_.tankCount : scan_.plateCount;
    uint8_t rom[8];
    if (bus.search(rom)) {
        if (++scanExamined_ > ids.size() || count == ids.size()) scan_.overflow = true;
        else if (rom[0] == 0x28 && OneWire::crc8(rom, 7) == rom[7]) {
            uint64_t id = 0;
            for (unsigned i = 0; i < 8; ++i) id |= uint64_t{rom[i]} << (8 * i);
            ids[count++] = id;
        }
        if (scanExamined_ <= ids.size()) return;
    }
    scanExamined_ = 0;
    if (++scanBus_ == 2) {
        scan_.ready = true;
        nextConversionUs_ = 0;
    }
}

bool SensorManager::tick(const Config& config, uint64_t now) {
    bool changed = false;
    if (now >= nextVinUs_) {
        nextVinUs_ = now + VIN_INTERVAL_US;
        uint32_t sumMv = 0;
        bool railed = false;
        for (unsigned i = 0; i < vinSamples; ++i) {
            const auto mv = analogReadMilliVolts(board::vinPin);
            sumMv += mv;
            railed |= mv == 0 || mv >= 3100;
        }
        const float vin = sumMv * config.vinGain / (1000.0f * vinSamples) + config.vinOffsetV;
        inputs_.vin = measured(vin, now, ++vinSequence_, 0.01f, 100.0f, railed ? OutOfRange : Good);
        changed = true;
    }
    if (scanning()) {
        stepScan();
        return changed;
    }
    if (now >= nextConversionUs_) {
        nextConversionUs_ = now + SENSOR_INTERVAL_US;
        cycleRevision_ = config.revision;
        if (++sequence_ == 0) ++sequence_;
        plateStarted_ = dsTrigger(plateBus_);
        tankStarted_ = dsTrigger(tankBus_);
        // Use the later time: no DS read before the last conversion had 750 ms.
        fetchDsUs_ = esp_timer_get_time() + DS_CONVERSION_US;
        plateIndex_ = 0;
        inputs_.platePending = {};
        inputs_.platePendingRevision = cycleRevision_;
        roomStarted_ = wireReady_ && shtTrigger();
        if (wireReady_) {
            outside_.trigger(board::outsideAddress);
            stall_.trigger(board::stallAddress);
        }
        fetchClimateUs_ = esp_timer_get_time() + climateConversionUs;
        changed = true;
    }
    if (fetchClimateUs_ && now >= fetchClimateUs_) {
        inputs_.room = shtRead(now, sequence_, roomStarted_);
        inputs_.outside = outside_.read(board::outsideAddress, now, sequence_);
        inputs_.stall = stall_.read(board::stallAddress, now, sequence_);
        fetchClimateUs_ = 0;
        changed = true;
    }
    if (fetchDsUs_ && now >= fetchDsUs_) {
        if (config.revision != cycleRevision_) {
            fetchDsUs_ = 0;
            inputs_.platePending = {};
            nextConversionUs_ = 0;
            return true;
        }
        if (!plateIndex_) {
            plateStarted_ = plateStarted_ && plateBus_.read_bit();
            tankStarted_ = tankStarted_ && tankBus_.read_bit();
        }
        if (plateIndex_ < inputs_.plate.size()) {
            const auto i = plateIndex_++;
            inputs_.platePending[i] = dsRead(plateBus_, config.plate[i].romId, plateStarted_,
                                             fetchDsUs_, sequence_);
        }
        if (plateIndex_ == inputs_.plate.size()) {
            inputs_.plate = inputs_.platePending;
            inputs_.plateSequence = sequence_;
            inputs_.configRevision = cycleRevision_;
            inputs_.tank = dsRead(tankBus_, config.tankRomId, tankStarted_, fetchDsUs_, sequence_);
            fetchDsUs_ = 0;
        }
        changed = true;
    }
    return changed;
}
}
