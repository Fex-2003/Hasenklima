#include "WebServerManager.h"
#include "WebAssets.h"
#include <BoardConfig.h>
#include <HardwareLimits.h>
#include <Arduino.h>
#include <WiFi.h>
#include <cJSON.h>
#include <esp_arduino_version.h>
#include <esp_heap_caps.h>
#include <esp_ota_ops.h>
#include <esp_partition.h>
#include <esp_timer.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>

namespace klima {
namespace {
constexpr char kApi[] = "/api/v1";
constexpr unsigned kCsvPort = 81;
using JsonPtr = std::unique_ptr<cJSON, decltype(&cJSON_Delete)>;

const char* statusLine(int status) {
    switch (status) {
    case 200: return "200 OK";
    case 202: return "202 Accepted";
    case 303: return "303 See Other";
    case 400: return "400 Bad Request";
    case 401: return "401 Unauthorized";
    case 403: return "403 Forbidden";
    case 404: return "404 Not Found";
    case 409: return "409 Conflict";
    case 422: return "422 Unprocessable Entity";
    default: return "503 Service Unavailable";
    }
}

esp_err_t send(httpd_req_t* req, int status, const char* data, size_t size,
               const char* contentType = "application/json; charset=utf-8") {
    httpd_resp_set_status(req, statusLine(status));
    httpd_resp_set_type(req, contentType);
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "X-Content-Type-Options", "nosniff");
    return httpd_resp_send(req, data, size);
}

esp_err_t error(httpd_req_t* req, int status, const char* code, const char* message) {
    char buffer[384];
    JsonWriter json(buffer, sizeof(buffer));
    json.object();
    json.integer("schema_version", 1);
    json.text("code", code);
    json.text("message", message);
    json.end();
    return send(req, status, buffer, json.size());
}

esp_err_t sendJson(httpd_req_t* req, const JsonWriter& json, const char* buffer, int status = 200) {
    if (!json.good()) return error(req, 503, "RESPONSE_TOO_LARGE", "Die Antwort überschreitet den verfügbaren Puffer.");
    return send(req, status, buffer, json.size());
}

bool nameIn(const char* key, const char* const* keys, size_t count) {
    for (size_t i = 0; i < count; ++i) if (!strcmp(key, keys[i])) return true;
    return false;
}

template <typename... Keys>
bool fields(const cJSON* object, Keys... names) {
    if (!cJSON_IsObject(object)) return false;
    const char* known[] = {nullptr, names...};
    for (const cJSON* item = object->child; item; item = item->next)
        if (!item->string || !nameIn(item->string, known + 1, sizeof...(names))) return false;
    return true;
}

const cJSON* field(const cJSON* object, const char* name) {
    return cJSON_GetObjectItemCaseSensitive(object, name);
}

bool boolField(const cJSON* doc, const char* key, bool& out, bool required = false) {
    const auto* value = field(doc, key);
    if (!value) return !required;
    if (!cJSON_IsBool(value)) return false;
    out = cJSON_IsTrue(value);
    return true;
}

bool uintField(const cJSON* doc, const char* key, uint32_t& out, bool required = true) {
    const auto* value = field(doc, key);
    if (!value) return !required;
    if (!cJSON_IsNumber(value) || !std::isfinite(value->valuedouble) || value->valuedouble < 0 ||
        value->valuedouble > UINT32_MAX || value->valuedouble != std::floor(value->valuedouble)) return false;
    out = static_cast<uint32_t>(value->valuedouble);
    return true;
}

bool pwmField(const cJSON* doc, const char* key, float& percent, bool required) {
    const auto* value = field(doc, key);
    if (!value) return !required;
    if (!cJSON_IsNumber(value) || !std::isfinite(value->valuedouble) || std::abs(value->valuedouble) > 1e8) return false;
    percent = static_cast<float>(value->valuedouble);
    return true;
}

bool copyText(char* out, size_t capacity, const cJSON* value, bool required = true) {
    if (!value) return !required;
    if (!cJSON_IsString(value) || !value->valuestring || strlen(value->valuestring) >= capacity) return false;
    memcpy(out, value->valuestring, strlen(value->valuestring) + 1);
    return true;
}

bool romField(const cJSON* doc, const char* key, uint64_t& rom) {
    const auto* value = field(doc, key);
    if (!value) return true;
    if (cJSON_IsNull(value)) { rom = 0; return true; }
    if (!cJSON_IsString(value) || strlen(value->valuestring) != 16) return false;
    rom = 0;
    for (const char* p = value->valuestring; *p; ++p) {
        const int digit = *p >= '0' && *p <= '9' ? *p - '0' : *p >= 'A' && *p <= 'F' ? *p - 'A' + 10 :
            *p >= 'a' && *p <= 'f' ? *p - 'a' + 10 : -1;
        if (digit < 0) return false;
        rom = rom * 16 + digit;
    }
    return true;
}

bool revisionField(const cJSON* doc, const char* key, Command& command, bool required) {
    command.checkRevision = field(doc, key) != nullptr;
    return uintField(doc, key, command.expectedRevision, required);
}

bool requestId(const cJSON* doc, Command& command, bool required) {
    if (!copyText(command.requestId, sizeof(command.requestId), field(doc, "request_id"), required)) return false;
    if (!command.requestId[0]) return !required && !field(doc, "request_id");
    for (const char* p = command.requestId; *p; ++p) {
        const char c = *p;
        if ((c < 'a' || c > 'z') && (c < 'A' || c > 'Z') && (c < '0' || c > '9') && c != '-' && c != '_')
            return false;
    }
    return true;
}

// Depth and control bytes are checked before cJSON allocates a parse tree.
bool boundedJson(const char* data) {
    int depth = 0;
    bool quoted = false;
    for (const char* p = data; *p; ++p) {
        if (quoted && *p == '\\') {
            if (!p[1] || !strncmp(p + 1, "u0000", 5)) return false;
            ++p;
        } else if (*p == '"') {
            quoted = !quoted;
        } else if (!quoted && (*p == '{' || *p == '[')) {
            if (++depth > 3) return false;
        } else if (!quoted && (*p == '}' || *p == ']')) {
            if (--depth < 0) return false;
        }
    }
    return !quoted && depth == 0;
}

bool uniqueKeys(const cJSON* doc) {
    if (!cJSON_IsObject(doc)) return true;
    for (const cJSON* item = doc->child; item; item = item->next) {
        for (const cJSON* other = item->next; other; other = other->next)
            if (!strcmp(item->string, other->string)) return false;
        if (!uniqueKeys(item)) return false;
    }
    return true;
}

bool readBody(httpd_req_t* req, char* buffer, size_t capacity) {
    if (req->content_len >= capacity) return false;
    if (!req->content_len) { strcpy(buffer, "{}"); return true; }
    char type[64];
    if (httpd_req_get_hdr_value_str(req, "Content-Type", type, sizeof(type)) != ESP_OK ||
        strncmp(type, "application/json", 16) || (type[16] && type[16] != ';')) return false;
    const int64_t deadlineUs = esp_timer_get_time() + 1500000;
    size_t position = 0;
    while (position < req->content_len && esp_timer_get_time() < deadlineUs) {
        const int received = httpd_req_recv(req, buffer + position, req->content_len - position);
        if (received <= 0 || memchr(buffer + position, '\0', received)) return false;
        position += received;
    }
    buffer[position] = '\0';
    return position == req->content_len && boundedJson(buffer);
}

const char* operationState(OperationState state) {
    switch (state) {
    case OperationState::Pending: return "pending";
    case OperationState::Applied: return "applied";
    case OperationState::Failed: return "failed";
    }
    return "failed";
}

void operationJson(JsonWriter& json, const OperationResult& op) {
    json.object();
    json.integer("schema_version", 1);
    json.integer("operation_id", op.id);
    json.integer("revision", op.revision);
    json.integer("session_id", op.sessionId);
    json.integer("status_code", op.status);
    json.text("state", operationState(op.state));
    json.text("code", op.code);
    json.text("message", op.message);
    json.number("pwm_limit_percent", op.heaterDutyLimit * 100.0);
    json.number("pwm_effective_percent", op.heaterDutyEffective * 100.0);
    json.integer("inhibit_reasons", op.inhibitReasons);
    json.end();
}

bool operationId(const char* path, uint32_t& id) {
    constexpr char prefix[] = "/operations/";
    if (strncmp(path, prefix, sizeof(prefix) - 1)) return false;
    const char* p = path + sizeof(prefix) - 1;
    if (!*p || strlen(p) > 10) return false;
    uint64_t value = 0;
    for (; *p >= '0' && *p <= '9'; ++p) value = value * 10 + *p - '0';
    if (*p || value == 0 || value > UINT32_MAX) return false;
    id = static_cast<uint32_t>(value);
    return true;
}

bool pathMatch(const char* path, const char* expected) { return strcmp(path, expected) == 0; }

void writeDebug(JsonWriter& json, const NetworkSnapshot& s, const NetDiagnostics& d,
                NetworkStatus network, uint32_t droppedTelemetry, uint32_t firmwareBytes) {
    json.object();
    writeHeader(json, s);
    json.text("firmware_version", FIRMWARE_VERSION);
    json.text("idf_version", esp_get_idf_version());
    char core[20];
    snprintf(core, sizeof(core), "%d.%d.%d", ESP_ARDUINO_VERSION_MAJOR, ESP_ARDUINO_VERSION_MINOR, ESP_ARDUINO_VERSION_PATCH);
    json.text("arduino_core", core);
    json.number("max_plate_temp_c", d.plateLimitC);
    json.integer("heater_pwm_hz", board::heaterPwmHz);
    json.integer("heater_pwm_bits", board::heaterPwmBits);
    json.object("pins");
    json.integer("heater", board::heaterPin);
    json.integer("cooling", board::coolingPin);
    json.integer("evaporation", board::evaporationPin);
    json.integer("driver_heater", board::driverHeaterPin);
    json.integer("plate_bus", board::plateBusPin);
    json.integer("tank_bus", board::tankBusPin);
    json.integer("vin_adc", board::vinPin);
    json.integer("i2c_sda", board::i2cSda);
    json.integer("i2c_scl", board::i2cScl);
    json.end();
    json.integer("flash_bytes", ESP.getFlashChipSize());
    json.integer("firmware_bytes", firmwareBytes);
    const auto* app = esp_ota_get_running_partition();
    const auto* fs = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_DATA_SPIFFS, nullptr);
    if (app) json.integer("app_partition_bytes", app->size);
    if (fs) json.integer("filesystem_partition_bytes", fs->size);
    json.boolean("filesystem_mounted", d.filesystemMounted);
    json.integer("free_heap_bytes", heap_caps_get_free_size(MALLOC_CAP_8BIT));
    json.integer("largest_heap_block_bytes", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
    json.integer("minimum_free_heap_bytes", heap_caps_get_minimum_free_size(MALLOC_CAP_8BIT));
    json.integer("max_control_gap_us", d.maxControlGapUs);
    json.integer("max_control_gap_ms", d.maxControlGapUs / 1000);
    json.integer("max_control_run_us", d.maxControlRunUs);
    json.integer("max_sensor_age_ms", d.maxSensorAgeUs / 1000);
    json.integer("max_sensor_run_us", d.maxSensorRunUs);
    json.integer("max_network_run_us", d.maxNetworkRunUs);
    json.integer("max_storage_run_us", d.maxStorageRunUs);
    json.integer("max_flash_operation_us", d.maxFlashOpUs);
    json.integer("control_stack_free_bytes", d.controlStackBytes);
    json.integer("sensor_stack_free_bytes", d.sensorStackBytes);
    json.integer("network_stack_free_bytes", d.networkStackBytes);
    json.integer("storage_stack_free_bytes", d.storageStackBytes);
    json.integer("reset_reason", d.resetReason);
    json.integer("reboot_count", d.rebootCount);
    json.boolean("watchdog_active", d.watchdogActive);
    json.integer("command_queue_used", d.commandQueueUsed);
    json.integer("record_queue_used", d.recordQueueUsed);
    json.integer("dropped_mqtt_telemetry", droppedTelemetry);
    json.boolean("http_ready", network.httpReady);
    json.boolean("csv_ready", network.csvReady);
    json.boolean("wifi_online", network.wifiOnline);
    json.boolean("mqtt_online", network.mqttOnline);
    char ip[16];
    const IPAddress addr = WiFi.localIP();
    snprintf(ip, sizeof(ip), "%u.%u.%u.%u", addr[0], addr[1], addr[2], addr[3]);
    json.text("wifi_ip", ip);
    json.integer("plate_enabled_mask", s.enabledMask);
    json.number("pi_integrator", s.piIntegrator);
    json.number("pwm_limit_percent", s.dutyLimit * 100.0);
    json.integer("inhibit_reasons", s.inhibitReasons);
    json.object("measurement_age_ms");
    json.array("plate");
    auto age = [&json, &s](const char* key, const NetSample& sample) {
        if (sample.timeUs && sample.timeUs <= s.timestampUs) json.integer(key, (s.timestampUs - sample.timeUs) / 1000);
        else json.nullable(key);
    };
    for (const auto& p : s.plate) age(nullptr, p);
    json.end();
    age("tank", s.tank);
    age("vin", s.vin);
    age("room_temperature", s.room.temperature);
    age("room_humidity", s.room.humidity);
    age("stall_temperature", s.stall.temperature);
    age("stall_humidity", s.stall.humidity);
    age("stall_pressure", s.stall.pressure);
    age("outside_temperature", s.outside.temperature);
    age("outside_humidity", s.outside.humidity);
    age("outside_pressure", s.outside.pressure);
    json.end();
    json.end();
}
} // namespace

