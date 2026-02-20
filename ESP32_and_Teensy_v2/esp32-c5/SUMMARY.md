<!-- Visual Studio Code: press Ctrl-Shift-V to view preview -->

# ESP32-C5 SpaceMouse Bridge — Project Summary

## What this project does
This firmware turns an **ESP32-C5** into a bridge and status UI for a SpaceMouse setup:
- It communicates with a **Teensy** over high-speed UART.
- It receives SpaceMouse state from the Teensy as line-delimited JSON.
- It hosts a web dashboard (HTTP + WebSocket) for live monitoring.
- It shows compact status information on a 0.96" OLED over SPI.
- It uses a roaming Wi-Fi manager to connect to known networks and expose Wi-Fi management pages.

---

## High-level architecture
The codebase is split into three runtime modules plus the app entry point:

- **`main.cpp`**
  - Initializes display, Teensy link, and web stack in `setup()`.
  - Runs `loopWeb()`, `loopTeensy()`, and OLED refresh (~20 Hz) in `loop()`.

- **`teensy.cpp` / `teensy.h`**
  - Manages UART1 communication with Teensy (`921600`, pins RX=3, TX=2).
  - Implements ping/pong keepalive and connection state machine.
  - Parses incoming JSON messages (`pong`, `update`) with ArduinoJson.
  - Stores latest shared SpaceMouse data (`spaceMouseData`) and exposes status text.

- **`web.cpp` / `web.h`**
  - Starts Wi-Fi management via `RoamingWiFiManager`.
  - Serves a minimal root page (`/`) and multiple visualization pages (`/basic`, `/3d`).
  - Serves shared stylesheet at `/styles.css` for all pages.
  - Exposes WebSocket endpoint `/ws` used by visualization pages.
  - Broadcasts live SpaceMouse telemetry to all WS clients every 50 ms (20 Hz).

- **`display.cpp` / `display.h`**
  - Drives a 128x64 SSD1306 OLED over FSPI.
  - Shows Wi-Fi IP (or failure) and Teensy/SpaceMouse status values.

---

## Runtime data flow
1. Teensy sends newline-delimited JSON packets to ESP32 UART1.
2. ESP32 parses packets and updates `spaceMouseData` + `teensyState`.
3. Web module periodically serializes current state to JSON and pushes to all WebSocket clients.
4. Browser UI updates status, translation/rotation values, sliders, and button indicators.
5. OLED shows concise local status (IP + compact motion/button summary).

---

## Teensy protocol details
### Outbound from ESP32
- `{"type":"ping"}` every **100 ms**.

### Inbound to ESP32
- `{"type":"pong","spacemouse_connected":<bool>}`
  - updates connection state: `CONNECTED` or `NO_MOUSE`
- `{"type":"update", ...}` containing:
  - `sm_xyz` (3 ints)
  - `sm_rxyz` (3 ints)
  - `sm_buttons` (`uint16_t` bitfield)

### Connection timeout behavior
- If no pong within **200 ms**, state falls back to disconnected.

---

## Web pages and routing
- `/` (from `include/index.html.h`) is a minimal landing page with links.
- `/basic` (from `include/basic.html.h`) hosts the original live dashboard.
- `/3d` (from `include/pose3d.html.h`) hosts a simple real-time 3D pose visualization.
- `/styles.css` (from `include/styles.css.h`) provides common styles used by all pages.

### Basic dashboard behavior
The embedded page in `include/basic.html.h`:
- Connects to `ws://<device-ip>/ws`
- Reconnects automatically when disconnected (every 2 seconds)
- Displays:
  - connection state (`Disconnected`, `No Mouse`, `Connected`)
  - translation (`x`, `y`, `z`)
  - rotation (`rx`, `ry`, `rz`)
  - left/right button state from bitmask
- Includes quick link to `/wifi` for Wi-Fi manager access.

### 3D pose page behavior
The embedded page in `include/pose3d.html.h`:
- Connects to `ws://<device-ip>/ws`
- Reconnects automatically when disconnected (every 2 seconds)
- Renders a simple wireframe cube and axes on a `<canvas>`
- Uses `x/y/z` as translation and `rx/ry/rz` as rotation for the pose visualization
- Uses SpaceMouse axis convention in the view (`+x` right, `+y` down, `+z` right-hand rule)

---

## Hardware mapping and interfaces
### OLED (SSD1306, SPI)
Defined in `include/display.h`:
- CLK: GPIO 0
- MOSI: GPIO 1
- RESET: GPIO 6
- DC: GPIO 7
- CS: GPIO 8
- Resolution: 128x64

### Teensy UART
Defined in `src/teensy.cpp`:
- ESP32 RX: GPIO 3
- ESP32 TX: GPIO 2
- Baud: 921600

---

## Build and platform configuration
From `platformio.ini`:
- Environment: `esp32c5-display`
- Platform: `platform-espressif32` from pioarduino GitHub
- Framework: Arduino
- Board: `esp32-c5-devkitc1-n16r4`
- Filesystem: LittleFS
- Partition table: `partitions-16M.csv`
- Upload speed: 921600
- Monitor speed: 115200

### Libraries
- Adafruit SSD1306
- Adafruit GFX Library
- ESPAsyncWebServer
- roaming-wifi-manager
- ArduinoJson

### Build flags
- `RED_GREEN_REVERSED`
- `WIFI_DEBUG_LEVEL=3`

---

## Partitions (16 MB layout)
Defined in `partitions-16M.csv`:
- `nvs`: 24K
- `phy_init`: 4K
- `factory` app: 2M
- `coredump`: 64K
- `littlefs`: 100K

---

## Credentials and environment notes
`include/web.h` expects Wi-Fi/admin credentials through a header include path that is local to the developer environment:
- `../../../../VincentLibraryTester/include/VincentCredentials.h`

If this file is missing, fallback macros can be used.

---

## Current workspace structure
- `src/`
  - `main.cpp`
  - `display.cpp`
  - `teensy.cpp`
  - `web.cpp`
- `include/`
  - `basic.html.h`
  - `display.h`
  - `pose3d.html.h`
  - `styles.css.h`
  - `teensy.h`
  - `web.h`
  - `index.html.h`
- `platformio.ini`
- `partitions-16M.csv`
- `data/` (currently empty)

---

## In short
This project is a clean three-part ESP32 firmware:
- **Input:** Teensy JSON stream for SpaceMouse telemetry
- **Processing:** connection/state tracking + shared data model
- **Output:** real-time web dashboard and local OLED status view

It is primarily a **diagnostic/monitoring bridge** between a Teensy-based SpaceMouse interface and network clients.
