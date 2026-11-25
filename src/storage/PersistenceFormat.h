#pragma once

#include "../domain/Config.h"

namespace klima {

using ConfigBlob = std::array<uint8_t, 2048>;
using EnergyBlob = std::array<uint8_t, 32>;

struct EnergyCheckpoint {
    uint64_t microWh = 0;
    uint64_t installationId = 0;
    uint32_t sequence = 0;
};

// A return of zero means that nothing may be persisted.
size_t encodeConfig(const Config& config, ConfigBlob& output);
bool decodeConfig(const uint8_t* bytes, size_t size, float plateLimitC, Config& config);
EnergyBlob encodeEnergy(const EnergyCheckpoint& checkpoint);
bool decodeEnergy(const EnergyBlob& bytes, EnergyCheckpoint& checkpoint);

} // namespace klima
