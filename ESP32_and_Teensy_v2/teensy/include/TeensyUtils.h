#pragma once
#include <ArduinoJson.h>

// Sends JSON document to serial over microUsb which is a programming and debugging port
void sendJsonToMicroUsb(JsonDocument &doc);

// Sends some welcome info to the debugging serial port
void sendWelcomeInfo();