bool WebServerManager::begin() {
    firmwareBytes_ = ESP.getSketchSize();
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.max_open_sockets = 3;
    cfg.max_uri_handlers = 4;
    cfg.lru_purge_enable = true;
    cfg.recv_wait_timeout = cfg.send_wait_timeout = 1;
    cfg.stack_size = 10240;
    cfg.task_priority = 3;
    cfg.core_id = 0;
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    if (httpd_start(&http_, &cfg) == ESP_OK) {
        httpReady_ = true;
        for (httpd_method_t method : {HTTP_GET, HTTP_POST, HTTP_PUT, HTTP_PATCH}) {
            const httpd_uri_t route{"/*", method, handle, this};
            httpReady_ &= httpd_register_uri_handler(http_, &route) == ESP_OK;
        }
    }
    cfg.server_port = kCsvPort;
    ++cfg.ctrl_port;
    cfg.max_open_sockets = 1;
    cfg.max_uri_handlers = 1;
    cfg.stack_size = 5120;
    cfg.task_priority = 2;
    if (httpd_start(&csv_, &cfg) == ESP_OK) {
        const httpd_uri_t route{"/api/v1/test.csv", HTTP_GET, handleCsv, this};
        csvReady_ = httpd_register_uri_handler(csv_, &route) == ESP_OK;
    }
    return httpReady_ && csvReady_;
}

