/*
    Manages communication with various USB devices through the USB Host.
    Including: SpaceMouse, USB hub, USB serial, etc.
*/

#include "USBHost_t36.h"
#include <ArduinoJson.h>

// Define global storage for robot joints
int g_robot_joints[8] = {0,0,0,0,0,0,0,0};

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

void sendJsonToHost(JsonDocument &doc) {
    if (Serial) {
        String out;
        serializeJson(doc, out);
        Serial.println(out);
    }
}

void sendJsonToClient(JsonDocument &doc) {
    if (userial && userial.availableForWrite()) {
        String out;
        serializeJson(doc, out);
        userial.println(out);
    }
}

bool spaceMouseConnected() {
    return joystick;
}

bool usbSerialConnected() {
    return userial;
}

// Dominant axis detection parameters
static const int DOMINANT_DEADZONE = 150; // Minimum absolute axis value to consider active
static const uint32_t DOMINANT_REPORT_INTERVAL_MS = 100;

// Track last reported axis/direction to suppress redundant messages
static int last_dominant_axis_index = -1; // -1 means none
static int last_dominant_direction = 0;   // -1 backward, +1 forward, 0 none

// USB Serial line buffer for robot joint positions
static String usbserial_line_buffer = "";
static uint32_t usbserial_connected_ms = 0; // Track when USB Serial was connected


void setupUsbHost()
{
    myusb.begin();
    if (Serial) {
        Serial.println("[INFO] USB Host initialized");
    }
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



                if (drivers[i] == &userial) {
                    // Send JSON status update
                    JsonDocument doc;
                    doc["usbserial_connected"] = false;
                    sendJsonToHost(doc);
                    // Reset connection time
                    usbserial_connected_ms = 0;
                }
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

                if (drivers[i] == &userial) {
                    // Send JSON status update
                    userial.begin(115200);
                    JsonDocument doc;
                    doc["usbserial_connected"] = true;
                    sendJsonToHost(doc);
                    // Record connection time
                    usbserial_connected_ms = millis();
                }
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
                
                if (hiddrivers[i] == &joystick) {
                    // Send JSON status update
                    JsonDocument doc;
                    doc["spacemouse_connected"] = false;
                    sendJsonToHost(doc);
                }
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
                
                if (hiddrivers[i] == &joystick) {
                    // Send JSON status update
                    JsonDocument doc;
                    doc["spacemouse_connected"] = true;
                    sendJsonToHost(doc);
                }
            }
        }
    }
}

void sendRawJoystickData() {
    // Send SpaceMouse state at 10 Hz when connected
    static uint32_t last_report_ms = 0;
    uint32_t now = millis();
    if (joystick && (now - last_report_ms >= 100)) { // 1 Hz
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
        sendJsonToHost(doc);
        sendJsonToClient(doc);
    }
}

void detectSendDominantAxis() {
    static int qSent = 0;
    if (!spaceMouseConnected()) {
        return;
    }

    static uint32_t last_ms = 0;
    uint32_t now = millis();
    if (now - last_ms < DOMINANT_REPORT_INTERVAL_MS) {
        return;
    }
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
    if (!changed) {
        // we send the Q command several times to be sure it is processed
        if (dominant_index < 0) {
            qSent++;
            if (qSent >= 5) {
                return;
            }
        } else {
            qSent = 0;
            return;
        }
    }

    last_dominant_axis_index = dominant_index;
    last_dominant_direction = direction;

    int targetJoints[8];
    for (int i = 0; i < 8; i++) {
        targetJoints[i] = g_robot_joints[i];
    }

    if (joystick.getButtons() == 0) {
        if (dominant_index == 0) { // X
            targetJoints[0] += direction * 2000;
        } else if (dominant_index == 5) { // RZ
            targetJoints[1] -= direction * 2000;
        } else if (dominant_index == 2) { // Z
            targetJoints[2] -= direction * 2000;
            targetJoints[3] -= direction * 2000;
        } else if (dominant_index == 3) { // RX
            targetJoints[2] += direction * 2000;
            targetJoints[3] -= direction * 2000;
        }
    } else {
        // needle forward/backward control
        if (dominant_index == 1) { // Y
            targetJoints[4] -= direction * 2000;
        } else if (dominant_index == 3) { // RX
            targetJoints[4] -= direction * 2000;
        }
    }

    // Send setpoint array to PC host for display
    {
        JsonDocument setpointDoc;
        JsonArray setpoint_array = setpointDoc["robot_setpoint"].to<JsonArray>();
        for (int i = 0; i < 8; i++) {
            setpoint_array.add(targetJoints[i]);
        }
        sendJsonToHost(setpointDoc);
    }

    if (userial) {
        if (dominant_index < 0) {
            userial.println("Q"); // brake command
            JsonDocument txDoc;
            txDoc["usbserial_tx"] = "Q";
            sendJsonToHost(txDoc);
        } else {
            char cmd[64];
            snprintf(cmd, sizeof(cmd), "A%d B%d C%d D%d E%d F%d G%d H%d",
                    targetJoints[0], targetJoints[1], targetJoints[2], targetJoints[3],
                    targetJoints[4], targetJoints[5], targetJoints[6], targetJoints[7]);
            userial.printf("%s\n",cmd);

            JsonDocument txDoc;
            txDoc["usbserial_tx"] = cmd; // command content without newline
            sendJsonToHost(txDoc);
        }
    }

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
    sendJsonToHost(doc);
}

