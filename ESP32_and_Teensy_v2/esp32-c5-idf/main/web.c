// Web server: the pages of the Arduino version (embedded, gzipped) and the SpaceMouse's values.
//   GET  /  /basic  /3d  /styles.css     the pages
//   WS   /ws          {"state","x","y","z","rx","ry","rz","buttons"} 50 times a second (the Arduino version: 20); the page
//                     sends one message ("stream") to start it
//   GET  /data        the same, once (CORS open)
//   GET  /api/state   firmware, Teensy link, Wi-Fi
#include "web.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

#include "cJSON.h"
#include "closer_demo.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "lwip/sockets.h"
#include "teensy.h"
#include "web_table.h"

static const char *TAG = "web";

// ---------------------------------------------------------------- clients (for the report)

#define MAX_CLIENTS 16
static struct { uint32_t ip; int64_t last_us; } seen[MAX_CLIENTS];

static void touch_client(httpd_req_t *req) {
    struct sockaddr_in6 addr;
    socklen_t len = sizeof addr;
    if (getpeername(httpd_req_to_sockfd(req), (struct sockaddr *)&addr, &len) != 0) return;
    uint32_t ip = addr.sin6_family == AF_INET6 ? addr.sin6_addr.un.u32_addr[3] : ((struct sockaddr_in *)&addr)->sin_addr.s_addr;
    int64_t now = esp_timer_get_time();
    int oldest = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (seen[i].ip == ip) { seen[i].last_us = now; return; }
        if (seen[i].last_us < seen[oldest].last_us) oldest = i;
    }
    seen[oldest].ip = ip;
    seen[oldest].last_us = now;
}

static int active_clients(void) {
    int64_t now = esp_timer_get_time();
    int n = 0;
    for (int i = 0; i < MAX_CLIENTS; i++) n += seen[i].ip && now - seen[i].last_us < 60 * 1000000LL;
    return n;
}

// ---------------------------------------------------------------- pages

// "/" is index.html; a path without an extension is that page (/basic -> basic.html, /3d -> 3d.html).
static esp_err_t static_get(httpd_req_t *req) {
    char path[64];
    size_t n = strcspn(req->uri, "?");
    if (n + 6 >= sizeof path) return httpd_resp_send_404(req);
    memcpy(path, req->uri, n);
    path[n] = 0;
    if (strcmp(path, "/") == 0) strcpy(path, "/index.html");
    else if (!strchr(strrchr(path, '/'), '.')) strcat(path, ".html");
    for (size_t i = 0; i < sizeof WEB_FILES / sizeof WEB_FILES[0]; i++) {
        if (strcmp(WEB_FILES[i].path, path) == 0) {
            if (strstr(path, ".html")) touch_client(req);
            httpd_resp_set_type(req, WEB_FILES[i].type);
            httpd_resp_set_hdr(req, "Content-Encoding", "gzip");
            httpd_resp_set_hdr(req, "Cache-Control", "no-cache");
            return httpd_resp_send(req, (const char *)WEB_FILES[i].start, WEB_FILES[i].end - WEB_FILES[i].start);
        }
    }
    return httpd_resp_send_404(req);
}

// ---------------------------------------------------------------- data

static int build_data(char *buf, size_t cap) {
    teensy_snapshot_t t;
    teensy_get(&t);
    return snprintf(buf, cap, "{\"state\":%d,\"x\":%d,\"y\":%d,\"z\":%d,\"rx\":%d,\"ry\":%d,\"rz\":%d,\"buttons\":%u}",
                    (int)t.state, t.xyz[0], t.xyz[1], t.xyz[2], t.rxyz[0], t.rxyz[1], t.rxyz[2], t.buttons);
}

static esp_err_t data_get(httpd_req_t *req) {
    touch_client(req);
    char buf[160];
    int n = build_data(buf, sizeof buf);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    return httpd_resp_send(req, buf, n);
}

// ---------------------------------------------------------------- live stream (WebSocket)

#define MAX_WS_CLIENTS 4
#define STREAM_PERIOD_MS 20
static httpd_handle_t server;
static int ws_fds[MAX_WS_CLIENTS] = {-1, -1, -1, -1};
static SemaphoreHandle_t ws_lock;

static void ws_add(int fd) {
    xSemaphoreTake(ws_lock, portMAX_DELAY);
    bool known = false;
    for (int i = 0; i < MAX_WS_CLIENTS; i++) known |= ws_fds[i] == fd;
    for (int i = 0; i < MAX_WS_CLIENTS && !known; i++) {
        if (ws_fds[i] < 0) { ws_fds[i] = fd; known = true; }
    }
    if (!known) httpd_sess_trigger_close(server, fd);
    xSemaphoreGive(ws_lock);
}

static esp_err_t ws_handler(httpd_req_t *req) {
    httpd_ws_frame_t frame = {0};
    uint8_t buf[32];
    frame.payload = buf;
    esp_err_t e = httpd_ws_recv_frame(req, &frame, sizeof buf);
    if (e != ESP_OK) return e;
    touch_client(req);
    ws_add(httpd_req_to_sockfd(req));
    return ESP_OK;
}

