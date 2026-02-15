// To build and run without auto-cleaning the build folder:
// pio run --disable-auto-clean -t upload


#include <Arduino.h>

#include "web.h"
#include "display.h"
#include "teensy.h"

void setup() {
    Serial.begin(115200); // UART
    Serial.println("SpaceMouse ESP32-C5 starting...");

    setupDisplay();
    setupTeensy();
    setupWeb();
}

void loop() {
    loopWeb();
    loopTeensy();

    static unsigned long lastDisplayUpdate = 0;
    if (millis() - lastDisplayUpdate > 50) {
        updateDisplay();
        lastDisplayUpdate = millis();
    }
}

