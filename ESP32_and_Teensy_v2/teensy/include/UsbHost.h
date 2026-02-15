#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>


bool spaceMouseConnected();

// returns true if any axis or button value has changed since last call to resetAxisOrButtonChanged()
bool getAxisOrButtonChanged();

void resetAxisOrButtonChanged();

JsonDocument getCurrentSpaceMouseDataAsJson();

void initUsbHost();
void loopUsbHost();