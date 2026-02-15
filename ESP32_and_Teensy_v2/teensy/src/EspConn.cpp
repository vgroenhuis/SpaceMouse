#include "EspConn.h"
#include "TeensyUtils.h"
#include "UsbHost.h"
#include <ArduinoJson.h>

void initEspConn() {
    Serial1.begin(921600);
}

void sendJsonToEsp(JsonDocument &doc) {
    String out;
    serializeJson(doc, out);
    Serial1.println(out);
    if (1) {
        sendJsonToMicroUsb(doc); // Also send to micro USB for debugging
    }
}

void sendPong() {
    // Send pong response back to ESP over Serial1
    JsonDocument pongDoc;
    pongDoc["type"] = "pong";
    pongDoc["spacemouse_connected"] = spaceMouseConnected();
    sendJsonToEsp(pongDoc);
}

void processIncomingEspData() {
    static String serial1_buffer = "";
    
    // Read available data from Serial1 (ESP connection)
    while (Serial1.available() > 0) {
        char c = Serial1.read();
        
        if (c == '\n') {
            // Complete line received, try to parse as JSON
            if (serial1_buffer.length() > 0) {
                JsonDocument doc;
                DeserializationError error = deserializeJson(doc, serial1_buffer);
                
                if (!error) {
                    // Check if this is a ping request
                    if (doc["type"] == "ping") {
                        sendPong();
                    }
                    
                    // Valid JSON received, forward to micro USB
                    //sendJsonToMicroUsb(doc);
                }
                // If error, silently ignore invalid JSON
                
                serial1_buffer = "";
            }
        } else if (c != '\r') {
            // Add character to buffer (skip carriage return)
            serial1_buffer += c;
        }
    }
}

void sendJoystickDataToEsp() {
    if (spaceMouseConnected()) {
        if (getAxisOrButtonChanged()) {
            resetAxisOrButtonChanged();
            JsonDocument doc = getCurrentSpaceMouseDataAsJson();
            sendJsonToEsp(doc);
        }
    }
}

void loopEspConn() {
    processIncomingEspData();
    sendJoystickDataToEsp();
}
