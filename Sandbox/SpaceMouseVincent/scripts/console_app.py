import serial
import json
import time

SERIAL_PORT = "COM30"        # adjust for your system!
BAUD = 115200

class TeensyClient:
    def __init__(self, port, baud):
        self.ser = serial.Serial(port, baud, timeout=1)
        time.sleep(2)  # give Teensy time to reboot after port open
        self.read_boot_message()

    def readline_json(self):
        line = self.ser.readline().decode('utf-8').strip()
        if not line:
            return None
        try:
            return json.loads(line)
        except json.JSONDecodeError:
            print("Bad JSON:", line)
            return None

    def send(self, obj):
        msg = json.dumps(obj)
        self.ser.write((msg + "\n").encode('utf-8'))

    def request(self, cmd_dict):
        self.send(cmd_dict)
        return self.readline_json()

    def read_boot_message(self):
        boot = self.readline_json()
        print("Boot:", boot)

    # Convenience wrappers
    def get(self, param):
        return self.request({"cmd": "get", "param": param})

    def set(self, param, value):
        return self.request({"cmd": "set", "param": param, "value": value})

    def save(self):
        return self.request({"cmd": "save"})

    def status(self):
        return self.request({"cmd": "status"})


if __name__ == "__main__":
    t = TeensyClient(SERIAL_PORT, BAUD)

    print("\n--- STATUS ---")
    print(t.status())

    print("\n--- GET max_speed ---")
    print(t.get("max_speed"))

    print("\n--- SET max_speed=2.5 ---")
    print(t.set("max_speed", 2.5))
    print(t.get("max_speed"))

    print("\n--- SAVE TO FLASH ---")
    print(t.save())

    print("\n--- STATUS AGAIN ---")
    print(t.status())
