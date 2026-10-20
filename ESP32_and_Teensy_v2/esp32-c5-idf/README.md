# SpaceMouse v2: ESP32-C5 firmware (ESP-IDF)

ESP-IDF version of the Arduino/PlatformIO firmware in `../esp32-c5`. Same pins, the same serial protocol with the Teensy
(`../teensy` is unchanged), the same pages. What changed:

* **Wi-Fi without RoamingWiFiManager.** The network, password and the lab demos key are stored in flash (NVS) by a one-time
  settings image (`tools/provision.py`), not in the source. The shared `components/closer_demo` joins the network (5 GHz only),
  keeps the time, reconnects, and moves to a clearly stronger access point of the same network. The `/wifi` pages are gone.
* **Lab demos page.** The board reports to https://closer.ram.eemcs.utwente.nl/labdemos/ about once a minute: address, signal,
  firmware, the Teensy link (no Teensy / no SpaceMouse / connected, messages per second), viewers. The page shows its status,
  history and set-up instructions, and serves its pages over HTTPS (`/labdemos/device/spacemouse/`).
* **Over-the-air updates.** A new release on the server is installed at the next report; one that cannot report within two
  minutes is rolled back. The pages are embedded (gzipped) in the firmware image.
* **Display.** u8g2 with proportional fonts (as the HandTool): the address and signal, the translation and rotation values and
  the buttons, what Wi-Fi is doing while it connects, and what is wrong with the Teensy link.
* **Status LED** (WS2812 on GPIO 27, red and green swapped on the Waveshare board): orange connecting, green connected, blinking
  blue while updating, blinking red after repeated Wi-Fi failures.
* **Stream.** `/ws` sends `{"state","x","y","z","rx","ry","rz","buttons"}` 50 times a second (was 20); the page sends one
  message to start it. `GET /data` gives the same once, `GET /api/state` the firmware and link state.

Pins (menuconfig > SpaceMouse): Teensy UART TX=2, RX=3 at 921600 baud; OLED D0/CLK=0, D1/MOSI=1, RES=6, DC=7, CS=8.

## Build

    . $IDF_PATH/export.sh            # ESP-IDF 5.5
    idf.py build

## First flash (USB)

On the lab demos server, `python -m labdemos.cli add spacemouse` gives the device key (`local/device.key`). Put the Wi-Fi settings
in `local/provision.json` (`{"ssid": "iotroam", "password": "..."}`), then

    python tools/provision.py        # local/spacemouse-factory.bin: bootloader, partitions, firmware, settings
    python -m esptool --chip esp32c5 -p <port> write-flash 0x0 spacemouse-factory.bin

`local/` holds the Wi-Fi password and the key and is not in git. Over-the-air updates never touch the settings.

## Updates over the air

Raise `VERSION`, `idf.py build`, and on the lab demos server:

    python -m labdemos.cli publish spacemouse <path>/build/spacemouse.bin

## Files

    main/main.c       start-up order
    main/teensy.c     the serial link: ping/pong, JSON updates, connection state
    main/display.c    the OLED (u8g2, bit-banged SPI)
    main/led.c        the status LED
    main/web.c        pages, data, WebSocket stream, reports
    web/              index.html, basic.html, 3d.html, styles.css (embedded at build time)
    components/       closer_demo (Wi-Fi, reports, updates; shared by the CloSer lab demos), u8g2 (display library, only the
                      fonts used)
    tools/            provision.py (first-flash image), pack_web.py (embeds web/)
