# Communicating with Teensy

Vincent Groenhuis, November 2025

## Summary

A test script to study a robust protocol for communication over serial with the Teensy 4.1, using a Python script on the host machine.

## Introduction

A C++ script for Teensy 4.1 plus a Python app for windows.
The apps communicate to each other over serial.
The protocol uses JSON messages, intertwined with human-readable messages.

## Manual

- Program Teensy 4.1 using PlatformIO
- Run gui_app.py
- In the gui_app, choose the COM port used by the Teensy 4.1. Some messages should appear in the GUI window.
- Click "Get Status" to get some dummy status information
- Click "Load Settings" to retrieve some settings.
- Change settings (max speed, invert joystick, mode etc), click "Set Values" and then "Save Settings". Unplug Teensy 4.1 (shutting it of), plug Teensy back in, connect again and confirm that changed settings are correctly saved.

## Troubleshooting

### Python executable cannot be found
Ctrl-shift-P and choose Python environment, optionally create virtual environment (venv)

### No module named 'serial'
Run following commands in terminal / Powershell:
- pip install pyserial
- pip install pyqt6
