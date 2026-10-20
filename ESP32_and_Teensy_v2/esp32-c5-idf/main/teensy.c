#include "teensy.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <string.h>

#include "cJSON.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_timer.h"

static const char *TAG = "teensy";

#define PORT UART_NUM_1
#define PING_MS 100
#define PONG_TIMEOUT_MS 200

static SemaphoreHandle_t lock;
static teensy_snapshot_t snap;
static int64_t last_pong_us;

const char *teensy_state_text(teensy_state_t s) {
    switch (s) {
        case TEENSY_CONNECTED: return "connected";
        case TEENSY_NO_MOUSE: return "no SpaceMouse";
        default: return "no Teensy";
    }
}

static void read_triple(cJSON *a, int16_t out[3]) {
    if (!cJSON_IsArray(a) || cJSON_GetArraySize(a) != 3) return;
    for (int i = 0; i < 3; i++) {
        cJSON *v = cJSON_GetArrayItem(a, i);
        if (cJSON_IsNumber(v)) out[i] = (int16_t)v->valuedouble;
    }
}

static void handle_line(const char *line, uint32_t *count) {
    cJSON *j = cJSON_Parse(line);
    const cJSON *type = cJSON_GetObjectItem(j, "type");
    if (!cJSON_IsString(type)) {
        xSemaphoreTake(lock, portMAX_DELAY);
        snap.parse_errors++;
        xSemaphoreGive(lock);
        cJSON_Delete(j);
        return;
    }
    (*count)++;
    xSemaphoreTake(lock, portMAX_DELAY);
    if (strcmp(type->valuestring, "pong") == 0) {
        last_pong_us = esp_timer_get_time();
        snap.state = cJSON_IsTrue(cJSON_GetObjectItem(j, "spacemouse_connected")) ? TEENSY_CONNECTED : TEENSY_NO_MOUSE;
    } else if (strcmp(type->valuestring, "update") == 0) {
        read_triple(cJSON_GetObjectItem(j, "sm_xyz"), snap.xyz);
        read_triple(cJSON_GetObjectItem(j, "sm_rxyz"), snap.rxyz);
        cJSON *b = cJSON_GetObjectItem(j, "sm_buttons");
        if (cJSON_IsNumber(b)) snap.buttons = (uint16_t)b->valuedouble;
        snap.updates++;
    }
    xSemaphoreGive(lock);
    cJSON_Delete(j);
}

static void teensy_task(void *arg) {
    static char line[512];
    size_t len = 0;
    uint8_t buf[256];
    int64_t next_ping = 0, rate_t0 = esp_timer_get_time();
    uint32_t count = 0;
    for (;;) {
        int n = uart_read_bytes(PORT, buf, sizeof buf, pdMS_TO_TICKS(10));
        for (int i = 0; i < n; i++) {
            char c = (char)buf[i];
            if (c == '\n') {
                line[len] = 0;
                if (len) handle_line(line, &count);
                len = 0;
            } else if (c != '\r') {
                if (len < sizeof line - 1) line[len++] = c;
                else len = 0;                         // garbage: drop the line
            }
        }
        int64_t now = esp_timer_get_time();
        if (now >= next_ping) {
            static const char ping[] = "{\"type\":\"ping\"}\n";
            uart_write_bytes(PORT, ping, sizeof ping - 1);
            next_ping = now + PING_MS * 1000;
        }
        xSemaphoreTake(lock, portMAX_DELAY);
        if (now - last_pong_us > PONG_TIMEOUT_MS * 1000 && snap.state != TEENSY_DISCONNECTED) {
            snap.state = TEENSY_DISCONNECTED;
            ESP_LOGW(TAG, "Teensy not answering");
        }
        if (now - rate_t0 >= 1000000) {
            snap.messages_per_s = count * 1e6f / (float)(now - rate_t0);
            count = 0;
            rate_t0 = now;
        }
        xSemaphoreGive(lock);
    }
}

void teensy_get(teensy_snapshot_t *out) {
    xSemaphoreTake(lock, portMAX_DELAY);
    *out = snap;
    xSemaphoreGive(lock);
}

void teensy_start(void) {
    lock = xSemaphoreCreateMutex();
    uart_config_t cfg = {
        .baud_rate = CONFIG_SPACEMOUSE_TEENSY_BAUD,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };
    esp_err_t e = uart_driver_install(PORT, 2048, 1024, 0, NULL, 0);
    if (e == ESP_OK) e = uart_param_config(PORT, &cfg);
    if (e == ESP_OK) e = uart_set_pin(PORT, CONFIG_SPACEMOUSE_TEENSY_TX_GPIO, CONFIG_SPACEMOUSE_TEENSY_RX_GPIO,
                                      UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
    if (e != ESP_OK) {
        ESP_LOGE(TAG, "UART: %s", esp_err_to_name(e));   // carry on: the page and the reports say "no Teensy"
        return;
    }
    xTaskCreate(teensy_task, "teensy", 4096, NULL, 6, NULL);
}
