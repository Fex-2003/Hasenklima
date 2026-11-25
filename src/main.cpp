#include "runtime/Runtime.h"
#include <Arduino.h>

namespace {
klima::Runtime runtime;
}

void setup() {
    runtime.begin();
}

void loop() {
    vTaskDelay(pdMS_TO_TICKS(1000));
}
