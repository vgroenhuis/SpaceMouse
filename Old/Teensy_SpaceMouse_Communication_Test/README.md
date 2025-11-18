# Teensy - SpaceMouse communication test

Vincent Groenhuis, November 2025

## Summary

Test script to detect certain USB devices connected to Teensy 4.1, including SpaceMouse and USB hub, and log some data. A minimum working script to reproduce crashes with the Pico connected through certain USB hubs to the Teensy is also available in Crash.hpp

## Manual

- In main.cpp, comment one line depending on which subproject to run: Crash.hpp or Teensy_SpaceMouse_test.hpp
- Program Teensy 4.1 with this script using PlatformIO.
- Open a serial monitor on the host PC.
- Connect a USB hub and/or SpaceMouse to the female USB A plug on the Teensy 4.1.
- Connect another microcontroller to Teensy 4.1 with USB cable.
- Whenever a client is connected/disconnect, an appropriate message should appear in the console.
- Whenever the Teensy crashes (due to faulty usbserial driver), the watchdog will automatically reset the Teensy.

## Onboard LED status for Teensy_SpaceMouse_test:

- Duty 10%: No USB hub or SpaceMouse connected
- Duty 50%: USB hub connected, but no SpaceMouse
- Duty 90%: SpaceMouse connected
