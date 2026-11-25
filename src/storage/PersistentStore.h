#pragma once

#include "PersistenceFormat.h"

#include <nvs.h>

namespace klima {

enum class LoadStatus { Missing, Valid, Invalid, Io };
using LoadResult = LoadStatus;

class PersistentStore {
public:
    bool begin();
    LoadResult loadConfig(Config& config, float plateLimitC);
    bool saveConfig(const Config& config);
    LoadResult loadEnergy(EnergyCheckpoint& checkpoint);
    bool saveEnergy(uint64_t microWh);
    bool resetEnergy();
    uint32_t bootCount() const { return bootCount_; }
    EnergyCheckpoint energyCheckpoint() const { return energy_; }

private:
    bool writeEnergy(uint64_t microWh, bool reset);
    nvs_handle_t configNvs_ = 0, energyNvs_ = 0;
    int configSlot_ = -1, energySlot_ = -1;
    uint32_t configRevision_ = 0, bootCount_ = 0;
    bool energyReadBlocked_ = false;
    EnergyCheckpoint energy_;
};

} // namespace klima
