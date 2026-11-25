#pragma once

#include "NetworkJson.h"
#include "SessionAuth.h"
#include <atomic>
#include <esp_http_server.h>

namespace klima {

class WebServerManager {
public:
    explicit WebServerManager(NetworkPort& port) : port_(port) {}
    bool begin();
    bool httpReady() const { return httpReady_; }
    bool csvReady() const { return csvReady_; }
    void update(NetworkStatus state, uint32_t dropped);
private:
    static esp_err_t handle(httpd_req_t* request);
    static esp_err_t handleCsv(httpd_req_t* request);
    esp_err_t route(httpd_req_t* request);
    esp_err_t get(httpd_req_t* request, const char* path);
    esp_err_t write(httpd_req_t* request, const char* path);
    esp_err_t csv(httpd_req_t* request);
    NetworkStatus status() const;

    NetworkPort& port_;
    SessionAuth auth_;
    httpd_handle_t http_{}, csv_{};
    bool httpReady_{}, csvReady_{};
    uint32_t firmwareBytes_{};
    std::atomic<uint8_t> statusBits_{0};
    std::atomic<uint32_t> droppedTelemetry_{0};
    NetworkSnapshot snapshot_{};
    NetHistoryPoint history_[145]{};
    NetFault faultHistory_[100]{};
    char response_[12288]{};
    char body_[4096]{};
};

} // namespace klima
