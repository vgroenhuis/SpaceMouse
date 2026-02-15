#pragma once
#include <stdint.h>

struct SpaceMouseData {
    int16_t xyz[3];
    int16_t rxyz[3];
    uint16_t buttons; // each bit represents a button, 1 = pressed, 0 = not pressed
};