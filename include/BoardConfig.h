#pragma once

#include <cstdint>

namespace klima::board {
inline constexpr int heaterPin = 18;
inline constexpr int coolingPin = 23;
inline constexpr int evaporationPin = 19;
inline constexpr int driverHeaterPin = 33;
inline constexpr int plateBusPin = 13;
inline constexpr int tankBusPin = 14;
inline constexpr int i2cSda = 21;
inline constexpr int i2cScl = 22;
inline constexpr int vinPin = 34;
inline constexpr uint32_t i2cFrequencyHz = 100000;
inline constexpr uint8_t roomAddress = 0x44;
inline constexpr uint8_t outsideAddress = 0x76;
inline constexpr uint8_t stallAddress = 0x77;
inline constexpr uint8_t heaterPwmBits = 10;
inline constexpr uint32_t heaterPwmHz = 20000;
inline constexpr uint8_t heaterPwmChannel = 0;
}
