// The 0.96" SSD1306 OLED (128x64, 4-wire SPI: D0=CLK, D1=MOSI, RES, DC, CS), drawn with u8g2 (components/u8g2), the
// SPI bit-banged on GPIOs (as Adafruit_SSD1306 does; it needs no SPI peripheral and is fast enough at 10 frames/s):
//
//   connected                          connecting
//   SpaceMouse                [wifi]   SpaceMouse              [x]
//   145.126.38.161   (bold, large)     Searching for          (bold)
//   -47 dBm  ch 124  5 GHz             iotroam ...
//   ---------------------------------  ch 124 · attempt 2
//   T     120    -35      0            ---------------------------------
//   R       0     12   -250            SpaceMouse: connected
//   connected             [L] [R]
#include "display.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "closer_demo.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_rom_sys.h"
#include "teensy.h"
#include "u8g2.h"

static u8g2_t u8g2;
static bool started;

// ---------------------------------------------------------------- u8g2 on GPIOs

static uint8_t gpio_cb(u8x8_t *u8x8, uint8_t msg, uint8_t arg_int, void *arg_ptr) {
    switch (msg) {
        case U8X8_MSG_GPIO_AND_DELAY_INIT: {
            const int pins[] = {CONFIG_SPACEMOUSE_OLED_CLK_GPIO, CONFIG_SPACEMOUSE_OLED_MOSI_GPIO, CONFIG_SPACEMOUSE_OLED_RES_GPIO,
                                CONFIG_SPACEMOUSE_OLED_DC_GPIO, CONFIG_SPACEMOUSE_OLED_CS_GPIO};
            for (size_t i = 0; i < sizeof pins / sizeof pins[0]; i++) {
                gpio_reset_pin(pins[i]);
                gpio_set_direction(pins[i], GPIO_MODE_OUTPUT);
            }
            break;
        }
        case U8X8_MSG_GPIO_SPI_CLOCK: gpio_set_level(CONFIG_SPACEMOUSE_OLED_CLK_GPIO, arg_int); break;
        case U8X8_MSG_GPIO_SPI_DATA: gpio_set_level(CONFIG_SPACEMOUSE_OLED_MOSI_GPIO, arg_int); break;
        case U8X8_MSG_GPIO_CS: gpio_set_level(CONFIG_SPACEMOUSE_OLED_CS_GPIO, arg_int); break;
        case U8X8_MSG_GPIO_DC: gpio_set_level(CONFIG_SPACEMOUSE_OLED_DC_GPIO, arg_int); break;
        case U8X8_MSG_GPIO_RESET: gpio_set_level(CONFIG_SPACEMOUSE_OLED_RES_GPIO, arg_int); break;
        case U8X8_MSG_DELAY_MILLI: vTaskDelay(pdMS_TO_TICKS(arg_int) ? pdMS_TO_TICKS(arg_int) : 1); break;
        case U8X8_MSG_DELAY_10MICRO: esp_rom_delay_us(10 * arg_int); break;
        case U8X8_MSG_DELAY_100NANO: break;
        case U8X8_MSG_DELAY_NANO: break;
        default: break;
    }
    return 1;
}

// ---------------------------------------------------------------- drawing helpers (as in the HandTool firmware)

static void textf(int x, int y, const uint8_t *font, const char *fmt, ...) {
    char s[48];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s, sizeof s, fmt, ap);
    va_end(ap);
    u8g2_SetFont(&u8g2, font);
    u8g2_DrawStr(&u8g2, x, y, s);
}

static void text_right(int x_right, int y, const uint8_t *font, const char *s) {
    u8g2_SetFont(&u8g2, font);
    u8g2_DrawStr(&u8g2, x_right - u8g2_GetStrWidth(&u8g2, s), y, s);
}

static void wifi_icon(int x, int y, int rssi) {
    int bars = rssi == 0 ? 0 : rssi > -55 ? 4 : rssi > -65 ? 3 : rssi > -75 ? 2 : 1;
    for (int i = 0; i < 4; i++) {
        int h = 3 + 2 * i;
        if (i < bars) u8g2_DrawBox(&u8g2, x + 3 * i, y - h, 2, h);
        else u8g2_DrawPixel(&u8g2, x + 3 * i, y - 1);
    }
    if (!bars) {
        u8g2_DrawLine(&u8g2, x + 6, y - 9, x + 11, y - 4);
        u8g2_DrawLine(&u8g2, x + 11, y - 9, x + 6, y - 4);
    }
}

static int wrapped(int x, int y, int line_h, int width, int max_lines, const uint8_t *font, const char *s) {
    u8g2_SetFont(&u8g2, font);
    char line[48];
    int lines = 0;
    while (*s && lines < max_lines) {
        size_t best = 0, n = 0;
        while (s[n]) {
            size_t m = n;
            while (s[m] && s[m] != ' ') m++;
            size_t k = m < sizeof line - 1 ? m : sizeof line - 1;
            memcpy(line, s, k);
            line[k] = 0;
            if (best && u8g2_GetStrWidth(&u8g2, line) > width) break;
            best = k;
            if (!s[m]) break;
            n = m + 1;
        }
        memcpy(line, s, best);
        line[best] = 0;
        u8g2_DrawStr(&u8g2, x, y + lines * line_h, line);
        lines++;
        s += best;
        while (*s == ' ') s++;
    }
    return lines;
}

