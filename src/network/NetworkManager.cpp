#include "NetworkManager.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_sntp.h>
#include <algorithm>
#include <cstring>

namespace klima {
namespace {
constexpr uint32_t kWifiRetryS[] = {10, 15, 30, 60};

bool wifiChanged(const Config& a, const Config& b) {
    return strcmp(a.wifiSsid, b.wifiSsid) || strcmp(a.wifiPassword, b.wifiPassword) ||
        strcmp(a.wifiIp, b.wifiIp) || strcmp(a.wifiMask, b.wifiMask) ||
        strcmp(a.wifiGateway, b.wifiGateway) || strcmp(a.wifiDns, b.wifiDns);
}
} // namespace

bool NetworkManager::begin() {
    WiFi.persistent(false);
    const bool ok = web_.begin();
    httpReady_.store(web_.httpReady());
    csvReady_.store(web_.csvReady());
    return ok;
}

void NetworkManager::applyWifi() {
    wifiOnline_.store(false);
    if (sntp_enabled()) sntp_stop();
    wifiRetryIndex_ = 0;
    retryWifiAtUs_ = 0;
    if (!config_.wifiSsid[0]) {
        WiFi.mode(WIFI_OFF);
        return;
    }
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, false);
    IPAddress ip, gateway, mask, dns;
    ip.fromString(config_.wifiIp);
    mask.fromString(config_.wifiMask);
    gateway.fromString(config_.wifiGateway);
    dns.fromString(config_.wifiDns);
    WiFi.config(ip, gateway, mask, dns);
}

void NetworkManager::applyTime() {
    if (sntp_enabled()) sntp_stop();
    memcpy(activeNtpServer_, config_.ntpServer, sizeof(activeNtpServer_));
    if (activeNtpServer_[0]) configTime(0, 0, activeNtpServer_);
}

void NetworkManager::tickWifi(uint64_t nowUs) {
    if (!config_.wifiSsid[0]) return;
    const bool connected = WiFi.status() == WL_CONNECTED && static_cast<uint32_t>(WiFi.localIP()) != 0;
    const bool prior = wifiOnline_.exchange(connected);
    if (connected) {
        if (!prior) { retryWifiAtUs_ = 0; wifiRetryIndex_ = 0; applyTime(); }
        return;
    }
    if (nowUs < retryWifiAtUs_) return;
    WiFi.begin(config_.wifiSsid, config_.wifiPassword);
    retryWifiAtUs_ = nowUs + kWifiRetryS[wifiRetryIndex_] * 1000000ULL;
    wifiRetryIndex_ = std::min<unsigned>(wifiRetryIndex_ + 1, sizeof(kWifiRetryS) / sizeof(kWifiRetryS[0]) - 1);
}

void NetworkManager::tick(uint64_t nowUs) {
    if (nowUs >= configInspectAtUs_) {
        const Config next = port_.config();
        configInspectAtUs_ = nowUs + 200000;
        if (!configKnown_ || next.revision != config_.revision) {
            if (!revisionPending_ || pendingRevision_ != next.revision) {
                revisionPending_ = true;
                pendingRevision_ = next.revision;
                configApplyAtUs_ = configKnown_ && wifiChanged(config_, next) ? nowUs + 2000000 : nowUs;
            }
            if (nowUs >= configApplyAtUs_) {
                const bool wifiNew = !configKnown_ || wifiChanged(config_, next);
                const bool timeNew = !configKnown_ || strcmp(config_.ntpServer, next.ntpServer);
                config_ = next;
                configKnown_ = true;
                revisionPending_ = false;
                if (wifiNew) applyWifi();
                else if (timeNew && wifiOnline()) applyTime();
            }
        }
    }
    tickWifi(nowUs);
    mqtt_.tick(nowUs, config_, wifiOnline(), port_);
    web_.update({wifiOnline(), mqttOnline(), httpReady(), csvReady()}, droppedTelemetry());
}

} // namespace klima
