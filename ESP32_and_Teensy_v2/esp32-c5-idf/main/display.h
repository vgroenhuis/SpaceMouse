// The 0.96" SSD1306 OLED (128x64, SPI): address, Wi-Fi, the SpaceMouse's axes and buttons, problems.
#pragma once

void display_start(void);
void display_boot_message(const char *text);
void display_run(void);                         // starts the refresh task (10 Hz)
