<!--- In Visual Studio Code, press Crtl-Shift-V to preview. -->

# PC - Teensy - SpaceMouse full application

A script for Teensy 4.1 to communicate with SpaceMouse and USB serial devices, with support for USB hubs. Comes with an extensive GUI for the host PC to examine status, adjust settings, etc.

## Manual

1. Connect Mamri and Spacemouse through USB hub to Teensy. Keep Mamri off for now.
2. Power Teensy using powerbank. The Teensy LED should blink at 1 Hz with 40% duty cycle.
3. Power Mamri. The Teensy LED should blink at 2 Hz with 90% duty cycle.

## Onboard LED

- 1 Hz, 5%: nothing connected
- 1 Hz, 25%: only usbserial connected
- 1 Hz, 50%: only SpaceMouse connected
- 2 Hz, 90%: both SpaceMouse and usbserial connected

## Troubleshooting

### Teensy LED does not blink
Program has crashed. Happens a lot when plugging/unplugging serial. Turn off the Teensy and redo the steps in manual.

### Message "JoystickController claim collection" in serial
Find the line with this message in `.platformio\packages\framework-arduinoteensy\libraries\USBHost_t36\joystick.cpp` and comment it out.

### Python: package serial not found
In a terminal or PowerShell, run the following command:
- pip install pyserial pyqt6

## Teensy crashes when Pico is plugged in USB hub
This is a known issue. One specific hub is known to work, so try using a different hub etc.
