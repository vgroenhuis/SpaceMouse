// SpaceMouse v2, ESP32-C5 side (ESP-IDF version of ../esp32-c5): a Teensy 4.1 reads a 3Dconnexion SpaceMouse over USB
// and sends its axes and buttons over a serial line; this board shows them on an OLED and serves them on the web. Same
// pins, serial protocol and pages as the Arduino version; new: Wi-Fi settings in flash (no RoamingWiFiManager),
// reporting to the CloSer lab demos page, over-the-air updates with rollback, a 50 Hz WebSocket stream, the display
// telling what Wi-Fi is doing.
#include "closer_demo.h"
#include "display.h"
#include "esp_log.h"
#include "led.h"
#include "teensy.h"
#include "web.h"

void app_main(void) {
    display_start();
    display_boot_message("starting...");
    closer_demo_start("spacemouse");   // NVS, Wi-Fi, clock, reporting, updates (in the background)
    led_start();
    teensy_start();
    web_start();
    display_run();
    ESP_LOGI("spacemouse", "running, firmware %s", closer_demo_version());
}