// A labelled row of three values, right-aligned in columns.
static void triple(int y, const char *label, const int16_t v[3]) {
    textf(0, y, u8g2_font_helvB08_tr, "%s", label);
    for (int i = 0; i < 3; i++) {
        char s[8];
        snprintf(s, sizeof s, "%d", v[i]);
        text_right(48 + 38 * i, y, u8g2_font_6x10_tr, s);
    }
}

static void button_box(int x, int y, const char *label, bool on) {
    if (on) {
        u8g2_DrawRBox(&u8g2, x, y - 8, 13, 10, 2);
        u8g2_SetDrawColor(&u8g2, 0);
        textf(x + 4, y, u8g2_font_5x8_tr, "%s", label);
        u8g2_SetDrawColor(&u8g2, 1);
    } else {
        u8g2_DrawRFrame(&u8g2, x, y - 8, 13, 10, 2);
        textf(x + 4, y, u8g2_font_5x8_tr, "%s", label);
    }
}

static void teensy_lines(int y, const teensy_snapshot_t *t) {
    if (t->state == TEENSY_CONNECTED) {
        triple(y, "T", t->xyz);
        triple(y + 10, "R", t->rxyz);
        textf(58, y + 19, u8g2_font_5x8_tr, "buttons");
        button_box(98, y + 19, "L", t->buttons & 1);
        button_box(113, y + 19, "R", t->buttons & 2);
    } else {
        textf(0, y + 4, u8g2_font_helvB08_tr, t->state == TEENSY_NO_MOUSE ? "No SpaceMouse" : "No Teensy");
        textf(0, y + 16, u8g2_font_5x8_tr, t->state == TEENSY_NO_MOUSE ? "plug it into the Teensy" : "check its wires and power");
    }
}

static void draw(const closer_demo_status_t *st, const teensy_snapshot_t *t, int tick) {
    u8g2_ClearBuffer(&u8g2);
    textf(0, 9, u8g2_font_helvB08_tr, "SpaceMouse");
    wifi_icon(115, 9, st->connected ? (st->rssi ? st->rssi : -60) : 0);
    if (st->updating) {
        textf(0, 30, u8g2_font_helvB10_tr, "Updating");
        textf(0, 44, u8g2_font_helvB10_tr, "firmware%.*s", tick / 3 % 4, "...");
        textf(0, 60, u8g2_font_6x10_tr, "Don't switch off");
    } else if (st->connected) {
        textf(0, 23, u8g2_font_helvB10_tr, "%s", st->ip);
        textf(0, 32, u8g2_font_5x8_tr, "%d dBm  ch %d  %s", st->rssi, st->channel, st->channel > 14 ? "5 GHz" : "2.4 GHz");
        u8g2_DrawHLine(&u8g2, 0, 34, 128);
        teensy_lines(44, t);
    } else {
        char state[64];
        bool busy = strncmp(st->wifi, "Searching", 9) == 0 || strncmp(st->wifi, "Getting", 7) == 0 ||
                    strncmp(st->wifi, "Starting", 8) == 0 || strncmp(st->wifi, "Moving", 6) == 0;
        snprintf(state, sizeof state, "%s%.*s", st->wifi, busy ? tick / 3 % 4 : 0, "...");
        int lines = wrapped(0, 22, 11, 127, 2, u8g2_font_helvB08_tr, state);
        char detail[48] = "";
        if (st->channel) snprintf(detail, sizeof detail, "ch %d  ", st->channel);
        if (st->attempts > 1) snprintf(detail + strlen(detail), sizeof detail - strlen(detail), "attempt %d", st->attempts);
        if (!detail[0] && strstr(st->wifi, st->ssid)) snprintf(detail, sizeof detail, "5 GHz access points");
        else if (!detail[0]) snprintf(detail, sizeof detail, "network %s", st->ssid);
        textf(0, 22 + lines * 11, u8g2_font_5x8_tr, "%s", detail);
        u8g2_DrawHLine(&u8g2, 0, 47, 128);
        textf(0, 61, u8g2_font_6x10_tr, "SpaceMouse: %s", teensy_state_text(t->state));
    }
    u8g2_SendBuffer(&u8g2);
}

// ---------------------------------------------------------------- start

void display_start(void) {
    u8g2_Setup_ssd1306_128x64_noname_f(&u8g2, U8G2_R0, u8x8_byte_4wire_sw_spi, gpio_cb);
    u8g2_InitDisplay(&u8g2);       // (the reset pulse, then the SSD1306's settings incl. its charge pump)
    u8g2_SetPowerSave(&u8g2, 0);
    u8g2_SetFontMode(&u8g2, 1);
    started = true;
    ESP_LOGI("display", "SSD1306 started (it can't be detected on SPI: a missing display just stays dark)");
}

void display_boot_message(const char *msg) {
    if (!started) return;
    u8g2_ClearBuffer(&u8g2);
    textf(0, 12, u8g2_font_helvB10_tr, "SpaceMouse");
    textf(0, 26, u8g2_font_6x10_tr, "firmware %s", closer_demo_version());
    textf(0, 44, u8g2_font_helvB08_tr, "%s", msg);
    u8g2_SendBuffer(&u8g2);
}

static void display_task(void *arg) {
    int tick = 0;
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(100));
        teensy_snapshot_t t;
        teensy_get(&t);
        closer_demo_status_t st;
        closer_demo_get_status(&st);
        draw(&st, &t, tick++);
    }
}

void display_run(void) {
    if (started) xTaskCreate(display_task, "display", 4096, NULL, 2, NULL);
}
