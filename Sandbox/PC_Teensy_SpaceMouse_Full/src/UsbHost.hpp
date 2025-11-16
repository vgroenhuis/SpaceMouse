/*
    Manages communication with various USB devices through the USB Host.
    Including: SpaceMouse, USB hub, USB serial, etc.
*/

#include "USBHost_t36.h"
#include <ArduinoJson.h>

// Forward declaration
void sendJson(JsonDocument &doc) {
  String out;
  serializeJson(doc, out);
  Serial.println(out);   // newline-delimited JSON
}

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
bool show_changed_only = false;


bool spaceMouseConnected() {
    return joystick;
}

// Dominant axis detection parameters
static const int DOMINANT_DEADZONE = 200; // Minimum absolute axis value to consider active
static const uint32_t DOMINANT_REPORT_INTERVAL_MS = 100; // 10 Hz like other reports

// Track last reported axis/direction to suppress redundant messages
static int last_dominant_axis_index = -1; // -1 means none
static int last_dominant_direction = 0;   // -1 backward, +1 forward, 0 none


void setupUsbHost()
{
    myusb.begin();
    Serial.println("[INFO] USB Host initialized");
}


//=============================================================================
// Show when devices are added or removed
//=============================================================================
void printDeviceListChanges()
{
    for (uint8_t i = 0; i < CNT_DEVICES; i++)
    {
        if (*drivers[i] != driver_active[i])
        {
            if (driver_active[i])
            {
                Serial.printf("[INFO] Device %s disconnected\n", driver_names[i]);
                driver_active[i] = false;
            }
            else
            {
                Serial.printf("[INFO] Device %s %x:%x connected:", driver_names[i], drivers[i]->idVendor(), drivers[i]->idProduct());
                driver_active[i] = true;

                const uint8_t *psz = drivers[i]->manufacturer();
                if (psz && *psz)
                    Serial.printf("  manufacturer: %s", psz);
                psz = drivers[i]->product();
                if (psz && *psz)
                    Serial.printf("  product: %s", psz);
                psz = drivers[i]->serialNumber();
                if (psz && *psz)
                    Serial.printf("  Serial: %s", psz);
                Serial.println("]");
            }
        }
    }

    for (uint8_t i = 0; i < CNT_HIDDEVICES; i++)
    {
        if (*hiddrivers[i] != hid_driver_active[i])
        {
            if (hid_driver_active[i])
            {
                Serial.printf("[INFO] HID Device %s disconnected\n", hid_driver_names[i]);
                hid_driver_active[i] = false;
                
                // Send JSON status update
                JsonDocument doc;
                doc["spacemouse_connected"] = false;
                sendJson(doc);
            }
            else
            {
                Serial.printf("[INFO] HID Device %s %x:%x connected:", hid_driver_names[i], hiddrivers[i]->idVendor(), hiddrivers[i]->idProduct());
                hid_driver_active[i] = true;

                const uint8_t *psz = hiddrivers[i]->manufacturer();
                if (psz && *psz)
                    Serial.printf("  manufacturer: %s", psz);
                psz = hiddrivers[i]->product();
                if (psz && *psz)
                    Serial.printf("  product: %s", psz);
                psz = hiddrivers[i]->serialNumber();
                if (psz && *psz)
                    Serial.printf("  Serial: %s", psz);
                Serial.println("]");
                
                // Send JSON status update
                JsonDocument doc;
                doc["spacemouse_connected"] = true;
                sendJson(doc);
            }
        }
    }
}

void sendRawJoystickDataToHost() {
    // Send SpaceMouse state at 10 Hz when connected
    static uint32_t last_report_ms = 0;
    uint32_t now = millis();
    if (joystick && (now - last_report_ms >= 100)) {
        last_report_ms = now;

        JsonDocument doc;
        // Report translation and rotation axes separately
        JsonArray xyz = doc["sm_xyz"].to<JsonArray>();
        xyz.add(joystick.getAxis(0)); // X
        xyz.add(joystick.getAxis(1)); // Y
        xyz.add(joystick.getAxis(2)); // Z
        JsonArray rxyz = doc["sm_rxyz"].to<JsonArray>();
        rxyz.add(joystick.getAxis(3)); // RX
        rxyz.add(joystick.getAxis(4)); // RY
        rxyz.add(joystick.getAxis(5)); // RZ
        doc["sm_buttons"] = joystick.getButtons();
        sendJson(doc);
    }
}

void detectDominantAxis() {
    if (!spaceMouseConnected()) return;

    static uint32_t last_ms = 0;
    uint32_t now = millis();
    if (now - last_ms < DOMINANT_REPORT_INTERVAL_MS) return;
    last_ms = now;

    // Read 6 axes: X,Y,Z,RX,RY,RZ
    int axes[6];
    for (int i = 0; i < 6; i++) axes[i] = joystick.getAxis(i);

    // Determine dominant axis above deadzone
    int dominant_index = -1;
    int dominant_abs = DOMINANT_DEADZONE;
    for (int i = 0; i < 6; i++) {
        int av = abs(axes[i]);
        if (av > dominant_abs) {
            dominant_abs = av;
            dominant_index = i;
        }
    }



    int direction = 0; // -1 backward, +1 forward
    if (dominant_index >= 0) {
        direction = (axes[dominant_index] > 0) ? 1 : -1;
    }

    // Only send if changed OR active axis present (periodic refresh while active)
    bool changed = (dominant_index != last_dominant_axis_index) || (direction != last_dominant_direction);
    if (!changed && dominant_index < 0) return; // nothing active and no change

    last_dominant_axis_index = dominant_index;
    last_dominant_direction = direction;

    JsonDocument doc;
    const char *axis_names[6] = {"X", "Y", "Z", "RX", "RY", "RZ"};
    if (dominant_index >= 0) {
        doc["dominant_axis_index"] = dominant_index;          // 0..5
        doc["dominant_axis"] = axis_names[dominant_index];    // Human-readable
        doc["dominant_direction"] = direction;                // +1 or -1
    } else {
        doc["dominant_axis_index"] = -1;
        doc["dominant_axis"] = "none";
        doc["dominant_direction"] = 0;                        // none
    }
    sendJson(doc);
}

void usbHostLoop()
{
    myusb.Task();
    printDeviceListChanges();
    sendRawJoystickDataToHost();
    detectDominantAxis();
}

