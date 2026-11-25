#pragma once

#include "domain/Config.h"
#include "domain/Model.h"
#include "network/NetworkContract.h"
#include <OneWire.h>

namespace klima {
class SensorManager {
public:
    void begin();
    bool tick(const Config& config, uint64_t nowUs);
    void startScan();
    bool scanning() const { return scanBus_ < 2; }
    const NetScan& scan() const { return scan_; }
    const Inputs& inputs() const { return inputs_; }

private:
    struct Bme {
        uint16_t t1{}, p1{};
        int16_t t2{}, t3{}, p2{}, p3{}, p4{}, p5{}, p6{}, p7{}, p8{}, p9{};
        uint8_t h1{}, h3{};
        int16_t h2{}, h4{}, h5{};
        int8_t h6{};
        bool configured{}, triggered{};
        bool trigger(uint8_t address);
        ClimateSamples read(uint8_t address, uint64_t now, uint32_t seq);
    };
    void stepScan();
    OneWire plateBus_, tankBus_;
    Bme outside_, stall_;
    Inputs inputs_{};
    NetScan scan_{};
    uint64_t nextConversionUs_{}, fetchDsUs_{}, fetchClimateUs_{}, nextVinUs_{};
    uint32_t sequence_{}, cycleRevision_{}, vinSequence_{};
    unsigned plateIndex_ = 0, scanBus_ = 2;
    unsigned scanExamined_ = 0;
    bool plateStarted_{}, tankStarted_{}, roomStarted_{}, wireReady_{};
};
}
