/*
    PC to Teensy 4.1 communication test script
*/

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include "version.h"

LittleFS_Program myfs;

// ------------ Persistent Config Structure --------------------

struct Config {
  float max_speed = 1.0;
  bool invert_joystick = false;
  int mode = 0;
};

// Global config
Config config;

// File path on LittleFS
const char *CONFIG_FILE = "/config.json";

bool initInfoSent = false;

// ------------ Save / Load Config -----------------------------


void loadConfig() {
  if (!myfs.exists(CONFIG_FILE)) return;

  File f = myfs.open(CONFIG_FILE, FILE_READ);
  if (!f) return;

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  if (!err) {
    config.max_speed = doc["max_speed"] | config.max_speed;
    config.invert_joystick = doc["invert_joystick"] | config.invert_joystick;
    config.mode = doc["mode"] | config.mode;
    Serial.println("[INFO] Config loaded successfully");
  } else {
    Serial.println("[ERROR] Failed to parse config file");
  }
  f.close();
}

bool saveConfig() {
    // Store current config
    // Config temp = config;
    
    File f = myfs.open(CONFIG_FILE, FILE_WRITE_BEGIN);
    if (!f) {
        Serial.println("[ERROR] Failed to open config file for writing");
        return false;
    }
    f.truncate();

    JsonDocument doc;
    doc["max_speed"] = config.max_speed;
    doc["invert_joystick"] = config.invert_joystick;
    doc["mode"] = config.mode;

    serializeJson(doc, f);
    f.close();
    Serial.println("[INFO] Config saved successfully");
    return true;
}

// ------------ Command Handling -------------------------------

String inputBuffer;

void sendJson(JsonDocument &doc) {
  String out;
  serializeJson(doc, out);
  Serial.println(out);   // newline-delimited JSON
}

void printRootDirectory() {
    File root = myfs.open("/");
    if (!root || !root.isDirectory()) {
        Serial.println("[ERROR] Failed to open root directory");
    } else {
        Serial.println("[INFO] Listing LittleFS root:");
        File f = root.openNextFile();
        if (!f) {
        Serial.println(" (empty)");
        }
        while (f) {
        Serial.print(" - ");
        Serial.print(f.name());
        if (f.isDirectory()) {
            Serial.println("/ <DIR>");
        } else {
            Serial.print(" (");
            Serial.print(f.size());
            Serial.println(" bytes)");
        }
        f = root.openNextFile();
        }
        root.close();
    }    
}

void printConfigFile() {
    if (!myfs.exists(CONFIG_FILE)) {
        Serial.println("[INFO] Config file does not exist");
        return;
    }

    File f = myfs.open(CONFIG_FILE, FILE_READ);
    if (!f) {
        Serial.println("[ERROR] Failed to open config file");
        return;
    }

    Serial.println("[INFO] Config file contents:");
    while (f.available()) {
        Serial.write(f.read());
    }
    Serial.println();
    f.close();
}

