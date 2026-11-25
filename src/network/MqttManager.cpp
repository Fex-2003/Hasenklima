#include "MqttManager.h"
#include <Arduino.h>
#include <algorithm>
#include <cstdio>
#include <cstring>

namespace klima {
namespace {
constexpr uint32_t kRetriesS[] = {1, 2, 5, 10, 30, 60};
constexpr size_t kMaxOutboxBytes = 16384;
constexpr unsigned kPlateCount = 9;
constexpr const char* kTopics[] = {
    "status/state", "status/faults", "status/identification", "sensor/plate/statistics",
    "sensor/tank", "sensor/room", "sensor/stall", "sensor/outside", "sensor/vin", "heater/state",
    "pump/cooling", "pump/evaporation", "driver_heater/state", "telemetry"
};

void hashText(uint32_t& hash, const char* value) {
    if (!value) return;
    for (; *value; ++value) hash = (hash ^ static_cast<unsigned char>(*value)) * 16777619u;
}

uint32_t stateHash(const NetworkSnapshot& snapshot) {
    uint32_t hash = 2166136261u;
    hashText(hash, snapshot.mode);
    hashText(hash, snapshot.autoMode);
    hashText(hash, snapshot.errorText);
    hashText(hash, testPhaseCode(snapshot.recorder.metadata.phase));
    hashText(hash, testReasonCode(snapshot.recorder.metadata.reason));
    hash = (hash ^ snapshot.errorFlag ^ (snapshot.warningFlag << 1) ^ (snapshot.inhibitReasons << 2)) * 16777619u;
    for (size_t i = 0; i < std::min<size_t>(snapshot.faultCount, snapshot.faults.size()); ++i) {
        const auto& fault = snapshot.faults[i];
        hashText(hash, fault.code);
        hash = (hash ^ fault.active ^ (fault.latched << 1)) * 16777619u;
    }
    return hash;
}
} // namespace

void MqttManager::event(void* context, esp_event_base_t, int32_t eventId, void* data) {
    auto& manager = *static_cast<MqttManager*>(context);
    if (eventId == MQTT_EVENT_CONNECTED) {
        manager.disconnected_.store(false);
        manager.online_.store(true);
    } else if (eventId == MQTT_EVENT_DISCONNECTED || eventId == MQTT_EVENT_ERROR) {
        manager.online_.store(false);
        manager.disconnected_.store(true);
    } else if (eventId == MQTT_EVENT_PUBLISHED) {
        manager.lastAck_.store(static_cast<esp_mqtt_event_handle_t>(data)->msg_id);
    }
}

bool MqttManager::start() {
    esp_mqtt_client_config_t settings{};
    settings.host = settings_.host;
    settings.port = settings_.port;
    settings.username = settings_.user[0] ? settings_.user : nullptr;
    settings.password = settings_.password[0] ? settings_.password : nullptr;
    settings.client_id = settings_.clientId;
    settings.lwt_topic = settings_.availability;
    settings.lwt_msg = "offline";
    settings.lwt_qos = 1;
    settings.lwt_retain = 1;
    settings.transport = MQTT_TRANSPORT_OVER_TCP;
    settings.protocol_ver = MQTT_PROTOCOL_V_3_1_1;
    settings.disable_auto_reconnect = true;
    settings.network_timeout_ms = 1400;
    settings.keepalive = 20;
    settings.task_prio = 2;
    settings.task_stack = 4608;
    settings.buffer_size = 1024;
    client_ = esp_mqtt_client_init(&settings);
    if (!client_) return false;
    if (esp_mqtt_client_register_event(client_, MQTT_EVENT_ANY, event, this) != ESP_OK ||
        esp_mqtt_client_start(client_) != ESP_OK) {
        esp_mqtt_client_destroy(client_);
        client_ = nullptr;
        return false;
    }
    return true;
}

void MqttManager::stop() {
    if (client_) {
        esp_mqtt_client_stop(client_);
        esp_mqtt_client_destroy(client_);
        client_ = nullptr;
    }
    online_.store(false);
    disconnected_.store(false);
    lastAck_.store(-1);
    shutdownId_ = -1;
    shutdownAtUs_ = 0;
    announced_ = batchActive_ = faultRetry_ = false;
}

bool MqttManager::enqueue(const char* relativeTopic, const char* message, size_t length, int qos,
                          bool retain, int* messageId) {
    char topic[144];
    snprintf(topic, sizeof(topic), "%s/%s", settings_.prefix, relativeTopic);
    const int queued = esp_mqtt_client_get_outbox_size(client_);
    if (queued < 0 || static_cast<size_t>(queued) + length + strlen(topic) + 24 > kMaxOutboxBytes) {
        ++dropped_;
        return false;
    }
    const int id = esp_mqtt_client_enqueue(client_, topic, message, length, qos, retain, true);
    if (id < 0) { ++dropped_; return false; }
    if (messageId) *messageId = id;
    return true;
}

bool MqttManager::identityChanged(const Config& c) const {
    char clientId[65];
    if (c.mqttClientId[0]) snprintf(clientId, sizeof(clientId), "%s", c.mqttClientId);
    else snprintf(clientId, sizeof(clientId), "hasenklima-%012llX",
                    static_cast<unsigned long long>(ESP.getEfuseMac() & 0xffffffffffffULL));
    return c.mqttPort != settings_.port || strcmp(c.mqttHost, settings_.host) ||
           strcmp(c.mqttUser, settings_.user) || strcmp(c.mqttPassword, settings_.password) ||
           strcmp(c.mqttPrefix, settings_.prefix) || strcmp(clientId, settings_.clientId);
}

void MqttManager::publishNext(const NetworkStatus& status) {
    if (publishIndex_ == 0) {
        if (faultUpdate_ && !enqueue("status/error_flag", snapshot_.errorFlag ? "true" : "false",
                                     snapshot_.errorFlag ? 4 : 5, 1, true)) faultRetry_ = true;
    } else if (publishIndex_ == 1) {
        if (faultUpdate_ && !enqueue("status/error_text", snapshot_.errorText,
                                     strlen(snapshot_.errorText), 1, true)) faultRetry_ = true;
    } else {
        const unsigned index = publishIndex_ - 2;
        char plateTopic[32];
        const char* topic;
        if (index < kPlateCount) {
            snprintf(plateTopic, sizeof(plateTopic), "sensor/plate/%u", index + 1);
            topic = plateTopic;
        } else {
            topic = kTopics[index - kPlateCount];
        }
        JsonWriter json(payload_, sizeof(payload_));
        writeMqttPayload(json, topic, snapshot_, status, index < kPlateCount ? index + 1 : 0);
        const bool durable = faultUpdate_ && !strcmp(topic, "status/faults");
        if (json.good()) {
            if (!enqueue(topic, payload_, json.size(), durable ? 1 : 0, durable) && durable) faultRetry_ = true;
        } else {
            ++dropped_;
            if (durable) faultRetry_ = true;
        }
    }
    ++publishIndex_;
    batchActive_ = publishIndex_ < 2 + kPlateCount + sizeof(kTopics) / sizeof(kTopics[0]);
}

void MqttManager::tick(uint64_t nowUs, const Config& config, bool wifi, NetworkPort& port) {
    const bool enabled = config.mqttEnabled && config.mqttHost[0];
    const bool changed = enabled && identityChanged(config);
    if (client_ && (!enabled || changed || shutdownAtUs_)) {
        if (!shutdownAtUs_ && online()) {
            enqueue("status/availability", "offline", 7, 1, true, &shutdownId_);
            shutdownAtUs_ = nowUs + 500000;
        }
        if (online() && shutdownAtUs_ > nowUs && shutdownId_ >= 0 && lastAck_.load() != shutdownId_)
            return;
        stop();
        retryIndex_ = 0;
        retryAtUs_ = nowUs;
    }
    if (!enabled || !wifi) return;
    if (!client_) {
        if (nowUs < retryAtUs_) return;
        snprintf(settings_.host, sizeof(settings_.host), "%s", config.mqttHost);
        snprintf(settings_.user, sizeof(settings_.user), "%s", config.mqttUser);
        snprintf(settings_.password, sizeof(settings_.password), "%s", config.mqttPassword);
        snprintf(settings_.prefix, sizeof(settings_.prefix), "%s", config.mqttPrefix);
        snprintf(settings_.availability, sizeof(settings_.availability), "%s/status/availability", settings_.prefix);
        if (config.mqttClientId[0]) snprintf(settings_.clientId, sizeof(settings_.clientId), "%s", config.mqttClientId);
        else snprintf(settings_.clientId, sizeof(settings_.clientId), "hasenklima-%012llX",
                        static_cast<unsigned long long>(ESP.getEfuseMac() & 0xffffffffffffULL));
        settings_.port = config.mqttPort;
        connectAtUs_ = nowUs;
        if (start()) return;
    }
    if (!online()) {
        if (client_ && !disconnected_.load() && nowUs - connectAtUs_ < 20000000) return;
        stop();
        retryAtUs_ = nowUs + kRetriesS[retryIndex_] * 1000000ULL;
        retryIndex_ = std::min<unsigned>(retryIndex_ + 1, sizeof(kRetriesS) / sizeof(kRetriesS[0]) - 1);
        return;
    }
    if (!announced_) {
        announced_ = enqueue("status/availability", "online", 6, 1, true);
        retryIndex_ = 0;
        nextTelemetryUs_ = inspectAtUs_ = nowUs;
        lastStateHash_ = 0;
    }
    if (batchActive_) {
        const NetworkStatus status{wifi, true, true, true};
        for (unsigned i = 0; i < 4 && batchActive_; ++i) publishNext(status);
    } else if (nowUs >= inspectAtUs_) {
        port.snapshot(snapshot_);
        const uint32_t signature = stateHash(snapshot_);
        faultUpdate_ = faultRetry_ || signature != lastStateHash_;
        inspectAtUs_ = nowUs + 250000;
        if (faultUpdate_ || nowUs >= nextTelemetryUs_) {
            if (esp_mqtt_client_get_outbox_size(client_) > static_cast<int>(kMaxOutboxBytes / 2)) {
                ++dropped_;
                return;
            }
            lastStateHash_ = signature;
            faultRetry_ = false;
            nextTelemetryUs_ = nowUs + config.mqttIntervalS * 1000000ULL;
            publishIndex_ = 0;
            batchActive_ = true;
        }
    }
}

} // namespace klima
