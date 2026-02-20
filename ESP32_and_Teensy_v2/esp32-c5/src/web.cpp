#include "web.h"
#include "index.html.h"
#include "basic.html.h"
#include "pose3d.html.h"
#include "styles.css.h"
#include "teensy.h"
#include <ArduinoJson.h>

RoamingWiFiManager manager;
AsyncWebSocket ws("/ws");

unsigned long lastBroadcastTime = 0;
const unsigned long BROADCAST_INTERVAL = 50; // 20 Hz = 50ms

void onWebSocketEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type,
                      void *arg, uint8_t *data, size_t len) {
    if (type == WS_EVT_CONNECT) {
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    } else if (type == WS_EVT_DISCONNECT) {
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
    }
}

void broadcastSpaceMouseData() {
    if (ws.count() == 0) return; // No clients connected
    
    unsigned long now = millis();
    if (now - lastBroadcastTime < BROADCAST_INTERVAL) return;
    lastBroadcastTime = now;
    
    JsonDocument doc;
    doc["state"] = (int)getTeensyState();
    doc["x"] = spaceMouseData.xyz[0];
    doc["y"] = spaceMouseData.xyz[1];
    doc["z"] = spaceMouseData.xyz[2];
    doc["rx"] = spaceMouseData.rxyz[0];
    doc["ry"] = spaceMouseData.rxyz[1];
    doc["rz"] = spaceMouseData.rxyz[2];
    doc["buttons"] = spaceMouseData.buttons;
    
    String jsonString;
    serializeJson(doc, jsonString);
    ws.textAll(jsonString);
}

String htmlWithAuth(const char* htmlTemplate) {
    String html = htmlTemplate;
    if (adminCredentials.first == "" || adminCredentials.second == "") {
        html.replace("$AUTH_INFO$", "");
    } else {
        html.replace("$AUTH_INFO$", "(user: " + adminCredentials.first + ", pass: " + (showPasswordOnRootPage ? adminCredentials.second : "****") + ")");
    }
    return html;
}

void handleRoot(AsyncWebServerRequest *request) {
    String html = htmlWithAuth(index_html);
    request->send(200, "text/html", html);
}

void handleBasic(AsyncWebServerRequest *request) {
    String html = htmlWithAuth(basic_html);
    request->send(200, "text/html", html);
}

void handle3d(AsyncWebServerRequest *request) {
    String html = htmlWithAuth(pose3d_html);
    request->send(200, "text/html", html);
}

void handleStyles(AsyncWebServerRequest *request) {
    request->send(200, "text/css", styles_css);
}

void setupWeb() {
    Serial.begin(115200);
    
    // Setup WebSocket
    ws.onEvent(onWebSocketEvent);
    
    manager.init(knownNetworks, adminCredentials, aliasUrl);
    // The manager already set up the ESP32AsyncWebServer instance at manager.server, but we can add our own routes to it.
    manager.server.addHandler(&ws);
    manager.server.on("/", [] (AsyncWebServerRequest *request) {
        handleRoot(request);
    });
    manager.server.on("/basic", HTTP_GET, [] (AsyncWebServerRequest *request) {
        handleBasic(request);
    });
    manager.server.on("/3d", HTTP_GET, [] (AsyncWebServerRequest *request) {
        handle3d(request);
    });
    manager.server.on("/styles.css", HTTP_GET, [] (AsyncWebServerRequest *request) {
        handleStyles(request);
    });

    Serial.println("Roaming Wifi tester: Setup completed.");
}

void loopWeb() {
    manager.loop(); // must be periodically called to handle wifi management tasks
    ws.cleanupClients(); // Clean up disconnected clients
    broadcastSpaceMouseData(); // Send spacemouse data to connected clients
}
