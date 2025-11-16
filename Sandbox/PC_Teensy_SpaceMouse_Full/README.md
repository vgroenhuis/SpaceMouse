# PC - Teensy - SpaceMouse full application

A script for Teensy 4.1 to communicate with SpaceMouse and USB serial devices, with support for USB hubs. Comes with an extensive GUI for the host PC to examine status, adjust settings, etc.

## Manual

TODO

## Onboard LED

10 Hz, 50%: waiting for host serial to start (debugging only)

## Troubleshooting
### Message "JoystickController claim collection" in serial
Find the line with this message in `.platformio\packages\framework-arduinoteensy\libraries\USBHost_t36\joystick.cpp` and comment it out.

### Python: package serial not found
In a terminal or PowerShell, run the following command:
- pip install pyserial pyqt6