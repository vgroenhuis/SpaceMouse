import sys
import os
import json
import serial
import serial.tools.list_ports
from PyQt6.QtCore import QThread, pyqtSignal, Qt
from PyQt6.QtWidgets import (
    QApplication, QWidget, QPushButton, QVBoxLayout, QHBoxLayout,
    QLabel, QTextEdit, QComboBox, QGroupBox, QGridLayout, QCheckBox
)
from PyQt6.QtGui import QFont, QColor, QIcon, QPixmap, QPainter, QPen, QBrush, QFontMetrics

# --------------------------- Icon Helpers ----------------------------

def get_spacemouse_icon() -> QIcon:
    """Return a SpaceMouse icon. Prefer file-based icon if present, otherwise generate a simple vector icon."""
    # Candidate file locations (drop a file named spacemouse.png or .ico in one of these)
    script_dir = os.path.dirname(os.path.abspath(__file__))
    project_root = os.path.dirname(os.path.dirname(script_dir))
    candidates = [
        os.path.join(script_dir, "assets", "spacemouse.ico"),
        os.path.join(script_dir, "assets", "spacemouse.png"),
    ]
    for path in candidates:
        if os.path.exists(path):
            icon = QIcon(path)
            if not icon.isNull():
                return icon

    # Fallback: generate a simple stylized SpaceMouse icon
    return QIcon(generate_spacemouse_pixmap())


def generate_spacemouse_pixmap(size: int = 64) -> QPixmap:
    """Generate a simple stylized SpaceMouse-like icon as a fallback."""
    pm = QPixmap(size, size)
    pm.fill(Qt.GlobalColor.transparent)
    p = QPainter(pm)
    p.setRenderHint(QPainter.RenderHint.Antialiasing, True)

    # Base disk
    center = size / 2
    radius = size * 0.42
    base_color = QColor(60, 60, 70)
    rim_color = QColor(30, 30, 35)
    p.setBrush(QBrush(base_color))
    p.setPen(QPen(rim_color, max(1, size * 0.03)))
    p.drawEllipse(int(center - radius), int(center - radius), int(radius * 2), int(radius * 2))

    # Cap (top)
    cap_r = radius * 0.6
    cap_color = QColor(90, 160, 220)
    p.setBrush(QBrush(cap_color))
    p.setPen(QPen(QColor(20, 30, 40), max(1, size * 0.02)))
    p.drawEllipse(int(center - cap_r), int(center - cap_r), int(cap_r * 2), int(cap_r * 2))

    # Accent highlight
    p.setPen(Qt.PenStyle.NoPen)
    highlight = QColor(255, 255, 255, 70)
    p.setBrush(QBrush(highlight))
    p.drawEllipse(int(center - cap_r*0.7), int(center - cap_r*0.9), int(cap_r*1.1), int(cap_r*0.9))

    p.end()
    return pm


# ------------------------ Serial Reader Thread ------------------------

