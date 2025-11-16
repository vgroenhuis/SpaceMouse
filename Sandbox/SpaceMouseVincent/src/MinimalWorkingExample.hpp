/*
Minimal working example for SpaceMouse.
It writes to serial monitor:
- connection and disconnection events of up to two USB hubs, one USB serial device, and up to two HID devices.
- connection and disconnection of a joystick device.
- if joystick device is connected: the values of the six axes and button state.
*/

#include "USBHost_t36.h"

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

void setup()
{
    myusb.begin();
    Serial.begin(115200);
    Serial.println("\nSpaceMouse minimal working example");
    pinMode(LED_BUILTIN, OUTPUT);
}


//=============================================================================
// Show when devices are added or removed
//=============================================================================
void PrintDeviceListChanges()
{
    for (uint8_t i = 0; i < CNT_DEVICES; i++)
    {
        if (*drivers[i] != driver_active[i])
        {
            if (driver_active[i])
            {
                Serial.printf("*** Device %s - disconnected ***\n", driver_names[i]);
                driver_active[i] = false;
            }
            else
            {
                Serial.printf("\n*** Device %s %x:%x - connected ***\n", driver_names[i], drivers[i]->idVendor(), drivers[i]->idProduct());
                driver_active[i] = true;

                const uint8_t *psz = drivers[i]->manufacturer();
                if (psz && *psz)
                    Serial.printf("  manufacturer: %s\n", psz);
                psz = drivers[i]->product();
                if (psz && *psz)
                    Serial.printf("  product: %s\n", psz);
                psz = drivers[i]->serialNumber();
                if (psz && *psz)
                    Serial.printf("  Serial: %s\n", psz);
            }
        }
    }

    for (uint8_t i = 0; i < CNT_HIDDEVICES; i++)
    {
        if (*hiddrivers[i] != hid_driver_active[i])
        {
            if (hid_driver_active[i])
            {
                Serial.printf("*** HID Device %s - disconnected ***\n", hid_driver_names[i]);
                hid_driver_active[i] = false;
            }
            else
            {
                Serial.printf("\n*** HID Device %s %x:%x - connected ***\n", hid_driver_names[i], hiddrivers[i]->idVendor(), hiddrivers[i]->idProduct());
                hid_driver_active[i] = true;

                const uint8_t *psz = hiddrivers[i]->manufacturer();
                if (psz && *psz)
                    Serial.printf("  manufacturer: %s\n", psz);
                psz = hiddrivers[i]->product();
                if (psz && *psz)
                    Serial.printf("  product: %s\n", psz);
                psz = hiddrivers[i]->serialNumber();
                if (psz && *psz)
                    Serial.printf("  Serial: %s\n", psz);
            }
        }
    }
}

void loop()
{
    myusb.Task();
    PrintDeviceListChanges();

    if (hid_driver_active[0])
    {
        Serial.printf("SpaceMouse: %d %d %d %d %d %d buttons: %d\n", joystick.getAxis(0), joystick.getAxis(1), joystick.getAxis(2), joystick.getAxis(3), joystick.getAxis(4), joystick.getAxis(5), joystick.getButtons());
    }

    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(100);
}