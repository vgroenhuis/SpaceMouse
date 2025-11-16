# Teensy - SpaceMouse communication test

Vincent Groenhuis, November 2025

## Summary

Test script to detect certain USB devices connected to Teensy 4.1, including SpaceMouse and USB hub, and log some data.

## Manual

- Program Teensy 4.1 with this script using PlatformIO
- Open a serial monitor
- Connect a USB hub and/or SpaceMouse to the female USB A plug on the Teensy 4.1
- Connect another microcontroller to Teensy 4.1 with USB cable
- Whenever a client is connected/disconnect, an appropriate message should appear in the console

## Onboard LED status

- Duty 10%: No USB hub or SpaceMouse connected
- Duty 50%: USB hub connected, but no SpaceMouse
- Duty 90%: SpaceMouse connected
