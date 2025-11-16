#include <Arduino.h>
#include <USBHost_t36.h>

const int LED_PIN = LED_BUILTIN;

// SpaceMouse data storage
struct SpaceMouseData {
  int16_t tx = 0;  // Translation X
  int16_t ty = 0;  // Translation Y
  int16_t tz = 0;  // Translation Z
  int16_t rx = 0;  // Rotation X (roll)
  int16_t ry = 0;  // Rotation Y (pitch)
  int16_t rz = 0;  // Rotation Z (yaw)
  uint8_t buttons = 0;
} spaceMouseData;

// Custom HID parser for SpaceMouse
class SpaceMouseParser : public USBHIDParser {
public:
  SpaceMouseParser(USBHost &host) : USBHIDParser(host) { }
  
protected:
  enum { 
    SPACEMOUSE_VID_3DCONNEXION = 0x256f,  // Modern 3Dconnexion VID
    SPACEMOUSE_VID_LOGITECH = 0x046d,     // Older SpaceMouse models (Logitech VID)
    SPACEMOUSE_COMPACT_PID = 0xc635 
  };
  
  bool claim(Device_t *device, int type, const uint8_t *descriptors, uint32_t len) override {
    // Try to claim SpaceMouse devices
    Serial.print("[SpaceMouse Parser] Claim attempt - Type: ");
    Serial.print(type);
    Serial.print(" VID:PID = 0x");
    Serial.print(device->idVendor, HEX);
    Serial.print(":0x");
    Serial.print(device->idProduct, HEX);
    Serial.print(" Descriptor len: ");
    Serial.println(len);
    
    if (type == 0) {
      // Accept 3Dconnexion SpaceMouse devices (both old Logitech VID and new 3Dconnexion VID)
      // Common SpaceMouse PIDs: 0xc603, 0xc605, 0xc606, 0xc621, 0xc623, 0xc625, 0xc626, 0xc627, 0xc628, 0xc629, 0xc62b, 0xc635
      if (device->idVendor == SPACEMOUSE_VID_3DCONNEXION || device->idVendor == SPACEMOUSE_VID_LOGITECH) {
        Serial.println(" -> CLAIMED as SpaceMouse!");
        
        // Print HID descriptor
        Serial.print("HID Descriptor (first 64 bytes): ");
        for (uint32_t i = 0; i < min(len, (uint32_t)64); i++) {
          if (descriptors[i] < 0x10) Serial.print("0");
          Serial.print(descriptors[i], HEX);
          Serial.print(" ");
        }
        Serial.println();
        
        return true;
      }
    } else if (type == 1) {
      // Also try claiming on interface descriptor
      Serial.print(" -> Checking interface, Class: ");
      if (len >= 9) {
        Serial.print(descriptors[5], HEX);
        Serial.print(" SubClass: ");
        Serial.print(descriptors[6], HEX);
        Serial.print(" Protocol: ");
        Serial.println(descriptors[7], HEX);
        
        // HID class is 0x03
        if (descriptors[5] == 0x03) {
          Serial.println(" -> CLAIMED as HID interface!");
          return true;
        }
      }
    }
    Serial.println(" -> Not claimed");
    return false;
  }
  
  void init() {
    Serial.println("[SpaceMouse] Init called - device initialized");
    USBHIDParser::init();
  }
  
  void disconnect() override {
    Serial.println("[SpaceMouse] Disconnected");
  }
  
  void control(const Transfer_t *transfer) override {
    Serial.println("[SpaceMouse] Control transfer received");
  }
  
  void timer_event(USBDriverTimer *timer) override {
    Serial.println("[SpaceMouse] Timer event");
  }
  
  void hid_input_begin(uint32_t topusage, uint32_t type, int lgmin, int lgmax) {
    Serial.print("[SpaceMouse] HID input begin - Usage: 0x");
    Serial.print(topusage, HEX);
    Serial.print(" Type: ");
    Serial.println(type);
  }
  
  void hid_input_data(uint32_t usage, int32_t value) {
    Serial.print("[SpaceMouse] Usage: 0x");
    Serial.print(usage, HEX);
    Serial.print(" Value: ");
    Serial.println(value);
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }
  
  void hid_input_end() {
    Serial.println("[SpaceMouse] HID input end");
  }
  
