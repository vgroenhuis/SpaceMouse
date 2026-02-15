/*
    Main script for SpaceMouse application on Teensy 4.1.
*/

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Watchdog_t4.h>
#include "UsbHost.h"
#include "TeensyUtils.h"
#include "EspConn.h"

WDT_T4<WDT1> wdt;

const bool WAIT_FOR_HOST_SERIAL = false; // set true for debugging, false for production

LittleFS_Program myfs;

void blinkLed(float freq, float duty) {
    static unsigned long lastBlink = 0;
    static bool ledState = false;
    unsigned long now = millis();
    unsigned long onDuration = (unsigned long)(1000.0f * duty / freq);
    unsigned long offDuration = (unsigned long)(1000.0f * (1.0f - duty) / freq);
    
    if (ledState && (now - lastBlink >= onDuration)) {  // LED on for onDuration
        digitalWrite(LED_BUILTIN, LOW);
        ledState = false;
        lastBlink = now;
    } else if (!ledState && (now - lastBlink >= offDuration)) {  // LED off for offDuration
        digitalWrite(LED_BUILTIN, HIGH);
        ledState = true;
        lastBlink = now;
    }
}

void initLittleFS() {
    if (!myfs.begin(1024 * 1024 * 1)) { // 1 MB for filedata
        // LittleFS failed - log error but continue operation
        // (allows standalone operation without PC connection)
        if (Serial) {
            Serial.printf("[WARNING] LittleFS initialization failed - continuing without filesystem\n");
        }
    } else {
        if (Serial) {
            Serial.println("[INFO] LittleFS initialized successfully");
        }
    }
}

void initWatchdog() {
  WDT_timings_t config;
  config.timeout = 4;
  wdt.begin(config);    
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.begin(115200); // PC-Teensy

    if (WAIT_FOR_HOST_SERIAL) {
        while (!Serial) {
            blinkLed(10.0f, 0.5f);  // Blink LED at 10Hz while waiting for Serial
        }
    }

    initWatchdog();
    initLittleFS();
    initEspConn();
    initUsbHost();
}

void loop()
{
    static bool welcomeInfoSent = false;

    if (Serial) {
        if (!welcomeInfoSent) {
            sendWelcomeInfo();
            welcomeInfoSent = true;
        }
    } else {
        welcomeInfoSent = false;
    }

    loopUsbHost();
    loopEspConn();

    if (spaceMouseConnected()) {
        blinkLed(1.0f, 0.95f); // 95% duty cycle when SpaceMouse is connected
    } else {
        blinkLed(1.0f, 0.05f); // 10% duty cycle if nothing is connected
    }
    wdt.feed();
}
