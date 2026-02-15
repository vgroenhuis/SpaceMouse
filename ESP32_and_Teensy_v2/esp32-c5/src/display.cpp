#include "display.h"
#include "web.h"
#include "teensy.h"
#include "Fonts/FreeMono12pt7b.h"
#include "Fonts/FreeSans9pt7b.h"
#include "Fonts/FreeSans12pt7b.h"
#include "Fonts/FreeSans18pt7b.h"

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &spi, OLED_DC, OLED_RESET, OLED_CS);
SPIClass spi = SPIClass(FSPI);


void setupDisplay() {
    spi.begin(OLED_CLK, -1, OLED_MOSI, OLED_CS);
    spi.setFrequency(8000000); // 1 MHz for testing, or 8 MHz for production

    if (!display.begin(SSD1306_SWITCHCAPVCC)) {
        Serial.println(F("SSD1306 does not work!"));
        for (;;); // Don't proceed, loop forever
    }
    display.setFont(&FreeSans9pt7b);
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    display.clearDisplay();
    display.setCursor(0, 15);
    display.println("Connecting");
    display.display();

    Serial.println("Display setup completed.");
}

void updateDisplay() {
    display.clearDisplay();
    display.setCursor(0, 15);
    display.setTextSize(1);
    //display.println("SpaceMouse");
    display.setTextSize(1);
    if (manager.isConnected()) {
        display.println(manager.getConnectedIp());
    } else {
        display.println("WiFi fail");
    }
    
    display.println(getTeensyStatus().c_str());
    display.display();
}