static int ws_viewers(void) {
    xSemaphoreTake(ws_lock, portMAX_DELAY);
    int n = 0;
    for (int i = 0; i < MAX_WS_CLIENTS; i++) n += ws_fds[i] >= 0;
    xSemaphoreGive(ws_lock);
    return n;
}

static void stream_task(void *arg) {
    char buf[160];
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(STREAM_PERIOD_MS));
        xSemaphoreTake(ws_lock, portMAX_DELAY);
        int fds[MAX_WS_CLIENTS], count = 0;
        for (int i = 0; i < MAX_WS_CLIENTS; i++) if (ws_fds[i] >= 0) fds[count++] = ws_fds[i];
        xSemaphoreGive(ws_lock);
        if (!count) continue;
        int n = build_data(buf, sizeof buf);
        httpd_ws_frame_t frame = {.type = HTTPD_WS_TYPE_TEXT, .payload = (uint8_t *)buf, .len = n};
        for (int k = 0; k < count; k++) {
            bool alive = httpd_ws_get_fd_info(server, fds[k]) == HTTPD_WS_CLIENT_WEBSOCKET &&
                         httpd_ws_send_frame_async(server, fds[k], &frame) == ESP_OK;
            if (!alive) {
                xSemaphoreTake(ws_lock, portMAX_DELAY);
                for (int i = 0; i < MAX_WS_CLIENTS; i++) if (ws_fds[i] == fds[k]) ws_fds[i] = -1;
                xSemaphoreGive(ws_lock);
            }
        }
    }
}

// ---------------------------------------------------------------- state

static esp_err_t state_get(httpd_req_t *req) {
    teensy_snapshot_t t;
    teensy_get(&t);
    closer_demo_status_t st;
    closer_demo_get_status(&st);
    cJSON *j = cJSON_CreateObject();
    cJSON_AddStringToObject(j, "fw", closer_demo_version());
    cJSON_AddNumberToObject(j, "state", t.state);
    cJSON_AddStringToObject(j, "state_text", teensy_state_text(t.state));
    cJSON_AddNumberToObject(j, "messages_per_s", (int)(t.messages_per_s + 0.5f));
    cJSON_AddNumberToObject(j, "updates", t.updates);
    cJSON_AddNumberToObject(j, "parse_errors", t.parse_errors);
    cJSON_AddBoolToObject(j, "wifi", st.connected);
    cJSON_AddStringToObject(j, "ip", st.ip);
    cJSON_AddNumberToObject(j, "rssi", st.rssi);
    cJSON_AddNumberToObject(j, "viewers", ws_viewers());
    char *out = cJSON_PrintUnformatted(j);
    cJSON_Delete(j);
    if (!out) return httpd_resp_send_500(req);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    esp_err_t e = httpd_resp_sendstr(req, out);
    free(out);
    return e;
}

// ---------------------------------------------------------------- start

static void stats_task(void *arg) {
    for (;;) {
        teensy_snapshot_t t;
        teensy_get(&t);
        closer_demo_set_clients(active_clients());
        closer_demo_set_stat("state", t.state);
        closer_demo_set_stat("teensy_ok", t.state != TEENSY_DISCONNECTED);
        closer_demo_set_stat("mouse_ok", t.state == TEENSY_CONNECTED);
        closer_demo_set_stat("messages_per_s", t.messages_per_s);
        closer_demo_set_stat("updates", t.updates);
        closer_demo_set_stat("parse_errors", t.parse_errors);
        closer_demo_set_stat("viewers", ws_viewers());
        if (t.state == TEENSY_DISCONNECTED) closer_demo_set_message("Teensy not answering: check the serial wires and its power");
        else if (t.state == TEENSY_NO_MOUSE) closer_demo_set_message("No SpaceMouse on the Teensy's USB port");
        else closer_demo_set_message("");
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}

void web_start(void) {
    httpd_config_t cfg = HTTPD_DEFAULT_CONFIG();
    cfg.lru_purge_enable = true;
    cfg.uri_match_fn = httpd_uri_match_wildcard;
    cfg.max_uri_handlers = 8;
    cfg.stack_size = 6144;
    cfg.send_wait_timeout = 2;
    ws_lock = xSemaphoreCreateMutex();
    esp_err_t started = httpd_start(&server, &cfg);
    if (started != ESP_OK) {
        ESP_LOGE(TAG, "web server did not start: %s", esp_err_to_name(started));
        return;
    }
    const httpd_uri_t uris[] = {
        {.uri = "/data", .method = HTTP_GET, .handler = data_get},
        {.uri = "/ws", .method = HTTP_GET, .handler = ws_handler, .is_websocket = true},
        {.uri = "/api/state", .method = HTTP_GET, .handler = state_get},
        {.uri = "/*", .method = HTTP_GET, .handler = static_get},
    };
    for (size_t i = 0; i < sizeof uris / sizeof uris[0]; i++) {
        if (httpd_register_uri_handler(server, &uris[i]) != ESP_OK) ESP_LOGE(TAG, "could not register %s", uris[i].uri);
    }
    xTaskCreate(stats_task, "stats", 4096, NULL, 2, NULL);
    xTaskCreate(stream_task, "stream", 4096, NULL, 4, NULL);
    ESP_LOGI(TAG, "web server up");
}
