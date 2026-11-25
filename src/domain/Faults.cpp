#include "Control.h"

namespace klima {
namespace {
using Code = FaultCode;
using Level = Severity;
struct FaultDefinition {
    const char* code;
    const char* text;
    Severity level;
    uint32_t inhibits;
    bool requiresAck;
};
constexpr FaultDefinition definitions[] = {
    {"NONE", "", Level::Info, 0, false},
    {"PLATE_SENSOR_INVALID", "Aktiver Plattensensor ungültig", Level::Error, InhibitPlateMissing, true},
    {"PLATE_SENSOR_STALE", "Aktiver Plattenmesswert veraltet", Level::Error, InhibitPlateMissing, true},
    {"NO_ACTIVE_PLATE_SENSOR", "Kein Plattensensor aktiviert", Level::Error, InhibitPlateMissing, false},
    {"PLATE_OVERTEMP", "Plattentemperatur am festen Grenzwert oder darüber", Level::Critical, InhibitFault, true},
    {"VIN_INVALID", "Versorgungsspannungsmessung ungültig", Level::Error, InhibitVin, true},
    {"VIN_OUT_OF_RANGE", "Versorgungsspannung außerhalb des freigegebenen Bereichs", Level::Error, InhibitVin, true},
    {"CONFIG_INVALID", "Gespeicherte Konfiguration ungültig", Level::Critical, InhibitConfig, true},
    {"ACTUATOR_INIT_FAILED", "Aktorausgänge konnten nicht initialisiert werden", Level::Critical, InhibitFault, true},
    {"CONTROL_OVERRUN", "Zeitgrenze der Steuerung überschritten", Level::Critical, InhibitFault, true},
    {"ROOM_SENSOR_INVALID", "Klimamessung im Rückzugsraum unvollständig", Level::Warning, 0, false},
    {"OUTSIDE_TEMPERATURE_INVALID", "Außentemperatur ungültig", Level::Error, InhibitDriverAmbient | InhibitEvapSensors, false},
    {"OUTSIDE_HUMIDITY_INVALID", "Außenluftfeuchte ungültig", Level::Warning, InhibitEvapSensors, false},
    {"TANK_SENSOR_INVALID", "Tanktemperatur ungültig", Level::Warning, InhibitEvapSensors, false},
    {"STALL_SENSOR_INVALID", "Klimamessung im Stall unvollständig", Level::Warning, 0, false},
    {"RECORD_IO_ERROR", "Messdatei nicht verfügbar oder Schreibfehler", Level::Error, 0, false},
    {"RECORD_QUEUE_FULL", "Warteschlange der Messaufzeichnung voll", Level::Error, 0, false},
    {"TEST_INPUT_LIMIT", "Versuchstastgrad überschreitet die aktuelle Stromgrenze", Level::Error, InhibitHeatingLimit, false},
    {"WIFI_OFFLINE", "WLAN getrennt", Level::Warning, 0, false},
    {"MQTT_OFFLINE", "MQTT-Broker getrennt", Level::Warning, 0, false},
    {"PLATE_SENSOR_DISABLED", "Plattensensor manuell deaktiviert", Level::Info, 0, false},
    {"PLATE_SPREAD_WARNING", "Plattentemperaturspreizung auffällig", Level::Warning, 0, false},
    {"PLATE_CHANGE_WARNING", "Temperaturänderung eines Plattensensors auffällig", Level::Warning, 0, false},
    {"ENERGY_CHECKPOINT_ERROR", "Energiezähler konnte nicht dauerhaft gespeichert werden", Level::Warning, 0, false},
    {"WEB_OFFLINE", "Lokaler Webserver nicht verfügbar", Level::Warning, 0, false},
};
static_assert(std::size(definitions) == static_cast<std::size_t>(Code::Count));

std::size_t codeIndex(Code code) {
    return code < Code::Count ? static_cast<std::size_t>(code) : 0;
}
}  // namespace

const char* faultCodeName(FaultCode code) { return definitions[codeIndex(code)].code; }
const char* faultText(FaultCode code) { return definitions[codeIndex(code)].text; }
Severity faultSeverity(FaultCode code) { return definitions[codeIndex(code)].level; }

void FaultManager::observe(FaultCode code, bool active, uint64_t nowUs) {
    const auto index = codeIndex(code);
    if (index == 0) return;
    auto& s = statuses_[index];
    if (active) s.lastTimeUs = nowUs;
    if (s.active == active) return;
    s.active = active;
    if (active) {
        if (s.count == 0) s.firstTimeUs = nowUs;
        if (s.count != UINT32_MAX) ++s.count;
        s.severity = definitions[index].level;
        s.inhibitReasons = definitions[index].inhibits;
        s.latched = definitions[index].requiresAck;
    }
    addEvent(code, active ? FaultChange::Raised : FaultChange::Cleared, nowUs);
}

bool FaultManager::acknowledge(FaultCode code, uint64_t nowUs) {
    auto& s = statuses_[codeIndex(code)];
    if (!s.latched || s.active) return false;
    s.latched = false;
    addEvent(code, FaultChange::Acknowledged, nowUs);
    return true;
}

FaultSummary FaultManager::summary() const {
    FaultSummary result;
    Severity topSeverity = Severity::Info;
    for (std::size_t i = 1; i < statuses_.size(); ++i) {
        const auto code = static_cast<FaultCode>(i);
        const auto& status = statuses_[i];
        if (status.active) result.activeMask |= faultBit(code);
        if (status.latched) result.latchedMask |= faultBit(code);
        if (!status.active && !status.latched) continue;
        if (status.severity == Severity::Warning) result.warningFlag = true;
        if (status.severity < Severity::Error) continue;
        ++result.errorCount;
        if (status.severity > topSeverity) {
            result.topCode = code;
            topSeverity = status.severity;
        }
    }
    result.errorFlag = result.errorCount != 0;
    return result;
}

const FaultStatus& FaultManager::status(FaultCode code) const { return statuses_[codeIndex(code)]; }

void FaultManager::addEvent(FaultCode code, FaultChange change, uint64_t nowUs) {
    const auto& s = status(code);
    history_[next_] = {nowUs, s.count, code, s.severity, change};
    next_ = (next_ + 1) % capacity;
    if (size_ < capacity) ++size_;
}

const FaultEvent& FaultManager::at(std::size_t index) const {
    static constexpr FaultEvent empty{};
    return index < size_ ? history_[(next_ + capacity - size_ + index) % capacity] : empty;
}
}  // namespace klima
