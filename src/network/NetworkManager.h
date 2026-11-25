#pragma once

#include "MqttManager.h"
#include "WebServerManager.h"
#include <atomic>

namespace klima {

class NetworkManager {
public:
    explicit NetworkManager(NetworkPort& port) : port_(port), web_(port) {}
    bool begin();
    void tick(uint64_t nowUs);
    bool wifiOnline() const { return wifiOnline_.load(); }
    bool mqttOnline() const { return wifiOnline() && mqtt_.online(); }
    bool httpReady() const { return httpReady_.load(); }
    bool csvReady() const { return csvReady_.load(); }
    uint32_t droppedTelemetry() const { return mqtt_.dropped(); }
private:
    void applyWifi();
    void tickWifi(uint64_t nowUs);
    void applyTime();
    NetworkPort& port_;
    WebServerManager web_;
    MqttManager mqtt_;
    Config config_{};
    char activeNtpServer_[sizeof(Config::ntpServer)]{};
    uint32_t pendingRevision_{};
    bool configKnown_{}, revisionPending_{};
    uint64_t retryWifiAtUs_{}, configInspectAtUs_{}, configApplyAtUs_{};
    uint8_t wifiRetryIndex_{};
    std::atomic<bool> wifiOnline_{false}, httpReady_{false}, csvReady_{false};
};

} // namespace klima