void WebServerManager::update(NetworkStatus s, uint32_t dropped) {
    statusBits_.store((s.wifiOnline ? 1 : 0) | (s.mqttOnline ? 2 : 0) | (s.httpReady ? 4 : 0) | (s.csvReady ? 8 : 0));
    droppedTelemetry_.store(dropped);
}

NetworkStatus WebServerManager::status() const {
    const unsigned b = statusBits_.load();
    return {bool(b & 1), bool(b & 2), bool(b & 4), bool(b & 8)};
}

esp_err_t WebServerManager::handle(httpd_req_t* req) { return static_cast<WebServerManager*>(req->user_ctx)->route(req); }
esp_err_t WebServerManager::handleCsv(httpd_req_t* req) { return static_cast<WebServerManager*>(req->user_ctx)->csv(req); }

esp_err_t WebServerManager::route(httpd_req_t* req) {
    char path[128];
    const size_t length = strcspn(req->uri, "?");
    if (length >= sizeof(path)) return error(req, 404, "NOT_FOUND", "Unbekannter Pfad.");
    memcpy(path, req->uri, length);
    path[length] = '\0';
    if (req->method == HTTP_GET) return get(req, path);
    return write(req, path);
}

esp_err_t WebServerManager::get(httpd_req_t* req, const char* path) {
    if (pathMatch(path, "/") || pathMatch(path, "/setup") || pathMatch(path, "/debug")) {
        httpd_resp_set_hdr(req, "Content-Security-Policy",
            "default-src 'self'; object-src 'none'; frame-ancestors 'none'; base-uri 'none'; form-action 'self'");
        httpd_resp_set_hdr(req, "Referrer-Policy", "no-referrer");
        return send(req, 200, web::page, sizeof(web::page) - 1, "text/html; charset=utf-8");
    }
    if (pathMatch(path, "/app.css")) return send(req, 200, web::style, sizeof(web::style) - 1, "text/css; charset=utf-8");
    if (pathMatch(path, "/app.js")) return send(req, 200, web::script, sizeof(web::script) - 1, "text/javascript; charset=utf-8");
    if (strncmp(path, kApi, sizeof(kApi) - 1)) return error(req, 404, "NOT_FOUND", "Unbekannter Pfad.");
    path += sizeof(kApi) - 1;
    JsonWriter json(response_, sizeof(response_));
    if (pathMatch(path, "/snapshot")) {
        port_.snapshot(snapshot_);
        writeSnapshot(json, snapshot_, status());
    } else if (pathMatch(path, "/test")) {
        port_.snapshot(snapshot_);
        writeTest(json, snapshot_);
    } else if (pathMatch(path, "/history")) {
        port_.snapshot(snapshot_);
        const size_t count = std::min(port_.history(history_, std::size(history_)), std::size(history_));
        json.object();
        writeHeader(json, snapshot_);
        json.integer("interval_s", 600);
        json.array("points");
        for (size_t i = 0; i < count; ++i) {
            const auto& point = history_[i];
            json.array();
            json.integer(nullptr, point.timeUs);
            for (size_t channel = 0; channel < point.centiC.size(); ++channel) {
                if (point.validMask & (1u << channel)) json.number(nullptr, point.centiC[channel] / 100.0);
                else json.nullable(nullptr);
            }
            json.end();
        }
        json.end();
        json.end();
    } else if (pathMatch(path, "/config")) {
        writeConfig(json, port_.config());
    } else if (pathMatch(path, "/parameters")) {
        writeParameters(json);
    } else if (pathMatch(path, "/sensors/scan")) {
        const auto list = port_.scan();
        json.object();
        json.integer("schema_version", 1);
        json.boolean("ready", list.ready);
        json.boolean("overflow", list.overflow);
        json.array("plate");
        for (unsigned i = 0; i < std::min<size_t>(list.plateCount, list.plate.size()); ++i) json.hex(nullptr, list.plate[i]);
        json.end();
        json.array("tank");
        for (unsigned i = 0; i < std::min<size_t>(list.tankCount, list.tank.size()); ++i) json.hex(nullptr, list.tank[i]);
        json.end();
        json.end();
    } else if (pathMatch(path, "/faults")) {
        port_.snapshot(snapshot_);
        const size_t count = std::min(port_.faultHistory(faultHistory_, std::size(faultHistory_)), std::size(faultHistory_));
        json.object();
        writeHeader(json, snapshot_);
        json.array("current");
        for (unsigned i = 0; i < std::min<size_t>(snapshot_.faultCount, snapshot_.faults.size()); ++i)
            writeFault(json, snapshot_.faults[i]);
        json.end();
        json.end();
        if (!json.good()) return error(req, 503, "RESPONSE_TOO_LARGE", "Die Fehlerliste ist zu groß.");
        httpd_resp_set_type(req, "application/json; charset=utf-8");
        httpd_resp_set_hdr(req, "Cache-Control", "no-store");
        const int64_t deadline = esp_timer_get_time() + 3000000;
        if (httpd_resp_send_chunk(req, response_, json.size() - 1) != ESP_OK ||
            httpd_resp_send_chunk(req, ",\"history\":[", 12) != ESP_OK) return ESP_FAIL;
        for (size_t i = 0; i < count; ++i) {
            JsonWriter entry(response_, sizeof(response_));
            writeFault(entry, faultHistory_[i]);
            if (esp_timer_get_time() >= deadline || !entry.good() ||
                (i && httpd_resp_send_chunk(req, ",", 1) != ESP_OK) ||
                httpd_resp_send_chunk(req, response_, entry.size()) != ESP_OK) return ESP_FAIL;
        }
        if (httpd_resp_send_chunk(req, "]}", 2) != ESP_OK) return ESP_FAIL;
        return httpd_resp_send_chunk(req, nullptr, 0);
    } else if (pathMatch(path, "/debug")) {
        port_.snapshot(snapshot_);
        writeDebug(json, snapshot_, port_.diagnostics(), status(), droppedTelemetry_.load(), firmwareBytes_);
    } else if (pathMatch(path, "/test.csv")) {
        if (!status().csvReady) return error(req, 503, "CSV_UNAVAILABLE", "CSV-Übertragung nicht verfügbar.");
        port_.snapshot(snapshot_);
        if (snapshot_.recorder.running) return error(req, 409, "TEST_RUNNING", "Der Versuch läuft noch.");
        if (snapshot_.recorder.downloadActive) return error(req, 409, "DOWNLOAD_ACTIVE", "Ein Download läuft bereits.");
        if (!snapshot_.recorder.metadata.testId) return error(req, 404, "NO_TEST", "Es ist kein Versuch gespeichert.");
        const IPAddress ip = WiFi.localIP();
        char destination[80];
        snprintf(destination, sizeof(destination), "http://%u.%u.%u.%u:%u/api/v1/test.csv",
                 ip[0], ip[1], ip[2], ip[3], kCsvPort);
        httpd_resp_set_hdr(req, "Location", destination);
        return send(req, 303, "", 0);
    } else {
        uint32_t id;
        OperationResult op;
        if (!operationId(path, id) || !port_.operation(id, op))
            return error(req, 404, "NOT_FOUND", "Unbekannter Pfad oder abgelaufene Auftrags-ID.");
        operationJson(json, op);
    }
    return sendJson(req, json, response_);
}

