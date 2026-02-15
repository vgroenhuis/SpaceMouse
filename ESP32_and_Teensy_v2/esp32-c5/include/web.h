#include <Arduino.h>
#include "RoamingWiFiManager.h"
#include <AsyncWebSocket.h>

#if __has_include("../../../../VincentLibraryTester/include/VincentCredentials.h")
    #include "../../../../VincentLibraryTester/include/VincentCredentials.h"
#else
    #error "Wrong credentials path!"
    #define WIFI_CREDENTIALS {{"ssid","pass"},{"iotroam","pass2"}}
    #define ADMIN_CREDENTIALS {"",""}
    #define ALIAS_URL ""
#endif

#ifndef WIFI_CREDENTIALS
#define WIFI_CREDENTIALS {{"your-ssid","your-password"},{"your-ssid2","your-password2"}}
#endif

#ifndef ADMIN_CREDENTIALS
#define ADMIN_CREDENTIALS {"",""}
#endif

#ifndef ALIAS_URL
#define ALIAS_URL ""
#endif

const std::vector<NetworkCredentials> knownNetworks = WIFI_CREDENTIALS;
const std::pair<String, String> adminCredentials = ADMIN_CREDENTIALS;
const String aliasUrl = ALIAS_URL;
const bool showPasswordOnRootPage = true;

extern RoamingWiFiManager manager;
extern AsyncWebSocket ws;


// serve simple HTML page with link to wifi manager and its credentials
void handleRoot(AsyncWebServerRequest *request);

void setupWeb();

void loopWeb();