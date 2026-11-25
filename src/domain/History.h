#pragma once

#include "Model.h"

namespace klima {
struct HistoryPoint {
    uint64_t timeUs = 0;
    std::array<int16_t, 5> temperatureCenti{};
    uint8_t validMask = 0;
    std::array<uint8_t, 5> reserved{};
};
static_assert(sizeof(HistoryPoint) == 24);
enum class HistoryChannel : uint8_t { Room, Outside, Tank, PlateMean, PlateMedian };

class History {
public:
    static constexpr std::size_t capacity = 145;
    static constexpr uint64_t intervalUs = 600 * usPerS;
    bool update(uint64_t nowUs, const Config& cfg, const Inputs& in, const PlateStatistics& plate);
    std::size_t size() const { return size_; }
    const HistoryPoint& at(std::size_t index) const;
private:
    std::array<HistoryPoint, capacity> points_{};
    std::size_t size_ = 0;
    std::size_t next_ = 0;
    uint64_t nextCaptureUs_ = 0;
};

class EnergyCounter {
public:
    void restore(uint64_t energyMicroWh);
    // Complete the previous output segment before starting one with this value.
    // Pass UNSET if VIN/model data is invalid, even when switching off.
    void update(uint64_t nowUs, float estimatedW, bool heaterActive);
    uint64_t microWh() const { return totalMicroWh_; }
    bool incomplete() const { return incomplete_; }
private:
    uint64_t totalMicroWh_ = 0;
    uint64_t lastUs_ = 0;
    float previousW_ = unset;
    double remainingWattUs_ = 0;
    bool previousActive_ = false;
    bool initialized_ = false;
    bool incomplete_ = false;
};
}  // namespace klima