void processUsbSerialInput() {
    int available = userial.available();
    if (available > 0) {
        // Read all available bytes and buffer them
        while (userial.available() > 0) {
            int ch = userial.read();
            if (ch >= 0) {
                char c = (char)ch;
                if (c == '\n') {
                    // Complete line received
                    if (usbserial_line_buffer.length() > 0) {
                        // Check if this looks like robot joint positions (8 comma-separated numbers)
                        int comma_count = 0;
                        for (unsigned int i = 0; i < usbserial_line_buffer.length(); i++) {
                            if (usbserial_line_buffer[i] == ',') comma_count++;
                        }
                        
                        if (comma_count == 7) {
                            // Parse as robot joint positions
                            int joints[8];
                            int joint_idx = 0;
                            int start_idx = 0;
                            for (unsigned int i = 0; i <= usbserial_line_buffer.length(); i++) {
                                if (i == usbserial_line_buffer.length() || usbserial_line_buffer[i] == ',') {
                                    if (joint_idx < 8) {
                                        joints[joint_idx++] = usbserial_line_buffer.substring(start_idx, i).toInt();
                                    }
                                    start_idx = i + 1;
                                }
                            }
                            
                            // Send robot joints as JSON
                            if (joint_idx == 8) {
                                JsonDocument doc;
                                JsonArray joints_array = doc["robot_joints"].to<JsonArray>();
                                for (int i = 0; i < 8; i++) {
                                    joints_array.add(joints[i]);
                                }
                                sendJsonToHost(doc);

                                // Store latest joints into global variable
                                for (int i = 0; i < 8; i++) {
                                    g_robot_joints[i] = joints[i];
                                }

                                // Also forward the original line to host log as usbserial_data
                                // Trim optional carriage return if present
                                String raw_line = usbserial_line_buffer;
                                if (raw_line.endsWith("\r")) raw_line.remove(raw_line.length() - 1);
                                if (raw_line.length() > 0) {
                                    JsonDocument rawDoc;
                                    rawDoc["usbserial_data"] = raw_line;
                                    sendJsonToHost(rawDoc);
                                }
                            }
                        } else {
                            // Not robot joints, forward as regular serial data
                            JsonDocument doc;
                            doc["usbserial_data"] = usbserial_line_buffer;
                            sendJsonToHost(doc);
                        }
                        
                        usbserial_line_buffer = "";
                    }
                } else {
                    usbserial_line_buffer += c;
                }
            }
        }
    }
}

void periodicRequestSyncRobotPosition()
{
    // Only send position requests if USB Serial has been connected for at least 2 seconds
    uint32_t now = millis();
    if (usbserial_connected_ms == 0 || (now - usbserial_connected_ms) < 2000) {
        return; // Wait for 2 seconds after connection
    }

    // Send periodic command "P"
    static uint32_t last_cmd_ms = 0;
    if (now - last_cmd_ms >= 100) { // request position
        last_cmd_ms = now;
        if (userial.availableForWrite()) {
            userial.println("P");
            // Also inform PC host what we transmitted to USB serial
            {
                JsonDocument txDoc;
                txDoc["usbserial_tx"] = "P"; // command content without newline
                sendJsonToHost(txDoc);
            }
        }
    }
}

void usbHostLoop()
{
    myusb.Task();
    printDeviceListChanges();
    sendRawJoystickData();
    detectSendDominantAxis();

    // Read and forward USB Serial data to PC host
    if (userial) {
        processUsbSerialInput();
        periodicRequestSyncRobotPosition();
    }
}

