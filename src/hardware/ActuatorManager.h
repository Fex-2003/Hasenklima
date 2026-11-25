#pragma once

#include <cstdint>
#include "domain/Model.h"

namespace klima {
class ActuatorManager {
public:
    bool begin();
    OutputState apply(OutputRequest permitted, uint64_t nowUs);
    bool healthy() const { return ready_; }

private:
    bool setHeater(uint16_t counts);
    void shutdown();
    OutputState state_{};
    uint64_t pumpMayStartUs_ = 0;
    bool ready_ = false;
};
}
