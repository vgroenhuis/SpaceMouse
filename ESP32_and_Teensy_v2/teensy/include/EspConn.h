#pragma once
#include <Arduino.h>
/*
    Implements connection to ESP32.
    The ESP32 is the leader and the Teensy is the follower.
   
*/

void initEspConn();

void loopEspConn();

