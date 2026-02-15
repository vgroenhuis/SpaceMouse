/*
Minimal working example for SpaceMouse.
It writes to serial monitor:
- connection and disconnection events of up to two USB hubs, one USB serial device, and up to two HID devices.
- connection and disconnection of a joystick device.
- if joystick device is connected: the values of the six axes and button state.
*/

#include "USBHost_t36.h"
#include <Watchdog_t4.h>
#include <imxrt.h>

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

WDT_T4<WDT1> wdt;

void blinkLed(float freq, float duty)
{
    static unsigned long lastBlink = 0;
    static bool ledState = false;
    unsigned long now = millis();
    unsigned long onDuration = (unsigned long)(1000.0f * duty / freq);
    unsigned long offDuration = (unsigned long)(1000.0f * (1.0f - duty) / freq);

    if (ledState && (now - lastBlink >= onDuration))
    { // LED on for onDuration
        digitalWrite(LED_BUILTIN, LOW);
        ledState = false;
        lastBlink = now;
    }
    else if (!ledState && (now - lastBlink >= offDuration))
    { // LED off for offDuration
        digitalWrite(LED_BUILTIN, HIGH);
        ledState = true;
        lastBlink = now;
    }
}

void printLastResetCause()
{
    uint32_t cause = SRC_SRSR;
    Serial.printf("Last reset cause: 0x%08X\n", cause);

    if (cause & SRC_SRSR_IPP_RESET_B)
        Serial.println("Power reset");
    if (cause & SRC_SRSR_WDOG_RST_B)
        Serial.println("Watchdog reset");
    if (cause & SRC_SRSR_WDOG3_RST_B)
        Serial.println("Watchdog3 reset");
    if (cause & SRC_SRSR_JTAG_RST_B)
        Serial.println("JTAG reset");
    if (cause & SRC_SRSR_IPP_USER_RESET_B)
        Serial.println("RESET pin");
}

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH);

    Serial.begin(115200);
    while (!Serial)
        ; // wait until host opens serial monitor
    Serial.println();
    printLastResetCause();
    myusb.begin();
    Serial.println("SpaceMouse minimal working example");

    // Configure watchdog to reset after 5 seconds if not fed
    WDT_timings_t config;
    //config.trigger = 2;
    config.timeout = 4;
    wdt.begin(config);
    Serial.println("Watchdog enabled: 4 second timeout");
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
    // Feed the watchdog to prevent reset
    wdt.feed();

    myusb.Task();
    PrintDeviceListChanges();

    if (joystick)
    {
        Serial.printf("SpaceMouse: %d %d %d %d %d %d buttons: %d\n", joystick.getAxis(0), joystick.getAxis(1), joystick.getAxis(2), joystick.getAxis(3), joystick.getAxis(4), joystick.getAxis(5), joystick.getButtons());
        blinkLed(2, 0.9);
    }
    else if (hub1)
    {
        blinkLed(2, 0.5);
    }
    else
    {
        blinkLed(2, 0.1);
    }
    delay(50);
}