#include "USBHost_t36.h"
#include <Watchdog_t4.h>

USBHost myusb;
USBHub hub1(myusb);
USBSerial userial(myusb);
WDT_T4<WDT1> wdt;

void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    myusb.begin();
    wdt.begin({.timeout = 2});
}

void loop()
{
    myusb.Task(); // commenting this out does not change crash behaviour
    wdt.feed();
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(100);
}