esp_err_t WebServerManager::csv(httpd_req_t* req) {
    if (!port_.beginDownload())
        return error(req, 409, "TEST_OR_DOWNLOAD_BUSY", "Der Versuch ist nicht zum Download verfügbar.");
    struct Lease { NetworkPort& port; ~Lease() { port.endDownload(); } } lease{port_};
    httpd_resp_set_type(req, "text/csv; charset=utf-8");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Content-Disposition", "attachment; filename=hasenklima-sprungantwort.csv");
    const int64_t deadline = esp_timer_get_time() + 180000000;
    char line[512], buffer[1024];
    size_t used = 0;
    int result;
    while ((result = port_.nextCsv(line, sizeof(line))) > 0) {
        if (esp_timer_get_time() >= deadline || result > static_cast<int>(sizeof(line))) return ESP_FAIL;
        if (used + result > sizeof(buffer)) {
            if (httpd_resp_send_chunk(req, buffer, used) != ESP_OK) return ESP_FAIL;
            used = 0;
        }
        memcpy(buffer + used, line, result);
        used += result;
    }
    if (result < 0 || (used && httpd_resp_send_chunk(req, buffer, used) != ESP_OK)) return ESP_FAIL;
    return httpd_resp_send_chunk(req, nullptr, 0);
}

