import sys
import json
import serial
import serial.tools.list_ports
from PyQt6.QtCore import QThread, pyqtSignal
from PyQt6.QtWidgets import (
    QApplication, QWidget, QPushButton, QVBoxLayout, QHBoxLayout,
    QLabel, QLineEdit, QTextEdit, QComboBox, QSpinBox, QDoubleSpinBox,
    QCheckBox
)


# ------------------------ Serial Reader Thread ------------------------

class SerialReader(QThread):
    received = pyqtSignal(dict)
    text = pyqtSignal(str)

    def __init__(self, ser):
        super().__init__()
        self.ser = ser
        self.running = True

    def run(self):
        while self.running:
            try:
                line = self.ser.readline().decode("utf-8").strip()
                if not line:
                    continue

                self.text.emit(f"< {line}")

                try:
                    data = json.loads(line)
                    self.received.emit(data)
                except json.JSONDecodeError:
                    pass

            except Exception as e:
                self.text.emit(f"Error: {e}")
                break

    def stop(self):
        self.running = False
        self.wait()


# --------------------------- Main GUI --------------------------------

class MainWindow(QWidget):
    def __init__(self):
        super().__init__()

        self.ser = None
        self.reader_thread = None

        self.setWindowTitle("Teensy Control Panel")

        layout = QVBoxLayout()

        # --------------------------------------------------------------
        # Serial Port Controls
        # --------------------------------------------------------------

        port_layout = QHBoxLayout()
        self.port_combo = QComboBox()
        self.refresh_ports()

        self.refresh_btn = QPushButton("Refresh")
        self.refresh_btn.clicked.connect(self.refresh_ports)

        self.connect_btn = QPushButton("Connect")
        self.connect_btn.clicked.connect(self.connect_serial)

        self.disconnect_btn = QPushButton("Disconnect")
        self.disconnect_btn.clicked.connect(self.disconnect_serial)
        self.disconnect_btn.setEnabled(False)

        port_layout.addWidget(QLabel("Port:"))
        port_layout.addWidget(self.port_combo)
        port_layout.addWidget(self.refresh_btn)
        port_layout.addWidget(self.connect_btn)
        port_layout.addWidget(self.disconnect_btn)

        layout.addLayout(port_layout)

        # --------------------------------------------------------------
        # Configuration Controls
        # --------------------------------------------------------------

        cfg_layout = QHBoxLayout()
        
        self.max_speed = QDoubleSpinBox()
        self.max_speed.setRange(0.0, 999.0)
        self.max_speed.setSingleStep(0.1)

        self.invert_joystick = QCheckBox("Invert Joystick")

        self.mode = QSpinBox()
        self.mode.setRange(0, 10)

        cfg_layout.addWidget(QLabel("Max Speed:"))
        cfg_layout.addWidget(self.max_speed)
        cfg_layout.addWidget(self.invert_joystick)
        cfg_layout.addWidget(QLabel("Mode:"))
        cfg_layout.addWidget(self.mode)

        layout.addLayout(cfg_layout)

        # --------------------------------------------------------------
        # Buttons
        # --------------------------------------------------------------

        btns = QHBoxLayout()

        self.get_status_btn = QPushButton("Get Status")
        self.get_status_btn.clicked.connect(lambda: self.send_json({"cmd": "status"}))

        self.load_btn = QPushButton("Load Settings")
        self.load_btn.clicked.connect(lambda: self.send_json({"cmd": "load"}))

        self.save_btn = QPushButton("Save Settings")
        self.save_btn.clicked.connect(lambda: self.send_json({"cmd": "save"}))

        self.set_btn = QPushButton("Set Values")
        self.set_btn.clicked.connect(self.send_all_settings)

        self.list_btn = QPushButton("List Files")
        self.list_btn.clicked.connect(lambda: self.send_json({"cmd": "list"}))

        self.print_btn = QPushButton("Print Config")
        self.print_btn.clicked.connect(lambda: self.send_json({"cmd": "print"}))

        btns.addWidget(self.get_status_btn)
        btns.addWidget(self.load_btn)
        btns.addWidget(self.set_btn)
        btns.addWidget(self.save_btn)
        btns.addWidget(self.list_btn)
        btns.addWidget(self.print_btn)

        layout.addLayout(btns)

        # --------------------------------------------------------------
        # Log window
        # --------------------------------------------------------------

        self.log = QTextEdit()
        self.log.setReadOnly(True)
        layout.addWidget(self.log)

        self.setLayout(layout)

    # ------------------------ Serial Handling ------------------------

    def refresh_ports(self):
        self.port_combo.clear()
        ports = serial.tools.list_ports.comports()
        for p in ports:
            self.port_combo.addItem(p.device)

    def connect_serial(self):
        port = self.port_combo.currentText()
        if not port:
            self.log.append("No port selected")
            return

        try:
            self.ser = serial.Serial(port, 115200, timeout=0.1)
            self.log.append(f"Connected to {port}")

            self.reader_thread = SerialReader(self.ser)
            self.reader_thread.text.connect(self.log.append)
            self.reader_thread.received.connect(self.handle_json)
            self.reader_thread.start()

            self.connect_btn.setEnabled(False)
            self.disconnect_btn.setEnabled(True)

        except Exception as e:
            self.log.append(f"Connection failed: {e}")

    def disconnect_serial(self):
        # self.log.append("Disconnecting...")

        if self.reader_thread:
            # self.log.append("Stopping reader thread...")
            self.reader_thread.stop()

        if self.ser:
            try:
                # self.log.append("Closing serial...")
                self.ser.close()
            except:
                pass

        self.log.append("Disconnected")

        self.connect_btn.setEnabled(True)
        self.disconnect_btn.setEnabled(False)

    # ------------------------ JSON Send/Receive ------------------------

    def send_json(self, obj):
        if not self.ser:
            self.log.append("Not connected")
            return

        msg = json.dumps(obj) + "\n"
        self.ser.write(msg.encode("utf-8"))
        self.log.append(f"> {msg.strip()}")

    def handle_json(self, data):
        # Populate UI if it's a status response
        if data.get("result") == "ok" and "max_speed" in data:
            self.max_speed.setValue(data["max_speed"])
            self.invert_joystick.setChecked(data["invert_joystick"])
            self.mode.setValue(data["mode"])

    def send_all_settings(self):
        self.send_json({"cmd": "set", "param": "max_speed", "value": self.max_speed.value()})
        self.send_json({"cmd": "set", "param": "invert_joystick", "value": self.invert_joystick.isChecked()})
        self.send_json({"cmd": "set", "param": "mode", "value": self.mode.value()})


# --------------------------- Entry Point ------------------------------

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.resize(600, 400)
    window.show()
    sys.exit(app.exec())
