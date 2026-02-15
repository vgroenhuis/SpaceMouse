#include <Arduino.h>
#include <ArduinoJson.h>
#include "teensy.h"

HardwareSerial SerialTeensy(1); // Use UART1 for Teensy communication

// State machine
TeensyState teensyState = TeensyState::DISCONNECTED;
SpaceMouseData spaceMouseData;
unsigned long lastSpaceMouseDataTime = 0;

// Timing for handshake
unsigned long lastPingTime = 0;
unsigned long lastPongTime = 0;
const unsigned long PING_INTERVAL = 100; // Send ping this interval
const unsigned long PONG_TIMEOUT = 200;  // Consider disconnected if no pong this interval

// JSON parsing buffer
String jsonBuffer = "";

void setupTeensy() {
    SerialTeensy.begin(921600, SERIAL_8N1, 3, 2); // rxPin = 3, txPin = 2
    teensyState = TeensyState::DISCONNECTED;
}

void sendPing() {
    JsonDocument doc;
    doc["type"] = "ping";
    serializeJson(doc, SerialTeensy);
    SerialTeensy.println();
    lastPingTime = millis();
}

void handlePong(JsonDocument& doc) {
    lastPongTime = millis();
    if (doc["spacemouse_connected"]) {
        teensyState = TeensyState::CONNECTED;
    } else {
        teensyState = TeensyState::NO_MOUSE;
    }
}

void handleUpdate(JsonDocument& doc) {
    // Parse sm_xyz array
    JsonArray xyz = doc["sm_xyz"];
    if (xyz.size() == 3) {
        spaceMouseData.xyz[0] = xyz[0];
        spaceMouseData.xyz[1] = xyz[1];
        spaceMouseData.xyz[2] = xyz[2];
    }
    
    // Parse sm_rxyz array
    JsonArray rxyz = doc["sm_rxyz"];
    if (rxyz.size() == 3) {
        spaceMouseData.rxyz[0] = rxyz[0];
        spaceMouseData.rxyz[1] = rxyz[1];
        spaceMouseData.rxyz[2] = rxyz[2];
    }
    
    // Parse sm_buttons
    if (doc["sm_buttons"].is<uint16_t>()) {
        spaceMouseData.buttons = doc["sm_buttons"];
    }
    
    // Update timestamp
    lastSpaceMouseDataTime = millis();
}

/* Assume that message is complete */
void processJsonMessage(const String& message) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);
    
    if (error) {
        Serial.print("JSON parse error: ");
        Serial.println(error.c_str());
        return;
    }
    
    const char* type = doc["type"];
    if (type == nullptr) {
        Serial.println("Message missing 'type' field");
        return;
    }
    
    if (strcmp(type, "pong") == 0) {
        handlePong(doc);
    } else if (strcmp(type, "update") == 0) {
        handleUpdate(doc);
    } else {
        Serial.print("Unknown message type: ");
        Serial.println(type);
    }
}

void readTeensySerial() {
    while (SerialTeensy.available()) {
        char c = SerialTeensy.read();
        
        if (c == '\n') {
            // Complete message received
            if (jsonBuffer.length() > 0) {
                processJsonMessage(jsonBuffer);
                jsonBuffer = "";
            }
        } else if (c != '\r') {
            jsonBuffer += c;
        }
    }
}

void checkConnectionTimeout() {
    unsigned long now = millis();
    
    // Check if we should send a ping
    if (now - lastPingTime >= PING_INTERVAL) {
        sendPing();
    }
    
    // Check for timeout (only if we've sent at least one ping)
    if (lastPingTime > 0 && now - lastPongTime >= PONG_TIMEOUT) {
        if (teensyState == TeensyState::CONNECTED) {
            teensyState = TeensyState::DISCONNECTED;
            Serial.println("Teensy disconnected (timeout)");
        }
    }
}

void loopTeensy() {
    readTeensySerial();
    checkConnectionTimeout();
}

std::string getTeensyStatus() {
    if (teensyState == TeensyState::CONNECTED) {
        char buffer[100];
        char buttonChr;
        if (spaceMouseData.buttons == 0) {
            buttonChr = '-';
        } else if (spaceMouseData.buttons == 1) {
            buttonChr = 'L';
        } else if (spaceMouseData.buttons == 2) {
            buttonChr = 'R';
        } else {
            buttonChr = 'A';
        }

        snprintf(buffer, sizeof(buffer), "T %d %d %d\nR %d %d %d %c\n",
            spaceMouseData.xyz[0], spaceMouseData.xyz[1], spaceMouseData.xyz[2],
            spaceMouseData.rxyz[0], spaceMouseData.rxyz[1], spaceMouseData.rxyz[2],
            buttonChr);
        return std::string(buffer);
    } else if (teensyState == TeensyState::NO_MOUSE) {
        return "No Mouse";
    } else {
        return "No Teensy";
    }
}

TeensyState getTeensyState() {
    return teensyState;
}
