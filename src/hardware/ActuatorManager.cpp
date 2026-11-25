#include "ActuatorManager.h"
#include "BoardConfig.h"
#include "HardwareLimits.h"
#include "domain/Control.h"
#include <Arduino.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <algorithm>
#include <cmath>

namespace klima {
namespace {
constexpr unsigned fullCount = 1U << board::heaterPwmBits;
constexpr uint64_t pwmPeriodUs = 1000000U / board::heaterPwmHz;
constexpr gpio_num_t heaterGpio = static_cast<gpio_num_t>(board::heaterPin);

void setPin(int pin, bool on) {
    gpio_set_level(static_cast<gpio_num_t>(pin), on);
}
}

bool ActuatorManager::begin() {
    uint64_t pins = 0;
    for (int pin : {board::heaterPin, board::coolingPin, board::evaporationPin,
                    board::driverHeaterPin}) {
        setPin(pin, false);
        pins |= uint64_t{1} << pin;
    }
    const gpio_config_t io{pins, GPIO_MODE_OUTPUT, GPIO_PULLUP_DISABLE,
                           GPIO_PULLDOWN_DISABLE, GPIO_INTR_DISABLE};
    if (gpio_config(&io) != ESP_OK ||
        ledcSetup(board::heaterPwmChannel, board::heaterPwmHz, board::heaterPwmBits) == 0) {
        return false;
    }
    ledc_channel_config_t channel{};
    channel.gpio_num = board::heaterPin;
    channel.speed_mode = LEDC_HIGH_SPEED_MODE;
    channel.channel = LEDC_CHANNEL_0;
    channel.timer_sel = LEDC_TIMER_0;
    ready_ = ledc_channel_config(&channel) == ESP_OK;
    if (!ready_) shutdown();
    return ready_;
}

bool ActuatorManager::setHeater(uint16_t counts) {
    // The checked IDF calls are the underlying Arduino 2.0.17 LEDC interface.
    // fullCount is the actual counter increment, already normalized by the domain.
    const bool ok = counts == 0
        ? ledc_stop(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0) == ESP_OK
        : ledc_set_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, counts) == ESP_OK &&
          ledc_update_duty(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0) == ESP_OK;
    if (!ok) {
        shutdown();
        return false;
    }
    state_.heaterCount = counts;
    state_.heaterDuty = static_cast<float>(counts) / fullCount;
    return true;
}

void ActuatorManager::shutdown() {
    ledc_stop(LEDC_HIGH_SPEED_MODE, LEDC_CHANNEL_0, 0);
    ledcDetachPin(board::heaterPin);
    gpio_set_direction(heaterGpio, GPIO_MODE_OUTPUT);
    setPin(board::heaterPin, false);
    setPin(board::coolingPin, false);
    setPin(board::evaporationPin, false);
    setPin(board::driverHeaterPin, false);
    ready_ = false;
    state_ = {};
}

OutputState ActuatorManager::apply(OutputRequest permitted, uint64_t nowUs) {
    if (!ready_) return state_;
    const auto count = quantizeHeaterDuty(permitted.heaterDuty);
    const auto driverSteps = std::lround(quantizeDriverDuty(permitted.driverDuty) * driverPwmSteps);
    const bool wantsPump = permitted.cooling || permitted.evaporation;
    // Reject a contradictory final request even if an upstream caller malfunctions.
    const auto targetCount = wantsPump ? 0 : count;
    if (state_.heaterCount && !targetCount) {
        if (!setHeater(0)) return state_;
        pumpMayStartUs_ = nowUs + 2 * pwmPeriodUs;
    }
    const bool allowPumps = !count && nowUs >= pumpMayStartUs_;
    state_.cooling = allowPumps && permitted.cooling;
    state_.evaporation = allowPumps && permitted.evaporation;
    setPin(board::coolingPin, state_.cooling);
    setPin(board::evaporationPin, state_.evaporation);
    if (targetCount != state_.heaterCount && !setHeater(targetCount)) return state_;

    state_.driverDuty = static_cast<float>(driverSteps) / driverPwmSteps;
    setPin(board::driverHeaterPin, nowUs % Q4_WINDOW_US < static_cast<uint64_t>(driverSteps) * CONTROL_TICK_US);
    return state_;
}
}