esp_err_t WebServerManager::write(httpd_req_t* req, const char* path) {
    if (strncmp(path, kApi, sizeof(kApi) - 1)) return error(req, 404, "NOT_FOUND", "Unbekannter Pfad.");
    path += sizeof(kApi) - 1;
    const bool login = pathMatch(path, "/auth/login") && req->method == HTTP_POST;
    Config current = port_.config();
    char token[33]{};
    httpd_req_get_hdr_value_str(req, "X-Klima-Session", token, sizeof(token));
    const auto nowUs = static_cast<uint64_t>(esp_timer_get_time());
    const uint32_t owner = login ? 0 : auth_.authorize(token, current.devicePasswordHash, nowUs);
    if (!login && !owner) return error(req, 401, "LOGIN_REQUIRED", "Bitte mit dem Gerätepasswort anmelden.");
    if (!readBody(req, body_, sizeof(body_))) {
        error(req, 400, "INVALID_JSON", "Ungültige oder zu große JSON-Anfrage.");
        return ESP_FAIL;
    }
    JsonPtr parsed(cJSON_ParseWithOpts(body_, nullptr, true), &cJSON_Delete);
    const cJSON* doc = parsed.get();
    if (!cJSON_IsObject(doc) || !uniqueKeys(doc)) return error(req, 400, "INVALID_JSON", "Ein JSON-Objekt mit eindeutigen Feldern ist erforderlich.");
    auto bad = [req] { return error(req, 400, "INVALID_REQUEST", "Pflichtfeld fehlt oder ein Feld hat einen unzulässigen Typ."); };
    auto invalid = [req](const char* text) { return error(req, 422, "INVALID_VALUE", text); };
    JsonWriter json(response_, sizeof(response_));
    if (login) {
        char password[129], session[33];
        if (!fields(doc, "password") || !copyText(password, sizeof(password), field(doc, "password"))) return bad();
        if (!current.devicePasswordHash[0]) return error(req, 403, "LOCAL_SETUP_REQUIRED", "Das Gerätepasswort zuerst lokal bei der Inbetriebnahme setzen.");
        const bool authenticated = auth_.login(password, current.devicePasswordHash, nowUs, session);
        memset(password, 0, sizeof(password));
        if (!authenticated) return error(req, 401, "INVALID_LOGIN", "Anmeldung fehlgeschlagen. Bei erneutem Versuch kurz warten.");
        json.object();
        json.integer("schema_version", 1);
        json.text("session_token", session);
        json.integer("owner_id", auth_.authorize(session, current.devicePasswordHash, nowUs));
        json.end();
        return sendJson(req, json, response_);
    }
    if (pathMatch(path, "/auth/logout") && req->method == HTTP_POST) {
        if (!fields(doc)) return bad();
        auth_.logout(token);
        return send(req, 200, "{\"schema_version\":1}", 20);
    }
    Command cmd;
    cmd.ownerId = owner;
    if (pathMatch(path, "/config") && req->method == HTTP_PATCH) {
        cmd.kind = CommandKind::PatchConfig;
        if (!fields(doc, "expected_revision", "request_id", "values") ||
            !revisionField(doc, "expected_revision", cmd, true) || !requestId(doc, cmd, true)) return bad();
        const cJSON* changes = field(doc, "values");
        if (!cJSON_IsObject(changes) || !changes->child) return bad();
        cmd.config = current;
        for (const cJSON* item = changes->child; item; item = item->next) {
            const char* key = item->string;
            if (!strcmp(key, "heating_enabled") || !strcmp(key, "cooling_enabled") || !strcmp(key, "evaporation_enabled"))
                return error(req, 422, "FUNCTIONS_ENDPOINT_REQUIRED", "Hauptfreigaben werden über den Funktionsendpunkt geändert.");
            if (!strcmp(key, "device_password")) {
                if (!cJSON_IsString(item)) return bad();
                if (!makePasswordHash(item->valuestring, cmd.config.devicePasswordHash, sizeof(cmd.config.devicePasswordHash)))
                    return invalid("Das Gerätepasswort muss 12 bis 128 Byte lang sein.");
                continue;
            }
            const auto* def = findParameter(key);
            if (!def || !def->writable || !strcmp(key, "device_password_hash"))
                return error(req, 422, "PARAMETER_NOT_WRITABLE", "Der Parameter ist unbekannt oder ausschließlich lesbar.");
            bool ok;
            switch (def->type) {
            case ParameterType::Boolean:
                if (!cJSON_IsBool(item)) return bad();
                ok = writeNumber(cmd.config, *def, cJSON_IsTrue(item) ? 1 : 0);
                break;
            case ParameterType::String:
                if (!cJSON_IsString(item)) return bad();
                ok = writeText(cmd.config, *def, item->valuestring);
                break;
            default:
                if (!cJSON_IsNumber(item) && !cJSON_IsNull(item)) return bad();
                ok = writeNumber(cmd.config, *def, cJSON_IsNull(item) ? kNetUnavailable : item->valuedouble);
                break;
            }
            if (!ok) return invalid("Ein Parameter liegt außerhalb seines gültigen Bereichs.");
        }
    } else if (pathMatch(path, "/functions") && req->method == HTTP_POST) {
        cmd.kind = CommandKind::Functions;
        const char* keys[] = {"heating_enabled", "cooling_enabled", "evaporation_enabled"};
        if (!fields(doc, keys[0], keys[1], keys[2], "expected_revision", "request_id") ||
            !revisionField(doc, "expected_revision", cmd, false) || !requestId(doc, cmd, false)) return bad();
        for (unsigned i = 0; i < 3; ++i) {
            bool value;
            if (!field(doc, keys[i])) continue;
            if (!boolField(doc, keys[i], value, true)) return bad();
            cmd.functionMask |= 1u << i;
            if (value) cmd.functionValues |= 1u << i;
        }
        if (!cmd.functionMask) return bad();
    } else if (pathMatch(path, "/sensors/scan") && req->method == HTTP_POST) {
        cmd.kind = CommandKind::Scan;
        if (!fields(doc)) return bad();
    } else if (req->method == HTTP_PUT && !strncmp(path, "/sensors/plate/", 15) && strlen(path) == 16 &&
               path[15] >= '1' && path[15] <= '9') {
        cmd.kind = CommandKind::PlateSensor;
        cmd.plateIndex = path[15] - '1';
        const auto& existing = current.plate[cmd.plateIndex];
        cmd.romId = existing.romId;
        cmd.enabled = existing.enabled;
        if (!fields(doc, "rom_id", "enabled", "confirm_disable", "expected_revision", "request_id") ||
            !revisionField(doc, "expected_revision", cmd, true) || !requestId(doc, cmd, false) ||
            !romField(doc, "rom_id", cmd.romId) || !boolField(doc, "enabled", cmd.enabled) ||
            !boolField(doc, "confirm_disable", cmd.confirmDisable)) return bad();
        if (existing.enabled && !cmd.enabled && !cmd.confirmDisable)
            return invalid("Deaktivieren ausdrücklich bestätigen. An dieser Position entfällt die Temperaturüberwachung.");
    } else if (pathMatch(path, "/sensors/tank") && req->method == HTTP_PUT) {
        cmd.kind = CommandKind::TankSensor;
        if (!fields(doc, "rom_id", "expected_revision", "request_id") || !field(doc, "rom_id") ||
            !revisionField(doc, "expected_revision", cmd, true) || !requestId(doc, cmd, false) ||
            !romField(doc, "rom_id", cmd.romId)) return bad();
    } else if ((pathMatch(path, "/manual/start") || pathMatch(path, "/manual/output")) && req->method == HTTP_POST) {
        const bool start = pathMatch(path, "/manual/start");
        cmd.kind = start ? CommandKind::ManualStart : CommandKind::ManualOutput;
        cmd.durationSeconds = start ? current.manualTimeoutS : 0;
        float percent = 0;
        if (!fields(doc, "duration_s", "duration_seconds", "session_id", "heater_pwm_percent", "cooling", "evaporation", "ignore_dewpoint", "expected_revision", "request_id") ||
            !revisionField(doc, "expected_revision", cmd, false) || !requestId(doc, cmd, false) ||
            (field(doc, "duration_s") && field(doc, "duration_seconds")) ||
            !uintField(doc, "duration_s", cmd.durationSeconds, false) ||
            !uintField(doc, "duration_seconds", cmd.durationSeconds, false) ||
            !uintField(doc, "session_id", cmd.sessionId, !start) ||
            !pwmField(doc, "heater_pwm_percent", percent, true) || !boolField(doc, "cooling", cmd.cooling) ||
            !boolField(doc, "evaporation", cmd.evaporation) || !boolField(doc, "ignore_dewpoint", cmd.ignoreDewPoint) ||
            (!start && field(doc, "ignore_dewpoint")) || (start && field(doc, "session_id"))) return bad();
        const bool explicitTime = field(doc, "duration_s") || field(doc, "duration_seconds");
        if (percent < 0 || percent > 100 || ((start || explicitTime) && (cmd.durationSeconds < 60 || cmd.durationSeconds > 3600)))
            return invalid("PWM muss zwischen 0 und 100 % und die Sitzungsdauer zwischen 1 und 60 Minuten liegen.");
        if (percent > 0 && (cmd.cooling || cmd.evaporation))
            return error(req, 422, "OUTPUT_CONFLICT", "Heizung und Pumpen dürfen nicht gemeinsam angefordert werden.");
        cmd.heaterDuty = percent / 100.f;
    } else if (pathMatch(path, "/test/start") && req->method == HTTP_POST) {
        cmd.kind = CommandKind::TestStart;
        uint32_t baseline, heating, cooldown;
        if (!fields(doc, "baseline_minutes", "heat_minutes", "cooldown_minutes", "pwm_percent", "replace_previous_test", "expected_config_revision", "request_id") ||
            !revisionField(doc, "expected_config_revision", cmd, true) || !requestId(doc, cmd, true) ||
            !uintField(doc, "baseline_minutes", baseline) || !uintField(doc, "heat_minutes", heating) ||
            !uintField(doc, "cooldown_minutes", cooldown) || !pwmField(doc, "pwm_percent", cmd.test.pwmPercent, true) ||
            !boolField(doc, "replace_previous_test", cmd.confirmed, true)) return bad();
        if (baseline > 60 || heating == 0 || heating > 500 || cooldown > 60)
            return invalid("Vorlauf und Nachlauf: 0–60 Minuten. Heizphase: 1–500 Minuten.");
        cmd.test.baselineMinutes = baseline;
        cmd.test.heatMinutes = heating;
        cmd.test.cooldownMinutes = cooldown;
        if (!cmd.test.valid()) return invalid("Die Test-PWM muss größer als 0 und höchstens 100 % sein.");
        if (!cmd.confirmed) return invalid("Das Ersetzen des gespeicherten Versuchs muss bestätigt werden.");
    } else if (req->method == HTTP_POST && !strncmp(path, "/faults/", 8) && strlen(path) > 12 &&
               !strcmp(path + strlen(path) - 4, "/ack")) {
        cmd.kind = CommandKind::Acknowledge;
        const size_t length = strlen(path) - 12;
        if (length >= sizeof(cmd.faultCode) || !fields(doc)) return bad();
        memcpy(cmd.faultCode, path + 8, length);
        for (size_t i = 0; i < length; ++i) {
            const char ch = cmd.faultCode[i];
            if ((ch < 'A' || ch > 'Z') && (ch < '0' || ch > '9') && ch != '_') return bad();
        }
    } else if (req->method == HTTP_POST && (pathMatch(path, "/manual/stop") || pathMatch(path, "/test/stop") || pathMatch(path, "/stop"))) {
        cmd.kind = pathMatch(path, "/manual/stop") ? CommandKind::ManualStop :
            pathMatch(path, "/test/stop") ? CommandKind::TestStop : CommandKind::StopAll;
        if (!fields(doc)) return bad();
    } else if (req->method == HTTP_POST && (pathMatch(path, "/energy/reset") || pathMatch(path, "/restart"))) {
        cmd.kind = pathMatch(path, "/energy/reset") ? CommandKind::ResetEnergy : CommandKind::Restart;
        if (!fields(doc, "confirmed") || !boolField(doc, "confirmed", cmd.confirmed, true)) return bad();
        if (!cmd.confirmed) return invalid("Die Wartungsaktion muss bestätigt werden.");
    } else {
        return error(req, 404, "NOT_FOUND", "Unbekannter Pfad oder Methode.");
    }
    const auto op = port_.submit(cmd);
    operationJson(json, op);
    return sendJson(req, json, response_, op.status);
}

} // namespace klima
