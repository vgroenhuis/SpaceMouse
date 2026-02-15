<!--- In Visual Studio Code, press Crtl-Shift-V to preview. -->

# SpaceMouse application version 2

Converts SpaceMouse to a web-enabled device which can connect to other devices wirelessly.

A Teensy 4.1 acts as USB host to receive raw axis data from the SpaceMouse. The Teensy communicates the raw axis data with an ESP32-C5 over a hardware serial with a JSON-based protocol.

The ESP32-C5 has a web server and a small display. The display shows the IP address and the hostname (if any). The LED of the ESP32-C5 shows the wifi connectivity status.

# Power

Powerbank connects to ESP32-C5 through any USB-C port.
Both USB-C ports have CC1 and CC2 connected to GND through 5.1K resistors, configuring USB-C to deliver 5V.
Powerbank applies 5V at VBUS.
Diode D2 or D3 forwards 5V minus one diode drop (around 0.5V) to VCC_5V, so around 4.5V.
4.5V is provided through ESP32's 5V pin, to Teensy 4.1.

# Rails

Both left and right rails on breadboard provide 3.3V, to avoid accidental mix-up of 5V and 3.3V power.

# Pin assignment

## Teensy 4.1
USB host: female USB-A port for SpaceMouse. (No USB hub is used.)
0: Serial1 RX (115200 baud 8N1, follower)
1: Serial1 TX (115200 baud 8N1, follower)

GND: GND rail (left+right)
Vin: 5V rail (left)

## ESP32-C5

GND: GND rail (left+right)
5V: 5V rail (left)
2: Serial TX (115200 baud 8N1, leader) connected to Teensy's Serial Rx
3: Serial RX (115200 baud 8N1, leader) connected to Teensy's Serial Tx
0: Display D0 = SPI CLK
1: Display D1 = SPI MOSI
6: Display RES
7: Display DC = SPI Data/Command
8: Display CS

## 0.96" OLED

This display accepts 3.3V and 5V supply, but signals are 3.3V only.
Product site: [0.96" OLED display](https://www.tinytronics.nl/nl/displays/oled/0.96-inch-oled-display-128*64-pixels-wit)

GND: GND rail (left)
VCC: 5V rail (left)
D0, D1, RES, DC, CS: ESP32-C5 0, 1, 6, 7, 8

## Vincent's device

ESP32-C5 MAC address: 3C:DC:75:84:A9:30