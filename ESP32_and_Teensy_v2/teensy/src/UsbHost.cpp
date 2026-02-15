/*
    Manages communication with various USB devices through the USB Host.
    Including: SpaceMouse, USB hub, USB serial, etc.
*/

#include "UsbHost.h"
#include <USBHost_t36.h>
#include "TeensyUtils.h"
#include "../../common/include/SpaceMouseData.h"

// Define global storage for robot joints
SpaceMouseData spaceMouseData;
bool axisOrButtonChanged = true;
//int g_robot_joints[8] = {0,0,0,0,0,0,0,0};

USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);
USBHIDParser hid1(myusb);
USBHIDParser hid2(myusb);
USBSerial userial(myusb);
JoystickController joystick(myusb);

USBDriver *drivers[] = {&hub1, &hub2, &userial, &joystick, &hid1, &hid2};
#define CNT_DEVICES (sizeof(drivers) / sizeof(drivers[0]))
const char *driver_names[CNT_DEVICES] = {"Dev_hub", "Dev_hub2", "Dev_USB_Serial", "Dev_joystick", "Dev_HID", "Dev_HID2"};
bool driver_active[CNT_DEVICES] = {false};

USBHIDInput *hiddrivers[] = {&joystick};
#define CNT_HIDDEVICES (sizeof(hiddrivers) / sizeof(hiddrivers[0]))
const char *hid_driver_names[CNT_HIDDEVICES] = {"HID_SpaceMouse"};
bool hid_driver_active[CNT_HIDDEVICES] = {false};
//bool show_changed_only = false;


bool spaceMouseConnected() {
    return joystick;
}

void initUsbHost()
{
    myusb.begin();
    if (Serial) {
        Serial.println("[INFO] USB Host initialized");
    }
}

bool getAxisOrButtonChanged() {
    return axisOrButtonChanged;
}

void resetAxisOrButtonChanged() {
    axisOrButtonChanged = false;
}


void updateSpaceMouseData() {
    SpaceMouseData prevData = spaceMouseData; // Store previous state for change detection

    spaceMouseData.xyz[0] = joystick.getAxis(0);
    spaceMouseData.xyz[1] = joystick.getAxis(1);
    spaceMouseData.xyz[2] = joystick.getAxis(2);
    spaceMouseData.rxyz[0] = joystick.getAxis(3);
    spaceMouseData.rxyz[1] = joystick.getAxis(4);
    spaceMouseData.rxyz[2] = joystick.getAxis(5);
    spaceMouseData.buttons = joystick.getButtons();

    bool changed = (spaceMouseData.buttons != prevData.buttons) || memcmp(spaceMouseData.xyz, prevData.xyz, sizeof(spaceMouseData.xyz)) != 0
                   || memcmp(spaceMouseData.rxyz, prevData.rxyz, sizeof(spaceMouseData.rxyz)) != 0;
    if (changed) {
        axisOrButtonChanged = true;
    }
}

JsonDocument getCurrentSpaceMouseDataAsJson() {

    JsonDocument doc;
    doc["type"] = "update";
    // Report translation and rotation axes separately
    JsonArray xyz = doc["sm_xyz"].to<JsonArray>();
    for (int i = 0; i < 3; i++) {
        xyz.add(spaceMouseData.xyz[i]);
    }
    JsonArray rxyz = doc["sm_rxyz"].to<JsonArray>();
    for (int i = 0; i < 3; i++) {
        rxyz.add(spaceMouseData.rxyz[i]);
    }
    doc["sm_buttons"] = spaceMouseData.buttons;
    
    return doc;
}

void sendRawJoystickData() {
    // Send SpaceMouse state only when values change
    static uint32_t last_report_ms = 0;
    static int prev_axes[6] = {-1, -1, -1, -1, -1, -1};
    static uint32_t prev_buttons = 0;
    
    uint32_t now = millis();
    if (joystick && (now - last_report_ms >= 100)) { // Check at 10 Hz
        last_report_ms = now;

        // Read current joystick values
        int curr_axes[6];
        for (int i = 0; i < 6; i++) {
            curr_axes[i] = joystick.getAxis(i);
        }
        uint32_t curr_buttons = joystick.getButtons();

        // Check if any value has changed
        bool changed = (curr_buttons != prev_buttons);
        if (!changed) {
            for (int i = 0; i < 6; i++) {
                if (curr_axes[i] != prev_axes[i]) {
                    changed = true;
                    break;
                }
            }
        }

        // Only send if data has changed
        if (changed) {
            JsonDocument doc;
            // Report translation and rotation axes separately
            JsonArray xyz = doc["sm_xyz"].to<JsonArray>();
            xyz.add(curr_axes[0]); // X
            xyz.add(curr_axes[1]); // Y
            xyz.add(curr_axes[2]); // Z
            JsonArray rxyz = doc["sm_rxyz"].to<JsonArray>();
            rxyz.add(curr_axes[3]); // RX
            rxyz.add(curr_axes[4]); // RY
            rxyz.add(curr_axes[5]); // RZ
            doc["sm_buttons"] = curr_buttons;
            sendJsonToMicroUsb(doc);
            //sendJsonToClient(doc);

            // Update previous values
            for (int i = 0; i < 6; i++) {
                prev_axes[i] = curr_axes[i];
            }
            prev_buttons = curr_buttons;
        }
    }
}

void loopUsbHost()
{
    myusb.Task();
    updateSpaceMouseData();

    //sendRawJoystickData();
}

