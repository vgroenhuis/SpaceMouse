#pragma once

/*
    0.96" OLED display

    D0, D1, RES, DC, CS: ESP32-C5 0, 1, 6, 7, 8
*/

#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <SPI.h>


const int OLED_CLK = 0;
const int OLED_MOSI = 1;
const int OLED_RESET = 6;
const int OLED_DC = 7;
const int OLED_CS = 8;

const int SCREEN_WIDTH = 128;
const int SCREEN_HEIGHT = 64;

extern Adafruit_SSD1306 display;
extern SPIClass spi;

void setupDisplay();

void updateDisplay();