class SerialReader(QThread):
    received_json = pyqtSignal(dict)
    received_text = pyqtSignal(str)

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

                # Try to parse as JSON
                try:
                    data = json.loads(line)
                    self.received_json.emit(data)
                except json.JSONDecodeError:
                    # Not JSON, emit as text
                    self.received_text.emit(line)

            except Exception as e:
                self.received_text.emit(f"[ERROR] Reader exception: {e}")
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
        self.show_json = True
        
        self.fw_version = "Unknown"

        self.setWindowTitle("SpaceMouse Control Panel")
        # Set window/application icon
        icon = get_spacemouse_icon()
        self.setWindowIcon(icon)
        app = QApplication.instance()
        if app:
            app.setWindowIcon(icon)
        self.init_ui()

    def init_ui(self):
        main_layout = QVBoxLayout()

        # --------------------------------------------------------------
        # Serial Port Controls
        # --------------------------------------------------------------
        port_group = QGroupBox("Serial Connection")
        port_layout = QHBoxLayout()
        
        self.port_combo = QComboBox()
        teensy_count = self.refresh_ports()

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
        
        port_group.setLayout(port_layout)
        main_layout.addWidget(port_group)

        # --------------------------------------------------------------
        # Device Status Display
        # --------------------------------------------------------------
        status_group = QGroupBox("Device Status")
        status_layout = QGridLayout()
        
        # Firmware + SpaceMouse status
        self.fw_label = QLabel("Firmware: Unknown")
        self.spacemouse_status_label = QLabel("SpaceMouse: Disconnected")
        self.spacemouse_status_label.setStyleSheet("color: red; font-weight: bold;")
        status_layout.addWidget(self.fw_label, 0, 0, 1, 2)
        status_layout.addWidget(self.spacemouse_status_label, 0, 2, 1, 2)
        
        status_group.setLayout(status_layout)
        main_layout.addWidget(status_group)

        # --------------------------------------------------------------
        # SpaceMouse Data
        # --------------------------------------------------------------
        sm_group = QGroupBox("SpaceMouse Data")
        sm_layout = QHBoxLayout()
        # Single-line compact aggregated labels
        xyz_text_label = QLabel("X Y Z:")
        self.xyz_label = QLabel("[0, 0, 0]")
        self.xyz_label.setStyleSheet("font-family: Consolas;")
        # Fix width to prevent shifting of subsequent groups when values grow
        fm_xyz = QFontMetrics(self.xyz_label.font())
        xyz_template = "[-32768, -32768, -32768]"  # Worst-case width template
        self.xyz_label.setFixedWidth(fm_xyz.horizontalAdvance(xyz_template) + 8)
        rxyz_text_label = QLabel("  RX RY RZ:")
        self.rxyz_label = QLabel("[0, 0, 0]")
        self.rxyz_label.setStyleSheet("font-family: Consolas;")
        fm_rxyz = QFontMetrics(self.rxyz_label.font())
        rxyz_template = "[-32768, -32768, -32768]"
        self.rxyz_label.setFixedWidth(fm_rxyz.horizontalAdvance(rxyz_template) + 8)
        btn_text_label = QLabel("  Buttons:")
        self.btn_label = QLabel("")
        self.btn_label.setStyleSheet("font-family: Consolas;")
        fm_btn = QFontMetrics(self.btn_label.font())
        btn_template = "LEFT+RIGHT"  # Longest expected buttons text (10 chars)
        self.btn_label.setFixedWidth(fm_btn.horizontalAdvance(btn_template) + 8)
        # Dominant axis/direction label
        dominant_text_label = QLabel("  Dominant:")
        self.dominant_label = QLabel("NONE")
        self.dominant_label.setStyleSheet("font-family: Consolas;")
        fm_dominant = QFontMetrics(self.dominant_label.font())
        # Template covers longest expected text: "RX -" (axis + space + sign)
        dominant_template = "RX -"
        self.dominant_label.setFixedWidth(fm_dominant.horizontalAdvance(dominant_template) + 8)
        sm_layout.addWidget(xyz_text_label)
        sm_layout.addWidget(self.xyz_label)
        sm_layout.addWidget(rxyz_text_label)
        sm_layout.addWidget(self.rxyz_label)
        sm_layout.addWidget(btn_text_label)
        sm_layout.addWidget(self.btn_label)
        sm_layout.addWidget(dominant_text_label)
        sm_layout.addWidget(self.dominant_label)
        sm_layout.addStretch(1)
        sm_group.setLayout(sm_layout)
        main_layout.addWidget(sm_group)

        # --------------------------------------------------------------
        # Logs Section
        # --------------------------------------------------------------
        logs_layout = QHBoxLayout()
        
        # PC-Teensy Communication Log
        pc_teensy_group = QGroupBox("PC ↔ Teensy Communication (JSON)")
        pc_teensy_layout = QVBoxLayout()
        self.pc_teensy_log = QTextEdit()
        self.pc_teensy_log.setReadOnly(True)
        self.pc_teensy_log.setAcceptRichText(True)
        font = QFont("Consolas", 9)
        self.pc_teensy_log.setFont(font)
        self.show_json = False
        clear_pc_btn = QPushButton("Clear")
        clear_pc_btn.clicked.connect(lambda: self.pc_teensy_log.clear())
        self.show_json_chk = QCheckBox("Show JSON")
        self.show_json_chk.setChecked(False)
        self.show_json_chk.toggled.connect(lambda v: setattr(self, "show_json", v))
        
        pc_teensy_layout.addWidget(self.pc_teensy_log)
        controls_row = QHBoxLayout()
        controls_row.addWidget(self.show_json_chk)
        controls_row.addStretch(1)
        controls_row.addWidget(clear_pc_btn)
        pc_teensy_layout.addLayout(controls_row)
        pc_teensy_group.setLayout(pc_teensy_layout)
        
        # Teensy-Serial Communication Log (device messages)
        teensy_serial_group = QGroupBox("Teensy → USB Devices Log")
        teensy_serial_layout = QVBoxLayout()
        self.teensy_serial_log = QTextEdit()
        self.teensy_serial_log.setReadOnly(True)
        self.teensy_serial_log.setFont(font)
        
        clear_teensy_btn = QPushButton("Clear")
        clear_teensy_btn.clicked.connect(lambda: self.teensy_serial_log.clear())
        
        teensy_serial_layout.addWidget(self.teensy_serial_log)
        teensy_serial_layout.addWidget(clear_teensy_btn)
        teensy_serial_group.setLayout(teensy_serial_layout)
        
        logs_layout.addWidget(pc_teensy_group)
        logs_layout.addWidget(teensy_serial_group)
        
        main_layout.addLayout(logs_layout)

        self.setLayout(main_layout)
        
        # Auto-connect if exactly one Teensy detected
        if teensy_count == 1:
            self.connect_serial()

    # ------------------------ Serial Handling ------------------------

    def refresh_ports(self):
        self.port_combo.clear()
        ports = serial.tools.list_ports.comports()
        teensy_index = -1
        teensy_count = 0
        for i, p in enumerate(ports):
            # Check if it's a Teensy device (VID: 0x16C0)
            if p.vid == 0x16C0:
                self.port_combo.addItem(f"{p.device} (Teensy)", p.device)
                if teensy_index == -1:
                    teensy_index = i
                teensy_count += 1
            else:
                self.port_combo.addItem(p.device, p.device)
        
        # Auto-select first Teensy if found
        if teensy_index >= 0:
            self.port_combo.setCurrentIndex(teensy_index)
        
        # Return teensy count for auto-connect logic
        return teensy_count

    def connect_serial(self):
        port = self.port_combo.currentData()
        if not port:
            self.log_pc_teensy("[ERROR] No port selected")
            return

        try:
            self.ser = serial.Serial(port, 115200, timeout=0.1)
            self.log_pc_teensy(f"[PC] Connected to {port}")

            self.reader_thread = SerialReader(self.ser)
            self.reader_thread.received_text.connect(self.handle_text)
            self.reader_thread.received_json.connect(self.handle_json)
            self.reader_thread.start()

            self.connect_btn.setEnabled(False)
            self.disconnect_btn.setEnabled(True)

        except Exception as e:
            self.log_pc_teensy(f"[ERROR] Connection failed: {e}")

    def disconnect_serial(self):
        if self.reader_thread:
            self.reader_thread.stop()

        if self.ser:
            try:
                self.ser.close()
            except:
                pass

        self.log_pc_teensy("[PC] Disconnected")

        self.connect_btn.setEnabled(True)
        self.disconnect_btn.setEnabled(False)

    # ------------------------ Logging ------------------------

    def log_pc_teensy(self, msg, from_teensy=False, is_json=False):
        """Log PC-Teensy JSON communication"""
        if is_json and not self.show_json:
            return
        if from_teensy:
            # Add light grey background for messages from Teensy
            cursor = self.pc_teensy_log.textCursor()
            cursor.movePosition(cursor.MoveOperation.End)
            
            # Add newline before if there's existing content
            if not self.pc_teensy_log.document().isEmpty():
                cursor.insertText('\n')
            
            # Insert with background color and optional blue text for JSON
            fmt = cursor.charFormat()
            fmt.setBackground(QColor(230, 230, 230))  # Very light grey
            if is_json:
                fmt.setForeground(QColor(0, 0, 255))  # Blue for JSON
            else:
                fmt.setForeground(QColor(0, 0, 0))  # Black for other text
            cursor.setCharFormat(fmt)
            cursor.insertText(msg)
            
            # Reset format
            fmt.setBackground(Qt.GlobalColor.white)
            fmt.setForeground(QColor(0, 0, 0))
            cursor.setCharFormat(fmt)
            
            self.pc_teensy_log.setTextCursor(cursor)
            self.pc_teensy_log.ensureCursorVisible()
        else:
            self.pc_teensy_log.append(msg)

    def log_teensy_serial(self, msg):
        """Log Teensy-USB device communication"""
        self.teensy_serial_log.append(msg)

    # ------------------------ Message Handling ------------------------

    def handle_text(self, line):
        """Handle all received text lines"""
        self.log_pc_teensy(f"< {line}", from_teensy=True)

    def handle_json(self, data):
        """Handle JSON messages"""
        # Log the JSON with blue color
        self.log_pc_teensy(f"< {json.dumps(data)}", from_teensy=True, is_json=True)
        
        # Parse firmware info
        if "fw_ver" in data:
            self.fw_version = data["fw_ver"]
            self.fw_label.setText(f"Firmware: {self.fw_version}")
        
        # Protocol removed from UI
        
        # Handle SpaceMouse connection status
        if "spacemouse_connected" in data:
            if data["spacemouse_connected"]:
                self.spacemouse_status_label.setText("SpaceMouse: Connected")
                self.spacemouse_status_label.setStyleSheet("color: green; font-weight: bold;")
            else:
                self.spacemouse_status_label.setText("SpaceMouse: Disconnected")
                self.spacemouse_status_label.setStyleSheet("color: red; font-weight: bold;")

        # Handle SpaceMouse axes/buttons
        # Preferred: arrays sm_xyz:[x,y,z] and sm_rxyz:[rx,ry,rz]
        xyz = None
        rxyz = None
        if "sm_xyz" in data and isinstance(data["sm_xyz"], list):
            xyz = data["sm_xyz"][:3]
        if "sm_rxyz" in data and isinstance(data["sm_rxyz"], list):
            rxyz = data["sm_rxyz"][:3]

        # Legacy fallbacks
        if xyz is None or rxyz is None:
            if "sm_axes" in data and isinstance(data["sm_axes"], list):
                arr = data["sm_axes"]
                if xyz is None and len(arr) >= 3:
                    xyz = arr[0:3]
                if rxyz is None and len(arr) >= 6:
                    rxyz = arr[3:6]
            else:
                # Individual keys fallback
                vals = []
                for i in range(6):
                    key = f"sm_ax{i}"
                    vals.append(data.get(key))
                if xyz is None and None not in vals[0:3]:
                    xyz = vals[0:3]
                if rxyz is None and None not in vals[3:6]:
                    rxyz = vals[3:6]

        if xyz is not None:
            self.xyz_label.setText(f"[{xyz[0]}, {xyz[1]}, {xyz[2]}]")
        if rxyz is not None:
            self.rxyz_label.setText(f"[{rxyz[0]}, {rxyz[1]}, {rxyz[2]}]")
        if "sm_buttons" in data:
            self.btn_label.setText(self.format_buttons(data["sm_buttons"]))
        # Dominant axis/direction (new keys) with legacy fallback
        axis = None
        direction = None
        if "dominant_axis" in data and "dominant_direction" in data:
            axis = data.get("dominant_axis")
            direction = data.get("dominant_direction")
        if axis is not None and direction is not None:
            # direction may be int (1, -1, 0) or legacy string
            sign = ""  # default empty
            if isinstance(direction, int):
                if direction == 1:
                    sign = "+"
                elif direction == -1:
                    sign = "-"
            if axis == "none" or axis == "NONE" or sign == "":
                self.dominant_label.setText("NONE")
            else:
                self.dominant_label.setText(f"{axis}{sign}")
            

    def parse_device_status(self, line):
        """Deprecated: device-specific status UI removed"""
        return

    def send_json(self, obj):
        """Send JSON command to Teensy"""
        if not self.ser:
            self.log_pc_teensy("[ERROR] Not connected")
            return

        msg = json.dumps(obj) + "\n"
        self.ser.write(msg.encode("utf-8"))
        self.log_pc_teensy(f"> {msg.strip()}", is_json=True)

    # ------------------------ Helpers ------------------------

    def format_buttons(self, value):
        """Map SpaceMouse button numeric value to readable text.
        0: NONE, 1: LEFT, 2: RIGHT, 3: LEFT+RIGHT, other: raw value.
        """
        mapping = {0: "NONE", 1: "LEFT", 2: "RIGHT", 3: "LEFT+RIGHT"}
        return mapping.get(value, str(value))


# --------------------------- Entry Point ------------------------------

if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.resize(900, 700)
    window.show()
    sys.exit(app.exec())