void handleCommand(JsonDocument &cmd) {
  JsonDocument reply;

  const char *c = cmd["cmd"];
  if (!c) {
    reply["result"] = "err";
    reply["msg"] = "missing cmd";
    sendJson(reply);
    return;
  }

  // ------------------ GET ---------------------------------
  if (strcmp(c, "get") == 0) {
    const char* p = cmd["param"];
    reply["result"] = "ok";
    reply["param"] = p;

    if (strcmp(p, "max_speed") == 0) reply["value"] = config.max_speed;
    else if (strcmp(p, "invert_joystick") == 0) reply["value"] = config.invert_joystick;
    else if (strcmp(p, "mode") == 0) reply["value"] = config.mode;
    else {
      reply["result"] = "err";
      reply["msg"] = "unknown param";
    }

    sendJson(reply);
    return;
  }

  // ------------------ SET ---------------------------------
  if (strcmp(c, "set") == 0) {
    const char* p = cmd["param"];
    JsonVariant v = cmd["value"];

    if (strcmp(p, "max_speed") == 0) config.max_speed = v.as<float>();
    else if (strcmp(p, "invert_joystick") == 0) config.invert_joystick = v.as<bool>();
    else if (strcmp(p, "mode") == 0) config.mode = v.as<int>();
    else {
      reply["result"] = "err";
      reply["msg"] = "unknown param";
      sendJson(reply);
      return;
    }

    reply["result"] = "ok";
    reply["param"] = p;
    sendJson(reply);
    return;
  }

  // ------------------ SAVE --------------------------------
  if (strcmp(c, "save") == 0) {
    if (saveConfig()) {
      reply["result"] = "ok";
    } else {
      reply["result"] = "err";
      reply["msg"] = "failed to open config file";
    }
    sendJson(reply);
    return;
  }

  // ------------------ LOAD --------------------------------
  if (strcmp(c, "load") == 0) {
    loadConfig();
    reply["result"] = "ok";
    reply["max_speed"] = config.max_speed;
    reply["invert_joystick"] = config.invert_joystick;
    reply["mode"] = config.mode;
    sendJson(reply);
    return;
  }

  // ------------------ LIST --------------------------------
  if (strcmp(c, "list") == 0) {
    printRootDirectory();
    reply["result"] = "ok";
    sendJson(reply);
    return;
  }

  // ------------------ PRINT -------------------------------
  if (strcmp(c, "print") == 0) {
    printConfigFile();
    reply["result"] = "ok";
    sendJson(reply);
    return;
  }

  // ------------------ STATUS -------------------------------
  if (strcmp(c, "status") == 0) {
    reply["result"] = "ok";
    reply["max_speed"] = config.max_speed;
    reply["invert_joystick"] = config.invert_joystick;
    reply["mode"] = config.mode;

    sendJson(reply);
    return;
  }

  // ------------------ Unknown Command ----------------------
  reply["result"] = "err";
  reply["msg"] = "unknown command";
  sendJson(reply);
}

void processSerial() {
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n') {
      JsonDocument doc;
      DeserializationError err = deserializeJson(doc, inputBuffer);
      if (!err) handleCommand(doc);
      inputBuffer = "";
    } else {
      inputBuffer += c;
    }
  }
}


void blinkLed(float freq, float duty) {
    static unsigned long lastBlink = 0;
    static bool ledState = false;
    unsigned long now = millis();
    unsigned long onDuration = (unsigned long)(1000.0f * duty / freq);
    unsigned long offDuration = (unsigned long)(1000.0f * (1.0f - duty) / freq);
    
    if (ledState && (now - lastBlink >= onDuration)) {  // LED on for onDuration
        digitalWrite(LED_BUILTIN, LOW);
        ledState = false;
        lastBlink = now;
    } else if (!ledState && (now - lastBlink >= offDuration)) {  // LED off for offDuration
        digitalWrite(LED_BUILTIN, HIGH);
        ledState = true;
        lastBlink = now;
    }
}

void setup() {
    pinMode(LED_BUILTIN, OUTPUT);
    Serial.begin(115200);

    /*
    while (!Serial) {
        blinkLed(1.0f, 0.5f);  // Blink LED at 1Hz while waiting for Serial
    }*/

    if (!myfs.begin(1024 * 1024 * 4)) {
        Serial.printf("[ERROR] Error starting %s\n", "Program flash DISK");
        while(1) {
            blinkLed(1.0f, 0.9f);  // Blink LED at 1Hz with 90% duty cycle
        }
    } else {
        Serial.println("[INFO] LittleFS initialized successfully");
        printRootDirectory();
    }

    loadConfig();
}

void sendInitInfo() {
    // Send boot info
    Serial.println("[INFO] Welcome to space!");
    JsonDocument doc;
    doc["proto_ver"] = 1;
    doc["fw_ver"] = FW_VERSION_STRING;
    doc["device"] = "teensy41";
    sendJson(doc);
}

void loop() {
    if (Serial) {
        if (!initInfoSent) {
            sendInitInfo();
            initInfoSent = true;
        }
    } else {
        initInfoSent = false;
    }
    processSerial();
    blinkLed(2.0f, 0.25f);  // Blink LED at 2Hz with 25% duty cycle
}
