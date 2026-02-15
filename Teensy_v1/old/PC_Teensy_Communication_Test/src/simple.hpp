#include "USBHost_t36.h"
USBHost myusb;
USBHub hub1(myusb);
USBSerial userial(myusb);
void setup()
{
    pinMode(LED_BUILTIN, OUTPUT);
    myusb.begin();
}
void loop()
{
    myusb.Task(); // commenting this out does not change crash behaviour
    digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN));
    delay(100);
}