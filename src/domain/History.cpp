#include "History.h"

#include <cmath>

namespace klima {
bool History::update(uint64_t nowUs, const Config& cfg, const Inputs& in, const PlateStatistics& plate) {
    if (nowUs < nextCaptureUs_) return false;
    // Retain the 10-minute schedule. After a delay, store one actual value at
    // its real time and do not manufacture values for any missed intervals.
    if (size_ == 0) nextCaptureUs_ = nowUs;
    nextCaptureUs_ += ((nowUs - nextCaptureUs_) / intervalUs + 1) * intervalUs;

    HistoryPoint p;
    p.timeUs = nowUs;
    auto populate = [&](HistoryChannel channel, float value) {
        if (!std::isfinite(value) || value < minDsC || value > maxDsC) return;
        const auto index = static_cast<std::size_t>(channel);
        p.temperatureCenti[index] = static_cast<int16_t>(std::lround(value * 100));
        p.validMask |= static_cast<uint8_t>(1u << index);
    };
    auto insertFresh = [&](HistoryChannel channel, const Sample& sample) {
        if (freshTemperature(sample, nowUs, cfg.sensorMaxAgeMs)) populate(channel, sample.value);
    };
    insertFresh(HistoryChannel::Room, in.room.temperature);
    insertFresh(HistoryChannel::Outside, in.outside.temperature);
    insertFresh(HistoryChannel::Tank, in.tank);
    if (plate.complete) {
        populate(HistoryChannel::PlateMean, plate.meanC);
        populate(HistoryChannel::PlateMedian, plate.medianC);
    }
    points_[next_] = p;
    next_ = (next_ + 1) % capacity;
    if (size_ < capacity) ++size_;
    return true;
}

const HistoryPoint& History::at(std::size_t index) const {
    static constexpr HistoryPoint empty{};
    return index < size_ ? points_[(next_ + capacity - size_ + index) % capacity] : empty;
}

void EnergyCounter::restore(uint64_t energyMicroWh) {
    *this = {};
    totalMicroWh_ = energyMicroWh;
}

void EnergyCounter::update(uint64_t nowUs, float estimatedW, bool heaterActive) {
    if (initialized_ && previousActive_) {
        if (nowUs < lastUs_ || !std::isfinite(previousW_) || previousW_ < 0 ||
            !std::isfinite(estimatedW) || estimatedW < 0) {
            incomplete_ = true;
        } else {
            // W × µs / 3600 = µWh. The old power applies up to this update.
            const auto wattUs = static_cast<double>(previousW_) * static_cast<double>(nowUs - lastUs_) +
                remainingWattUs_;
            const auto whole = std::floor(wattUs / 3600);
            if (whole >= static_cast<double>(UINT64_MAX - totalMicroWh_)) {
                totalMicroWh_ = UINT64_MAX;
                incomplete_ = true;
            } else {
                totalMicroWh_ += static_cast<uint64_t>(whole);
                remainingWattUs_ = wattUs - whole * 3600;
            }
        }
    }
    if (heaterActive && (!std::isfinite(estimatedW) || estimatedW < 0)) incomplete_ = true;
    initialized_ = true;
    lastUs_ = nowUs;
    previousW_ = estimatedW;
    previousActive_ = heaterActive;
}
}  // namespace klima
