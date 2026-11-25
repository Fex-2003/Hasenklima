#pragma once

#include <cstdint>
#include <limits>

namespace klima {
#ifdef KLIMA_MAX_PLATE_TEMP_C
inline constexpr float MAX_PLATE_TEMP_C = KLIMA_MAX_PLATE_TEMP_C;
static_assert(MAX_PLATE_TEMP_C > -55.0f && MAX_PLATE_TEMP_C <= 125.0f,
              "MAX_PLATE_TEMP_C must be within the DS18B20 range");
#else
inline constexpr float MAX_PLATE_TEMP_C = std::numeric_limits<float>::quiet_NaN();
#endif
inline constexpr uint64_t CONTROL_TICK_US = 10000;
inline constexpr uint64_t CONTROL_GAP_LIMIT_US = 100000;
inline constexpr uint64_t SENSOR_INTERVAL_US = 2000000;
inline constexpr uint64_t VIN_INTERVAL_US = 100000;
inline constexpr uint64_t DS_CONVERSION_US = 750000;
inline constexpr uint64_t Q4_WINDOW_US = 1000000;
inline constexpr char FIRMWARE_VERSION[] = "1.0.0";
}
