/*
    Main script for SpaceMouse application on Teensy 4.1.
*/

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "version.h"
#include "UsbHost.hpp"

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
        Serial.printf("[ERROR] Error starting %s\n", "Program flash DISK");
        while(1) {
            blinkLed(1.0f, 0.9f);  // Blink LED at 1Hz with 90% duty cycle
        }
    } else {
        Serial.println("[INFO] LittleFS initialized successfully");
    }
}

void sendWelcomeInfo() {
    Serial.println("[INFO] Welcome to SpaceMouse application!");

    JsonDocument doc;
    doc["proto_ver"] = 1;
    doc["fw_ver"] = FW_VERSION_STRING;
    doc["device"] = "teensy41";
    doc["spacemouse_connected"] = spaceMouseConnected();
    sendJson(doc);
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);

    Serial.begin(115200);

    if (WAIT_FOR_HOST_SERIAL) {
        while (!Serial) {
            blinkLed(10.0f, 0.5f);  // Blink LED at 10Hz while waiting for Serial
        }
    }

    initLittleFS();
    setupUsbHost();
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

    usbHostLoop();
    if (spaceMouseConnected()) {
        blinkLed(2.0f, 0.9f); // 90% duty cycle when SpaceMouse is connected
    } else {
        blinkLed(1.0f, 0.1f); // 10% duty cycle
    }
}
