#include "PersistentStore.h"

#include <esp_system.h>

namespace klima {
namespace {
constexpr const char* kConfigSlots[] = {"cfg_a", "cfg_b"};
constexpr const char* kEnergySlots[] = {"energy_a", "energy_b"};

void classify(esp_err_t error, bool& invalid, bool& io) {
    if (error == ESP_ERR_NVS_TYPE_MISMATCH || error == ESP_ERR_NVS_INVALID_LENGTH) invalid = true;
    else if (error != ESP_ERR_NVS_NOT_FOUND) io = true;
}
} // namespace

bool PersistentStore::begin() {
    if (!configNvs_ && nvs_open("klima_config", NVS_READWRITE, &configNvs_) != ESP_OK) return false;
    const bool ready = energyNvs_ || nvs_open("klima_energy", NVS_READWRITE, &energyNvs_) == ESP_OK;
    if (!bootCount_) {
        uint32_t old = 0;
        const esp_err_t read = nvs_get_u32(configNvs_, "boot_count", &old);
        if (read == ESP_OK || read == ESP_ERR_NVS_NOT_FOUND) {
            bootCount_ = old == UINT32_MAX ? UINT32_MAX : old + 1;
            if (bootCount_ != old && (nvs_set_u32(configNvs_, "boot_count", bootCount_) != ESP_OK ||
                                     nvs_commit(configNvs_) != ESP_OK)) bootCount_ = 0;
        }
    }
    return ready;
}

LoadResult PersistentStore::loadConfig(Config& config, float plateLimitC) {
    if (!configNvs_) return LoadResult::Io;
    ConfigBlob bytes;
    Config candidate;
    bool invalid = false, io = false;
    configSlot_ = -1;
    configRevision_ = 0;
    for (int i = 0; i < 2; ++i) {
        size_t size = bytes.size();
        const auto error = nvs_get_blob(configNvs_, kConfigSlots[i], bytes.data(), &size);
        if (error != ESP_OK) {
            classify(error, invalid, io);
            continue;
        }
        if (!decodeConfig(bytes.data(), size, plateLimitC, candidate)) {
            invalid = true;
            continue;
        }
        if (configSlot_ < 0 || candidate.revision > configRevision_) {
            configSlot_ = i;
            configRevision_ = candidate.revision;
            config = candidate;
        }
    }
    if (configSlot_ >= 0) return LoadResult::Valid;
    return io ? LoadResult::Io : (invalid ? LoadResult::Invalid : LoadResult::Missing);
}

bool PersistentStore::saveConfig(const Config& config) {
    if (!configNvs_ || (configSlot_ >= 0 && config.revision <= configRevision_)) return false;
    ConfigBlob bytes;
    const size_t size = encodeConfig(config, bytes);
    const int next = configSlot_ == 0 ? 1 : 0;
    if (!size || nvs_set_blob(configNvs_, kConfigSlots[next], bytes.data(), size) != ESP_OK ||
        nvs_commit(configNvs_) != ESP_OK) return false;
    configSlot_ = next;
    configRevision_ = config.revision;
    return true;
}

LoadResult PersistentStore::loadEnergy(EnergyCheckpoint& checkpoint) {
    if (!energyNvs_) return LoadResult::Io;
    EnergyBlob bytes;
    EnergyCheckpoint candidate;
    bool invalid = false, io = false;
    energySlot_ = -1;
    energyReadBlocked_ = false;
    energy_ = {};
    for (int i = 0; i < 2; ++i) {
        size_t size = bytes.size();
        const auto error = nvs_get_blob(energyNvs_, kEnergySlots[i], bytes.data(), &size);
        if (error != ESP_OK) {
            classify(error, invalid, io);
            continue;
        }
        if (size != bytes.size() || !decodeEnergy(bytes, candidate)) {
            invalid = true;
            continue;
        }
        if (energySlot_ < 0 || candidate.sequence > energy_.sequence) {
            energySlot_ = i;
            energy_ = candidate;
        }
    }
    if (energySlot_ >= 0) {
        checkpoint = energy_;
        return LoadResult::Valid;
    }
    if (io || invalid) {
        energyReadBlocked_ = true;
        return io ? LoadResult::Io : LoadResult::Invalid;
    }
    checkpoint = {};
    return LoadResult::Missing;
}

bool PersistentStore::saveEnergy(uint64_t microWh) {
    return writeEnergy(microWh, false);
}

bool PersistentStore::resetEnergy() {
    return writeEnergy(0, true);
}

bool PersistentStore::writeEnergy(uint64_t microWh, bool reset) {
    if (!energyNvs_ || energy_.sequence == UINT32_MAX || (!reset && (energyReadBlocked_ || microWh < energy_.microWh))) return false;
    if (!reset && energySlot_ >= 0 && microWh == energy_.microWh) return true;
    EnergyCheckpoint candidate{microWh, energy_.installationId, energy_.sequence + 1};
    if (reset || !candidate.installationId) {
        candidate.installationId = (static_cast<uint64_t>(esp_random()) << 32) | esp_random();
        if (!candidate.installationId) candidate.installationId = 1;
    }
    const auto bytes = encodeEnergy(candidate);
    const int next = energySlot_ == 0 ? 1 : 0;
    if (nvs_set_blob(energyNvs_, kEnergySlots[next], bytes.data(), bytes.size()) != ESP_OK ||
        nvs_commit(energyNvs_) != ESP_OK) return false;
    energySlot_ = next;
    energy_ = candidate;
    energyReadBlocked_ = false;
    return true;
}

} // namespace klima
