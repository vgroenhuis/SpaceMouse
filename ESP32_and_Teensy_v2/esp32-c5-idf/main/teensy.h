// The Teensy 4.1 (USB host for the SpaceMouse) on UART1: newline-delimited JSON, as the Arduino version.
//   ESP32 -> Teensy  {"type":"ping"} every 100 ms
//   Teensy -> ESP32  {"type":"pong","spacemouse_connected":true|false}
//                    {"type":"update","sm_xyz":[x,y,z],"sm_rxyz":[rx,ry,rz],"sm_buttons":n}   (when something changes)
// No pong within 200 ms: the Teensy counts as disconnected.
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum { TEENSY_DISCONNECTED = 0, TEENSY_NO_MOUSE = 1, TEENSY_CONNECTED = 2 } teensy_state_t;

typedef struct {
    teensy_state_t state;
    int16_t xyz[3];
    int16_t rxyz[3];
    uint16_t buttons;          // bit 0 left, bit 1 right
    uint32_t updates;          // update messages since start
    float messages_per_s;      // all messages from the Teensy, last second
    uint32_t parse_errors;
} teensy_snapshot_t;

void teensy_start(void);
void teensy_get(teensy_snapshot_t *out);
const char *teensy_state_text(teensy_state_t s);
