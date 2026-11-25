#pragma once

#include "JsonWriter.h"
#include "NetworkContract.h"

namespace klima {

struct NetworkStatus { bool wifiOnline{}, mqttOnline{}, httpReady{}, csvReady{}; };
void writeSnapshot(JsonWriter& json, const NetworkSnapshot& state, const NetworkStatus& network);
void writeTest(JsonWriter& json, const NetworkSnapshot& state);
void writeFault(JsonWriter& json, const NetFault& fault);
void writeMqttPayload(JsonWriter& json, const char* relativeTopic, const NetworkSnapshot& state,
                      const NetworkStatus& network, unsigned plateIndex = 0);
void writeHeader(JsonWriter& json, const NetworkSnapshot& state);
void writeConfig(JsonWriter& json, const Config& config);
void writeParameters(JsonWriter& json);

} // namespace klima
