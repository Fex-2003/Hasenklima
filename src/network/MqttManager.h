#pragma once

#include "NetworkJson.h"
#include <atomic>
#include <mqtt_client.h>

namespace klima {

class MqttManager {
public:
    void tick(uint64_t nowUs, const Config& config, bool wifiOnline, NetworkPort& port);
    bool online() const { return online_.load(); }
    uint32_t dropped() const { return dropped_.load(); }
private:
    static void event(void* context, esp_event_base_t, int32_t eventId, void* data);
    bool start();
    void stop();
    bool enqueue(const char* relativeTopic, const char* message, size_t length, int qos, bool retain,
                 int* messageId = nullptr);
    void publishNext(const NetworkStatus& status);
    bool identityChanged(const Config& config) const;

    struct Settings {
        char host[254]{}, user[65]{}, password[129]{}, clientId[65]{}, prefix[97]{}, availability[122]{};
        uint32_t port{};
    } settings_{};
    esp_mqtt_client_handle_t client_{};
    std::atomic<bool> online_{false}, disconnected_{false};
    std::atomic<int> lastAck_{-1};
    std::atomic<uint32_t> dropped_{0};
    uint8_t retryIndex_{}, publishIndex_{};
    uint64_t retryAtUs_{}, nextTelemetryUs_{}, inspectAtUs_{}, shutdownAtUs_{}, connectAtUs_{};
    int shutdownId_{-1};
    bool announced_{}, batchActive_{}, faultUpdate_{}, faultRetry_{};
    uint32_t lastStateHash_{};
    NetworkSnapshot snapshot_{};
    char payload_[12288]{};
};

} // namespace klima