  void parse(const uint8_t *buffer, uint32_t len, const uint8_t *setup) {
    // Always print raw data first for debugging
    Serial.print("[SpaceMouse Parse] Len: ");
    Serial.print(len);
    Serial.print(" Data: ");
    for (uint32_t i = 0; i < min(len, (uint32_t)20); i++) {
      if (buffer[i] < 0x10) Serial.print("0");
      Serial.print(buffer[i], HEX);
      Serial.print(" ");
    }
    Serial.println();
    
    if (len < 7) {
      Serial.println("  -> Too short, ignoring");
      return;
    }
    
    // SpaceMouse Compact sends data in report ID 0x01 for motion data
    if (buffer[0] == 0x01 && len >= 7) {
      // Translation data (bytes 1-6)
      spaceMouseData.tx = (int16_t)(buffer[1] | (buffer[2] << 8));
      spaceMouseData.ty = (int16_t)(buffer[3] | (buffer[4] << 8));
      spaceMouseData.tz = (int16_t)(buffer[5] | (buffer[6] << 8));
      
      // Print translation data
      Serial.print("  -> TX: ");
      Serial.print(spaceMouseData.tx);
      Serial.print(" TY: ");
      Serial.print(spaceMouseData.ty);
      Serial.print(" TZ: ");
      Serial.print(spaceMouseData.tz);
      
      if (len >= 13) {
        // Rotation data (bytes 7-12)
        spaceMouseData.rx = (int16_t)(buffer[7] | (buffer[8] << 8));
        spaceMouseData.ry = (int16_t)(buffer[9] | (buffer[10] << 8));
        spaceMouseData.rz = (int16_t)(buffer[11] | (buffer[12] << 8));
        
        Serial.print(" RX: ");
        Serial.print(spaceMouseData.rx);
        Serial.print(" RY: ");
        Serial.print(spaceMouseData.ry);
        Serial.print(" RZ: ");
        Serial.print(spaceMouseData.rz);
      }
      
      Serial.println();
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
    // SpaceMouse button data (report ID 0x03)
    else if (buffer[0] == 0x03 && len >= 2) {
      spaceMouseData.buttons = buffer[1];
      Serial.print("  -> Buttons: 0x");
      Serial.println(spaceMouseData.buttons, HEX);
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
    // Check for report without report ID (some devices don't use report IDs)
    else if (len >= 12) {
      // Try parsing as if there's no report ID byte
      spaceMouseData.tx = (int16_t)(buffer[0] | (buffer[1] << 8));
      spaceMouseData.ty = (int16_t)(buffer[2] | (buffer[3] << 8));
      spaceMouseData.tz = (int16_t)(buffer[4] | (buffer[5] << 8));
      spaceMouseData.rx = (int16_t)(buffer[6] | (buffer[7] << 8));
      spaceMouseData.ry = (int16_t)(buffer[8] | (buffer[9] << 8));
      spaceMouseData.rz = (int16_t)(buffer[10] | (buffer[11] << 8));
      
      Serial.print("  -> (No ReportID) TX: ");
      Serial.print(spaceMouseData.tx);
      Serial.print(" TY: ");
      Serial.print(spaceMouseData.ty);
      Serial.print(" TZ: ");
      Serial.print(spaceMouseData.tz);
      Serial.print(" RX: ");
      Serial.print(spaceMouseData.rx);
      Serial.print(" RY: ");
      Serial.print(spaceMouseData.ry);
      Serial.print(" RZ: ");
      Serial.println(spaceMouseData.rz);
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
    else {
      Serial.println("  -> Unknown format");
    }
  }
};

// Create USB Host object
USBHost myusb;
USBHub hub1(myusb);
USBHub hub2(myusb);

// Create USB device objects for different types
MouseController mouse(myusb);
KeyboardController keyboard(myusb);
SpaceMouseParser spaceMouse(myusb);
USBHIDParser hid1(myusb);
USBHIDParser hid2(myusb);
USBHIDParser hid3(myusb);
USBHIDParser hid4(myusb);

// Variables to store mouse state
int mouseX = 0;
int mouseY = 0;
int mouseWheel = 0;
uint8_t mouseButtons = 0;

// Device tracking variables
bool mouseConnected = false;
bool keyboardConnected = false;
bool spaceMouseConnected = false;
bool hid1Connected = false;
bool hid2Connected = false;

// Keyboard event buffer
volatile int keyPressBuffer = 0;
volatile int keyReleaseBuffer = 0;
volatile bool keyPressFlag = false;
volatile bool keyReleaseFlag = false;

// Keyboard callback function for key press events
void onKeyPress(int key) {
  keyPressBuffer = key;
  keyPressFlag = true;
  digitalWrite(LED_PIN, !digitalRead(LED_PIN));
}

// Keyboard callback function for key release events
void onKeyRelease(int key) {
  keyReleaseBuffer = key;
  keyReleaseFlag = true;
}

void printDeviceInfo() {
  Serial.println("\n=== USB DEVICE INFORMATION ===");
  bool deviceFound = false;
  
  // Check mouse
  if (mouse.available()) {
    Serial.println("Device Type: Mouse");
    deviceFound = true;
  }
  
  // Check keyboard
  if (keyboard) {
    Serial.println("Device Type: Keyboard");
    deviceFound = true;
  }
  
  // Check SpaceMouse
  if (spaceMouse) {
    Serial.println("Device Type: SpaceMouse Parser (CLAIMED)");
    Serial.print("  VID: 0x"); Serial.println(spaceMouse.idVendor(), HEX);
    Serial.print("  PID: 0x"); Serial.println(spaceMouse.idProduct(), HEX);
    deviceFound = true;
  }
  
  // Check HID devices
  if (hid1) {
    Serial.println("Device Type: Generic HID Device 1");
    Serial.print("  VID: 0x"); Serial.println(hid1.idVendor(), HEX);
    Serial.print("  PID: 0x"); Serial.println(hid1.idProduct(), HEX);
    deviceFound = true;
  }
  
  if (hid2) {
    Serial.println("Device Type: Generic HID Device 2");
    Serial.print("  VID: 0x"); Serial.println(hid2.idVendor(), HEX);
    Serial.print("  PID: 0x"); Serial.println(hid2.idProduct(), HEX);
    deviceFound = true;
  }
  
  if (hid3) {
    Serial.println("Device Type: Generic HID Device 3");
    Serial.print("  VID: 0x"); Serial.println(hid3.idVendor(), HEX);
    Serial.print("  PID: 0x"); Serial.println(hid3.idProduct(), HEX);
    deviceFound = true;
  }
  
  if (hid4) {
    Serial.println("Device Type: Generic HID Device 4");
    Serial.print("  VID: 0x"); Serial.println(hid4.idVendor(), HEX);
    Serial.print("  PID: 0x"); Serial.println(hid4.idProduct(), HEX);
    deviceFound = true;
  }
  
  if (!deviceFound) {
    Serial.println("No devices detected");
  }
  
  Serial.println("==============================\n");
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  
  // Initialize serial communication
  Serial.begin(115200);
  while (!Serial && millis() < 3000); // Wait up to 3 seconds for serial
  
  Serial.println("Teensy 4.1 USB Device Reader");
  Serial.println("=============================");
  Serial.println("Waiting for USB Devices...");
  
  // Attach keyboard callbacks
  keyboard.attachPress(onKeyPress);
  keyboard.attachRelease(onKeyRelease);
  
  // Start USB host
  myusb.begin();
  
  digitalWrite(LED_PIN, HIGH);
}

void loop() {
  // Process USB tasks
  myusb.Task();
  
  // Check for device connection/disconnection changes
  bool currentMouseConnected = mouse.available();
  bool currentKeyboardConnected = (bool)keyboard;
  bool currentSpaceMouseConnected = (bool)spaceMouse;
  bool currentHid1Connected = (bool)hid1;
  bool currentHid2Connected = (bool)hid2;
  
  // Detect new device connections - Mouse
  if (currentMouseConnected && !mouseConnected) {
    Serial.println("\n>>> Mouse CONNECTED <<<");
    printDeviceInfo();
    mouseConnected = true;
  } else if (!currentMouseConnected && mouseConnected) {
    /*
    Serial.println("\n>>> Mouse DISCONNECTED <<<\n");
    mouseConnected = false;
    */
  }
  
  // Detect new device connections - Keyboard
  if (currentKeyboardConnected && !keyboardConnected) {
    Serial.println("\n>>> Keyboard CONNECTED <<<");
    printDeviceInfo();
    keyboardConnected = true;
  } else if (!currentKeyboardConnected && keyboardConnected) {
    Serial.println("\n>>> Keyboard DISCONNECTED <<<\n");
    keyboardConnected = false;
  }
  
  // Detect new device connections - SpaceMouse
  if (currentSpaceMouseConnected && !spaceMouseConnected) {
    Serial.println("\n>>> SpaceMouse CONNECTED <<<");
    printDeviceInfo();
    spaceMouseConnected = true;
  } else if (!currentSpaceMouseConnected && spaceMouseConnected) {
    Serial.println("\n>>> SpaceMouse DISCONNECTED <<<\n");
    spaceMouseConnected = false;
  }
  
  // Detect new device connections - HID1
  if (currentHid1Connected && !hid1Connected) {
    Serial.println("\n>>> HID Device 1 CONNECTED <<<");
    printDeviceInfo();
    hid1Connected = true;
  } else if (!currentHid1Connected && hid1Connected) {
    Serial.println("\n>>> HID Device 1 DISCONNECTED <<<\n");
    hid1Connected = false;
  }
  
  // Detect new device connections - HID2
  if (currentHid2Connected && !hid2Connected) {
    Serial.println("\n>>> HID Device 2 CONNECTED <<<");
    printDeviceInfo();
    hid2Connected = true;
  } else if (!currentHid2Connected && hid2Connected) {
    Serial.println("\n>>> HID Device 2 DISCONNECTED <<<\n");
    hid2Connected = false;
  }
  
  // Debug SpaceMouse status every 2 seconds
  static unsigned long lastSpaceMouseDebug = 0;
  if (spaceMouseConnected && millis() - lastSpaceMouseDebug > 2000) {
    Serial.println("[Debug] SpaceMouse is connected - waiting for data...");
    lastSpaceMouseDebug = millis();
  }

  bool mouseKeyPressed = false;
  if (mouseConnected) {
    static uint8_t lastMouseKeys = 0;
    uint8_t mouseKeys = mouse.getButtons();
    if (mouseKeys != lastMouseKeys) {
      mouseKeyPressed = true;
      lastMouseKeys = mouseKeys;
    }
  }
  
  // Handle mouse data if mouse is connected
  if (mouse.available() || mouseKeyPressed || mouseConnected) {
    // Get mouse data
    int deltaX = mouse.getMouseX();
    int deltaY = mouse.getMouseY();
    int deltaWheel = mouse.getWheel();
    uint8_t buttons = mouse.getButtons();
    
    // Update accumulated position
    mouseX += deltaX;
    mouseY += deltaY;
    mouseWheel += deltaWheel;
    
    // Print data if there's any movement or button change
    if (deltaX != 0 || deltaY != 0 || deltaWheel != 0 || buttons != mouseButtons) {
      Serial.print("[Mouse] X: ");
      Serial.print(deltaX);
      Serial.print(" (");
      Serial.print(mouseX);
      Serial.print(") | Y: ");
      Serial.print(deltaY);
      Serial.print(" (");
      Serial.print(mouseY);
      Serial.print(") | Wheel: ");
      Serial.print(deltaWheel);
      Serial.print(" (");
      Serial.print(mouseWheel);
      Serial.print(") | Buttons: ");
      
      // Print button state
      if (buttons & 0x01) Serial.print("LEFT ");
      if (buttons & 0x02) Serial.print("RIGHT ");
      if (buttons & 0x04) Serial.print("MIDDLE ");
      if (buttons & 0x08) Serial.print("BACK ");
      if (buttons & 0x10) Serial.print("FORWARD ");
      
      Serial.println();
      
      mouseButtons = buttons;
      
      // Blink LED on activity
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }
  }
  
  // Handle keyboard events from callbacks (safe to print in main loop)
  if (keyPressFlag) {
    keyPressFlag = false;
    int key = keyPressBuffer;
    Serial.print("[Keyboard] Key pressed: ");
    if (key >= 32 && key <= 126) {
      Serial.print("'");
      Serial.print((char)key);
      Serial.print("' ");
    }
    Serial.print("(0x");
    Serial.print(key, HEX);
    Serial.println(")");
  }
  
  if (keyReleaseFlag) {
    keyReleaseFlag = false;
    int key = keyReleaseBuffer;
    Serial.print("[Keyboard] Key released: ");
    if (key >= 32 && key <= 126) {
      Serial.print("'");
      Serial.print((char)key);
      Serial.print("' ");
    }
    Serial.print("(0x");
    Serial.print(key, HEX);
    Serial.println(")");
  }
  
  // Periodic status update if nothing is connected
  static unsigned long lastStatusUpdate = 0;
  if (!mouseConnected && !keyboardConnected && !spaceMouseConnected && !hid1Connected && !hid2Connected && 
      millis() - lastStatusUpdate > 5000) {
    
    bool anyDevice = (bool)hub1 || (bool)hub2 || (bool)hid3 || (bool)hid4 || (bool)spaceMouse;
    
    if (anyDevice) {
      Serial.println("USB device connected (checking for additional devices):");
      printDeviceInfo();
    } else {
      Serial.println("No USB devices detected...");
    }
    
    lastStatusUpdate = millis();
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
  }

  if (mouse.available()) {
    mouse.mouseDataClear();
  }
  //delay(200);
}

