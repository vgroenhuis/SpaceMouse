#include "TeensyUtils.h"
#include "UsbHost.h"

void sendJsonToMicroUsb(JsonDocument &doc) {
    if (Serial) {
        String out;
        serializeJson(doc, out);
        Serial.println(out);
    }
}

void sendWelcomeInfo() {
    Serial.println("[INFO] Welcome to SpaceMouse application!");

    JsonDocument doc;
    doc["proto_ver"] = 1;
    doc["device"] = "teensy41";
    doc["spacemouse_connected"] = spaceMouseConnected();
    sendJsonToMicroUsb(doc);
}
