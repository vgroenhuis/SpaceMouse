#pragma once
#include <string>
#include "../../common/include/SpaceMouseData.h"

// Teensy connection states
enum class TeensyState {
    DISCONNECTED,
    NO_MOUSE,
    CONNECTED
};

extern SpaceMouseData spaceMouseData;
extern unsigned long lastSpaceMouseDataTime;

void setupTeensy();
void loopTeensy();

std::string getTeensyStatus();
TeensyState getTeensyState();
