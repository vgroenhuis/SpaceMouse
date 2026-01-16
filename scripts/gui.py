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
    connection_lost = pyqtSignal(str)  # emits error message when underlying serial disconnects

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
                # Notify main window; include error text
                self.connection_lost.emit(str(e))
                break

    def stop(self):
        self.running = False
        self.wait()


# --------------------------- Main GUI --------------------------------

class ElideLabel(QLabel):
    """QLabel that automatically elides text when space is constrained."""
    def __init__(self, text: str = "", parent=None, mode: Qt.TextElideMode = Qt.TextElideMode.ElideRight):
        super().__init__(text, parent)
        self._full_text = text
        self._elide_mode = mode

    def setText(self, text: str):
        self._full_text = str(text)
        self._update_elide()

    def resizeEvent(self, event):
        self._update_elide()
        return super().resizeEvent(event)

    def _update_elide(self):
        fm = self.fontMetrics()
        width = max(1, self.width())
        elided = fm.elidedText(self._full_text, self._elide_mode, width)
        super().setText(elided)

class MainWindow(QWidget):
    def __init__(self):
        super().__init__()

        self.ser = None
        self.reader_thread = None
        self.show_json = True
        self.show_non_json = False
        self.show_position_updates = False  # Default: hide position updates
        
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
        status_group = QGroupBox("Teensy Status")
        status_layout = QGridLayout()
        
        # Firmware + SpaceMouse + USB Serial + Teensy connection status
        self.fw_label = QLabel("Firmware: Unknown")
        self.spacemouse_status_label = QLabel("SpaceMouse: Disconnected")
        self.spacemouse_status_label.setStyleSheet("color: red; font-weight: bold;")
        self.usbserial_status_label = QLabel("USB Serial: Disconnected")
        self.usbserial_status_label.setStyleSheet("color: red; font-weight: bold;")
        self.teensy_status_label = QLabel("Teensy: Disconnected")
        self.teensy_status_label.setStyleSheet("color: red; font-weight: bold;")
        status_layout.addWidget(self.fw_label, 0, 0, 1, 2)
        status_layout.addWidget(self.teensy_status_label, 0, 2, 1, 2)
        status_layout.addWidget(self.spacemouse_status_label, 0, 4, 1, 2)
        status_layout.addWidget(self.usbserial_status_label, 0, 6, 1, 2)
        
        status_group.setLayout(status_layout)
        main_layout.addWidget(status_group)

        # --------------------------------------------------------------
        # SpaceMouse Data
        # --------------------------------------------------------------
        sm_group = QGroupBox("SpaceMouse Data")
        sm_layout = QHBoxLayout()
        # Single-line compact aggregated labels
        xyz_text_label = QLabel("X Y Z:")
        self.xyz_label = ElideLabel("[0, 0, 0]")
        self.xyz_label.setStyleSheet("font-family: Consolas;")
        rxyz_text_label = QLabel("  RX RY RZ:")
        self.rxyz_label = ElideLabel("[0, 0, 0]")
        self.rxyz_label.setStyleSheet("font-family: Consolas;")
        btn_text_label = QLabel("  Buttons:")
        self.btn_label = ElideLabel("NONE")
        self.btn_label.setStyleSheet("font-family: Consolas;")
        dominant_text_label = QLabel("  Dominant:")
        self.dominant_label = ElideLabel("NONE ")
        self.dominant_label.setStyleSheet("font-family: Consolas;")
        # Provide sensible minimum widths to avoid hiding typical content
        # while still allowing eliding when extremely narrow.
        fm = QFontMetrics(self.xyz_label.font())
        xyz_template = "[-100, -100, -100]"
        rxyz_template = "[-100, -100, -100]"
        btn_template = "LEFT+RIGHT"
        dominant_template = "NONE"
        pad = 10  # small padding for readability
        self.xyz_label.setMinimumWidth(fm.horizontalAdvance(xyz_template) + pad)
        self.rxyz_label.setMinimumWidth(fm.horizontalAdvance(rxyz_template) + pad)
        self.btn_label.setMinimumWidth(fm.horizontalAdvance(btn_template) + pad)
        self.dominant_label.setMinimumWidth(fm.horizontalAdvance(dominant_template) + pad)
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
        # Robot Data (moved from SpaceMouse Data)
        # --------------------------------------------------------------
        robot_group = QGroupBox("Robot Data")
        robot_layout = QHBoxLayout()
        joints_text_label = QLabel("Current:")
        self.robot_joints_label = ElideLabel("[0, 0, 0, 0, 0, 0, 0, 0]")
        self.robot_joints_label.setStyleSheet("font-family: Consolas;")
        # Set minimum width for large joint values (e.g., 100000)
        fm = QFontMetrics(self.robot_joints_label.font())
        joints_template = "[100000, 100000, 100000, 100000, 100000, 100000, 100000, 100000]"
        self.robot_joints_label.setMinimumWidth(fm.horizontalAdvance(joints_template) + 12)
        robot_layout.addWidget(joints_text_label)
        robot_layout.addWidget(self.robot_joints_label)
        # Add setpoint display
        setpoint_text_label = QLabel("  Setpoint:")
        self.robot_setpoint_label = ElideLabel("[0, 0, 0, 0, 0, 0, 0, 0]")
        self.robot_setpoint_label.setStyleSheet("font-family: Consolas;")
        # Set minimum width for large setpoint values
        self.robot_setpoint_label.setMinimumWidth(fm.horizontalAdvance(joints_template) + 12)
        robot_layout.addWidget(setpoint_text_label)
        robot_layout.addWidget(self.robot_setpoint_label)
        robot_layout.addStretch(1)
        robot_group.setLayout(robot_layout)
        main_layout.addWidget(robot_group)

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
        self.show_non_json_chk = QCheckBox("Show non-JSON")
        self.show_non_json_chk.setChecked(False)
        self.show_non_json_chk.toggled.connect(lambda v: setattr(self, "show_non_json", v))
        
        pc_teensy_layout.addWidget(self.pc_teensy_log)
        controls_row = QHBoxLayout()
        controls_row.addWidget(self.show_json_chk)
        controls_row.addWidget(self.show_non_json_chk)
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
        self.show_position_chk = QCheckBox("Show position updates")
        self.show_position_chk.setChecked(False)
        self.show_position_chk.toggled.connect(lambda v: setattr(self, "show_position_updates", v))
        
        teensy_serial_layout.addWidget(self.teensy_serial_log)
        teensy_controls_row = QHBoxLayout()
        teensy_controls_row.addWidget(self.show_position_chk)
        teensy_controls_row.addStretch(1)
        teensy_controls_row.addWidget(clear_teensy_btn)
        teensy_serial_layout.addLayout(teensy_controls_row)
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
            self.log_pc_teensy("[ERROR] No port selected", is_system=True)
            return

        try:
            self.ser = serial.Serial(port, 115200, timeout=0.1)
            self.log_pc_teensy(f"[PC] Connected to {port}", is_system=True)

            self.reader_thread = SerialReader(self.ser)
            self.reader_thread.received_text.connect(self.handle_text)
            self.reader_thread.received_json.connect(self.handle_json)
            self.reader_thread.connection_lost.connect(self.handle_connection_lost)
            self.reader_thread.start()

            # Teensy successfully opened
            self.teensy_status_label.setText("Teensy: Connected")
            self.teensy_status_label.setStyleSheet("color: green; font-weight: bold;")

            self.connect_btn.setEnabled(False)
            self.disconnect_btn.setEnabled(True)

        except Exception as e:
            self.log_pc_teensy(f"[ERROR] Connection failed: {e}", is_system=True)

    def disconnect_serial(self):
        if self.reader_thread:
            self.reader_thread.stop()

        if self.ser:
            try:
                self.ser.close()
            except:
                pass

        self.log_pc_teensy("[PC] Disconnected", is_system=True)
        # Teensy gone implies SpaceMouse and USB Serial are also disconnected
        self.teensy_status_label.setText("Teensy: Disconnected")
        self.teensy_status_label.setStyleSheet("color: red; font-weight: bold;")
        self.spacemouse_status_label.setText("SpaceMouse: Disconnected")
        self.spacemouse_status_label.setStyleSheet("color: red; font-weight: bold;")
        self.usbserial_status_label.setText("USB Serial: Disconnected")
        self.usbserial_status_label.setStyleSheet("color: red; font-weight: bold;")

        self.connect_btn.setEnabled(True)
        self.disconnect_btn.setEnabled(False)

    # ------------------------ Logging ------------------------

    def log_pc_teensy(self, msg, from_teensy=False, is_json=False, is_system=False):
        """Log PC-Teensy JSON communication
        
        Args:
            msg: Message text
            from_teensy: True if message is from Teensy (grey background, left-aligned)
            is_json: True if JSON message
            is_system: True for system status messages (white background, italic, left-aligned)
        """
        if is_json and not self.show_json:
            return

        # Only log non-JSON messages coming from the Teensy if explicitly enabled.
        if from_teensy and (not is_json) and (not is_system) and (not self.show_non_json):
            return
        
        cursor = self.pc_teensy_log.textCursor()
        cursor.movePosition(cursor.MoveOperation.End)
        
        # Add newline before if there's existing content
        if not self.pc_teensy_log.document().isEmpty():
            cursor.insertText('\n')
        
        # Create block format for alignment
        block_fmt = cursor.blockFormat()
        fmt = cursor.charFormat()
        
        if is_system:
            # System status messages: white background, italic, left-aligned
            block_fmt.setAlignment(Qt.AlignmentFlag.AlignLeft)
            cursor.setBlockFormat(block_fmt)
            
            fmt.setBackground(Qt.GlobalColor.white)
            fmt.setForeground(QColor(0, 0, 0))
            fmt.setFontItalic(True)
            cursor.setCharFormat(fmt)
            cursor.insertText(msg)
        elif from_teensy:
            # Messages from Teensy: light grey background, left-aligned, NOT italic
            block_fmt.setAlignment(Qt.AlignmentFlag.AlignLeft)
            cursor.setBlockFormat(block_fmt)
            
            fmt.setBackground(QColor(230, 230, 230))  # Very light grey
            if is_json:
                fmt.setForeground(QColor(0, 0, 255))  # Blue for JSON
            else:
                fmt.setForeground(QColor(0, 0, 0))  # Black for other text
            fmt.setFontItalic(False)  # Explicitly set to not italic
            cursor.setCharFormat(fmt)
            cursor.insertText(msg)
        else:
            # Messages to Teensy (outgoing): light green background, right-aligned, NOT italic
            block_fmt.setAlignment(Qt.AlignmentFlag.AlignRight)
            cursor.setBlockFormat(block_fmt)
            
            fmt.setBackground(QColor(200, 255, 200))  # Light green
            fmt.setForeground(QColor(0, 0, 0))  # Black text
            fmt.setFontItalic(False)  # Explicitly set to not italic
            cursor.setCharFormat(fmt)
            cursor.insertText(msg)
        
        # Reset format
        fmt = cursor.charFormat()
        fmt.setBackground(Qt.GlobalColor.white)
        fmt.setForeground(QColor(0, 0, 0))
        fmt.setFontItalic(False)
        cursor.setCharFormat(fmt)
        
        self.pc_teensy_log.setTextCursor(cursor)
        self.pc_teensy_log.ensureCursorVisible()

    def log_teensy_serial(self, msg, from_device=False, is_json=False):
        """Log Teensy ↔ USB device communication in the right log.
        from_device=True styles messages coming from the device (Mamri) with grey background.
        from_device=False styles outgoing messages (Teensy→Mamri) with green background and right-align.
        """
        cursor = self.teensy_serial_log.textCursor()
        cursor.movePosition(cursor.MoveOperation.End)
        
        if not self.teensy_serial_log.document().isEmpty():
            cursor.insertText('\n')
        
        # Create block format for alignment
        block_fmt = cursor.blockFormat()
        fmt = cursor.charFormat()
        
        if from_device:
            # Messages from device: light grey background, left-aligned
            block_fmt.setAlignment(Qt.AlignmentFlag.AlignLeft)
            cursor.setBlockFormat(block_fmt)
            
            fmt.setBackground(QColor(230, 230, 230))
            fmt.setForeground(QColor(0, 0, 0))
        else:
            # Outgoing messages (Teensy→device): light green background, right-aligned
            block_fmt.setAlignment(Qt.AlignmentFlag.AlignRight)
            cursor.setBlockFormat(block_fmt)
            
            fmt.setBackground(QColor(200, 255, 200))  # Light green
            fmt.setForeground(QColor(0, 0, 0))
        
        cursor.setCharFormat(fmt)
        cursor.insertText(msg)
        
        # Reset format
        fmt.setBackground(Qt.GlobalColor.white)
        fmt.setForeground(QColor(0, 0, 0))
        cursor.setCharFormat(fmt)
        
        self.teensy_serial_log.setTextCursor(cursor)
        self.teensy_serial_log.ensureCursorVisible()

    # ------------------------ Message Handling ------------------------

    def handle_text(self, line):
        """Handle all received text lines"""
        self.log_pc_teensy(f"< {line}", from_teensy=True)

    def handle_connection_lost(self, err):
        """Handle unexpected Teensy serial disconnect (e.g., cable unplug)."""
        self.log_pc_teensy(f"[ERROR] Connection lost: {err}", is_system=True)
        # Update all dependent device statuses
        self.teensy_status_label.setText("Teensy: Disconnected")
        self.teensy_status_label.setStyleSheet("color: red; font-weight: bold;")
        self.spacemouse_status_label.setText("SpaceMouse: Disconnected")
        self.spacemouse_status_label.setStyleSheet("color: red; font-weight: bold;")
        self.usbserial_status_label.setText("USB Serial: Disconnected")
        self.usbserial_status_label.setStyleSheet("color: red; font-weight: bold;")
        # Clean up serial state
        try:
            if self.ser:
                self.ser.close()
        except:
            pass
        self.ser = None
        self.connect_btn.setEnabled(True)
        self.disconnect_btn.setEnabled(False)

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
        
        # Handle USB Serial connection status
        if "usbserial_connected" in data:
            if data["usbserial_connected"]:
                self.usbserial_status_label.setText("USB Serial: Connected")
                self.usbserial_status_label.setStyleSheet("color: green; font-weight: bold;")
            else:
                self.usbserial_status_label.setText("USB Serial: Disconnected")
                self.usbserial_status_label.setStyleSheet("color: red; font-weight: bold;")

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
            xyz_text = f"[{xyz[0]}, {xyz[1]}, {xyz[2]}]"
            self.xyz_label.setText(xyz_text)
            self.xyz_label.setToolTip(xyz_text)
        if rxyz is not None:
            rxyz_text = f"[{rxyz[0]}, {rxyz[1]}, {rxyz[2]}]"
            self.rxyz_label.setText(rxyz_text)
            self.rxyz_label.setToolTip(rxyz_text)
        if "sm_buttons" in data:
            btn_text = self.format_buttons(data["sm_buttons"])
            self.btn_label.setText(btn_text)
            self.btn_label.setToolTip(btn_text)
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
        
        # Robot joints array (8 numbers)
        if "robot_joints" in data and isinstance(data["robot_joints"], list):
            rj = data["robot_joints"][:8]
            vals = []
            for v in rj:
                try:
                    vals.append(str(int(float(v))))
                except Exception:
                    vals.append(str(v))
            joints_text = "[" + ", ".join(vals) + "]"
            self.robot_joints_label.setText(joints_text)
            self.robot_joints_label.setToolTip(joints_text)

        # Robot setpoint array (8 numbers)
        if "robot_setpoint" in data and isinstance(data["robot_setpoint"], list):
            rs = data["robot_setpoint"][:8]
            vals = []
            for v in rs:
                try:
                    vals.append(str(int(float(v))))
                except Exception:
                    vals.append(str(v))
            setpoint_text = "[" + ", ".join(vals) + "]"
            self.robot_setpoint_label.setText(setpoint_text)
            self.robot_setpoint_label.setToolTip(setpoint_text)

        # Handle USB Serial data - route to right log window
        if "usbserial_data" in data:
            serial_data = data.get("usbserial_data", "")
            if serial_data:
                # Filter position updates if checkbox is unchecked
                # Position data looks like comma-separated numbers (8 values)
                is_position_data = "," in serial_data and all(c.isdigit() or c in ",.- " for c in serial_data.strip())
                if not is_position_data or self.show_position_updates:
                    self.log_teensy_serial(serial_data, from_device=True)
        # Handle USB Serial transmissions (from Teensy to device) for logging
        if "usbserial_tx" in data:
            tx = data.get("usbserial_tx", "")
            if tx:
                # Filter "P" command if checkbox is unchecked
                if tx.strip() != "P" or self.show_position_updates:
                    self.log_teensy_serial(f"> {tx}")
            

    def parse_device_status(self, line):
        """Deprecated: device-specific status UI removed"""
        return

    def send_json(self, obj):
        """Send JSON command to Teensy"""
        if not self.ser:
            self.log_pc_teensy("[ERROR] Not connected", is_system=True)
            return

        msg = json.dumps(obj) + "\n"
        self.ser.write(msg.encode("utf-8"))
        self.log_pc_teensy(f"> {msg.strip()}", from_teensy=False, is_json=True)

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
