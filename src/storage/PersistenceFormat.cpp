#include "PersistenceFormat.h"
#include "RecordFormat.h"

#include <cstring>

namespace klima {
namespace {
constexpr uint32_t kConfigTag = 0x4746434b; // KCFG
constexpr uint32_t kEnergyTag = 0x474e454b; // KENG
constexpr uint32_t kConfigSchema = 1;
constexpr uint32_t kEnergySchema = 1;
constexpr size_t kConfigHeaderBytes = 16;

uint32_t parameterLayout() {
    // Changing the layout without a schema migration must fail the load, even
    // when the old and new parameter lists happen to occupy the same length.
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < parameterCount; ++i) {
        const auto& p = parameters[i];
        if (!p.persistent) continue;
        for (const char* c = p.id; *c; ++c) hash = (hash ^ static_cast<uint8_t>(*c)) * 16777619u;
        hash = (hash ^ static_cast<uint8_t>(p.type)) * 16777619u;
    }
    return hash;
}
} // namespace

size_t encodeConfig(const Config& config, ConfigBlob& bytes) {
    putLe32(bytes.data(), kConfigTag);
    putLe32(bytes.data() + 4, kConfigSchema);
    putLe32(bytes.data() + 8, config.revision);
    putLe32(bytes.data() + 12, parameterLayout());
    size_t pos = kConfigHeaderBytes;
    for (size_t i = 0; i < parameterCount; ++i) {
        const auto& p = parameters[i];
        if (!p.persistent) continue;
        if (p.type == ParameterType::String) {
            const auto first = readText(config, p);
            const auto end = static_cast<const char*>(std::memchr(first, 0, p.maxLength + 1));
            if (!end) return 0;
            const size_t length = static_cast<size_t>(end - first);
            if (length > UINT16_MAX || pos + 2 + length + 4 > bytes.size()) return 0;
            bytes[pos++] = static_cast<uint8_t>(length);
            bytes[pos++] = static_cast<uint8_t>(length >> 8);
            std::memcpy(bytes.data() + pos, first, length);
            pos += length;
        } else {
            if (pos + 8 > bytes.size()) return 0;
            const double value = readNumber(config, p);
            if (p.type == ParameterType::Boolean) bytes[pos++] = value != 0;
            else {
                if (p.type == ParameterType::Float) putLeFloat(bytes.data() + pos, static_cast<float>(value));
                else putLe32(bytes.data() + pos, static_cast<uint32_t>(value));
                pos += 4;
            }
        }
    }
    if (pos + config.plate.size() * 9 + 8 + 4 > bytes.size()) return 0;
    for (const auto& p : config.plate) {
        putLe64(bytes.data() + pos, p.romId);
        pos += 8;
        bytes[pos++] = p.enabled;
    }
    putLe64(bytes.data() + pos, config.tankRomId);
    pos += 8;
    putLe32(bytes.data() + pos, crc32(bytes.data(), pos));
    return pos + 4;
}

bool decodeConfig(const uint8_t* bytes, size_t size, float plateLimitC, Config& config) {
    if (size < kConfigHeaderBytes + 4 || getLe32(bytes) != kConfigTag || getLe32(bytes + 4) != kConfigSchema ||
        getLe32(bytes + 12) != parameterLayout() || getLe32(bytes + size - 4) != crc32(bytes, size - 4)) return false;
    Config candidate;
    candidate.revision = getLe32(bytes + 8);
    size_t pos = kConfigHeaderBytes;
    const size_t limit = size - 4;
    for (size_t i = 0; i < parameterCount; ++i) {
        const auto& p = parameters[i];
        if (!p.persistent) continue;
        if (p.type == ParameterType::String) {
            if (pos + 2 > limit) return false;
            const size_t length = bytes[pos] | (static_cast<size_t>(bytes[pos + 1]) << 8);
            pos += 2;
            if (pos + length > limit || !writeText(candidate, p, {reinterpret_cast<const char*>(bytes + pos), length})) return false;
            pos += length;
        } else if (p.type == ParameterType::Boolean) {
            if (pos == limit || !writeNumber(candidate, p, bytes[pos++])) return false;
        } else {
            if (pos + 4 > limit) return false;
            const double value = p.type == ParameterType::Float ? static_cast<double>(getLeFloat(bytes + pos))
                                                             : static_cast<double>(getLe32(bytes + pos));
            if (!writeNumber(candidate, p, value)) return false;
            pos += 4;
        }
    }
    if (pos + candidate.plate.size() * 9 + 8 != limit) return false;
    for (auto& p : candidate.plate) {
        p.romId = getLe64(bytes + pos);
        pos += 8;
        if (bytes[pos] > 1) return false;
        p.enabled = bytes[pos++];
    }
    candidate.tankRomId = getLe64(bytes + pos);
    if (validateConfig(candidate, plateLimitC)) return false;
    config = candidate;
    return true;
}

EnergyBlob encodeEnergy(const EnergyCheckpoint& checkpoint) {
    EnergyBlob bytes{};
    putLe32(bytes.data(), kEnergyTag);
    putLe32(bytes.data() + 4, kEnergySchema);
    putLe32(bytes.data() + 8, checkpoint.sequence);
    putLe64(bytes.data() + 12, checkpoint.installationId);
    putLe64(bytes.data() + 20, checkpoint.microWh);
    putLe32(bytes.data() + 28, crc32(bytes.data(), 28));
    return bytes;
}

bool decodeEnergy(const EnergyBlob& bytes, EnergyCheckpoint& checkpoint) {
    if (getLe32(bytes.data()) != kEnergyTag || getLe32(bytes.data() + 4) != kEnergySchema ||
        crc32(bytes.data(), 28) != getLe32(bytes.data() + 28)) return false;
    const EnergyCheckpoint decoded{getLe64(bytes.data() + 20), getLe64(bytes.data() + 12), getLe32(bytes.data() + 8)};
    if (!decoded.installationId || !decoded.sequence) return false;
    checkpoint = decoded;
    return true;
}

} // namespace klima
