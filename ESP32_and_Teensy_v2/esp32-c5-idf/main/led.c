#include "led.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

#include "closer_demo.h"
#include "esp_log.h"
#include "led_strip.h"

static led_strip_handle_t strip;

static void set(uint8_t r, uint8_t g, uint8_t b) {
#if CONFIG_SPACEMOUSE_LED_RED_GREEN_REVERSED
    led_strip_set_pixel(strip, 0, g, r, b);
#else
    led_strip_set_pixel(strip, 0, r, g, b);
#endif
    led_strip_refresh(strip);
}

static void led_task(void *arg) {
    for (int tick = 0;; tick++) {
        closer_demo_status_t st;
        closer_demo_get_status(&st);
        if (st.updating) set(0, 0, (tick % 4 < 2) ? 40 : 4);                 // blue, blinking
        else if (st.connected) set(0, 12, 0);                                  // green
        else if (st.attempts >= 3) set((tick % 4 < 2) ? 40 : 0, 0, 0);        // red, blinking
        else set((tick % 4 < 2) ? 30 : 8, (tick % 4 < 2) ? 10 : 3, 0);        // orange, pulsing
        vTaskDelay(pdMS_TO_TICKS(250));
    }
}

void led_start(void) {
#if CONFIG_SPACEMOUSE_LED_GPIO >= 0
    led_strip_config_t sc = {.strip_gpio_num = CONFIG_SPACEMOUSE_LED_GPIO, .max_leds = 1, .led_model = LED_MODEL_WS2812,
                             .color_component_format = LED_STRIP_COLOR_COMPONENT_FMT_GRB};
    led_strip_rmt_config_t rc = {.clk_src = RMT_CLK_SRC_DEFAULT, .resolution_hz = 10 * 1000 * 1000};
    if (led_strip_new_rmt_device(&sc, &rc, &strip) != ESP_OK) {
        ESP_LOGW("led", "no status LED");
        return;
    }
    xTaskCreate(led_task, "led", 2560, NULL, 2, NULL);
#endif
}
