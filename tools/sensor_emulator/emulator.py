"""
DVU-01 Sensor Emulator — WindTrans project
Эмулирует ответы датчика ДВУ-01 на команды от прибора WindTrans.

Requirements: PySide6, pyserial
"""

import sys
import threading
import queue
import time
import random
import datetime

import serial
import serial.tools.list_ports

from PySide6.QtCore    import Qt, QTimer, Signal, QObject
from PySide6.QtGui     import QFont, QColor, QTextCharFormat, QTextCursor, QClipboard
from PySide6.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QComboBox, QPushButton, QSlider, QLineEdit, QCheckBox,
    QTextEdit, QGroupBox, QSizePolicy, QTabWidget, QFrame, QScrollArea,
)


# ============================================================
#  Theme colours  (used only for the content area, not the title bar)
# ============================================================
BG       = "#1e1e2e"
PANEL    = "#2a2a3e"
ENTRY_BG = "#313145"
ACCENT   = "#7c3aed"
ACCENT2  = "#06b6d4"
FG       = "#e2e8f0"
FG_DIM   = "#94a3b8"
GREEN    = "#22c55e"
RED      = "#ef4444"
CON_BG   = "#0d0d1a"
CON_FG   = "#a5f3fc"
CON_CMD  = "#fbbf24"
CON_ERR  = "#f87171"
CON_INFO = "#64748b"

# ── Shared stylesheet applied to the central widget only ──────────────────────
CONTENT_QSS = f"""
QWidget {{
    background-color: {BG};
    color: {FG};
    font-family: "Segoe UI";
    font-size: 10pt;
}}
QGroupBox {{
    background-color: {PANEL};
    color: {FG_DIM};
    border: 1px solid #3f3f5e;
    border-radius: 6px;
    margin-top: 14px;
    font-size: 9pt;
    font-weight: bold;
}}
QGroupBox::title {{
    subcontrol-origin: margin;
    left: 10px;
    padding: 0 4px;
}}
QComboBox {{
    background-color: {ENTRY_BG};
    color: {FG};
    border: 1px solid #3f3f5e;
    border-radius: 4px;
    padding: 3px 8px;
}}
QComboBox::drop-down {{ border: none; }}
QComboBox QAbstractItemView {{
    background: {ENTRY_BG};
    color: {FG};
    selection-background-color: {ACCENT};
}}
QLineEdit {{
    background: {ENTRY_BG};
    color: {FG};
    border: 1px solid #3f3f5e;
    border-radius: 4px;
    padding: 3px 6px;
}}
QLineEdit:focus {{ border-color: {ACCENT2}; }}
QSlider::groove:horizontal {{
    height: 6px;
    background: {ENTRY_BG};
    border-radius: 3px;
}}
QSlider::handle:horizontal {{
    background: {ACCENT};
    width: 14px;
    height: 14px;
    margin: -4px 0;
    border-radius: 7px;
}}
QSlider::sub-page:horizontal {{
    background: {ACCENT};
    border-radius: 3px;
}}
QCheckBox {{ color: {FG}; }}
QCheckBox::indicator {{ width: 15px; height: 15px; }}
QTabWidget::pane {{
    border: 1px solid #3f3f5e;
    border-radius: 4px;
    background: {BG};
}}
QTabBar::tab {{
    background: {PANEL};
    color: {FG_DIM};
    border: 1px solid #3f3f5e;
    border-bottom: none;
    border-radius: 4px 4px 0 0;
    padding: 6px 18px;
    margin-right: 2px;
}}
QTabBar::tab:selected {{
    background: {BG};
    color: {FG};
    border-bottom: 2px solid {ACCENT2};
}}
QTabBar::tab:hover:!selected {{ color: {FG}; }}
QScrollArea {{ border: none; background: transparent; }}
"""

BTN_PRIMARY   = f"QPushButton {{ background:{GREEN};  color:#fff; border:none; border-radius:6px; font-size:11pt; font-weight:bold; padding:0 20px; }} QPushButton:hover {{ background:#16a34a; }} QPushButton:disabled {{ background:#374151; color:#6b7280; }}"
BTN_DANGER    = f"QPushButton {{ background:{RED};    color:#fff; border:none; border-radius:6px; font-size:11pt; font-weight:bold; padding:0 20px; }} QPushButton:hover {{ background:#b91c1c; }} QPushButton:disabled {{ background:#374151; color:#6b7280; }}"
BTN_SECONDARY = f"QPushButton {{ background:{PANEL};  color:{FG_DIM}; border:1px solid #3f3f5e; border-radius:6px; font-size:10pt; font-weight:bold; padding:0 16px; }} QPushButton:hover {{ background:{ENTRY_BG}; color:{FG}; }}"
BTN_COPY      = f"QPushButton {{ background:{ENTRY_BG}; color:{ACCENT2}; border:1px solid #3f3f5e; border-radius:4px; font-size:9pt; padding:2px 10px; }} QPushButton:hover {{ background:#1e2a3a; }}"


# ============================================================
#  Sensor emulator logic
# ============================================================
class SensorEmulator:
    FIRMWARE_VER = "1.0.0"
    DEVICE_NAME  = "DVU-01"

    def __init__(self):
        self.speed        = 5.0
        self.direction    = 180.0
        self.add_noise    = True
        self.session_open = False
        # AMES auto-send state
        self.ames_active   = False
        self.ames_count    = 0    # 0 = infinite
        self.ames_interval = 1000 # ms
        self.ames_sent     = 0    # packets sent so far
        self._ames_cb      = None # callback: fn(bytes) -> None, set by worker

    def _noise(self, val, amp):
        return val + random.gauss(0, amp) if self.add_noise else val

    def _make_ames_packet(self):
        """Generate a realistic AMES packet matching real DVU-01 output:
        Spd=X.XX Dir=XXX.X vX=X.XX vY=X.XX sq=100 dr1=0 dr2=0 dr3=0 dr4=0
        tm1=XXXXX tm2=XXXXX tm3=XXXXX tm4=XXXXX sndx=XXX.X sndy=XXX.X
        """
        spd = max(0.0, self._noise(self.speed, 0.05))
        d   = self._noise(self.direction, 0.5) % 360
        # vX / vY components
        import math
        rad = math.radians(d)
        vx  = round(spd * math.sin(rad) + (random.gauss(0, 0.01) if self.add_noise else 0), 2)
        vy  = round(spd * math.cos(rad) + (random.gauss(0, 0.01) if self.add_noise else 0), 2)
        # Simulated timer counts (realistic range ~16000 ticks)
        base = 16100 + int(random.gauss(0, 50)) if self.add_noise else 16100
        dt   = max(0, int(spd * 20))  # higher speed -> larger time difference
        tm1 = base - dt
        tm2 = base + dt + random.randint(-5, 5)
        tm3 = base + dt + random.randint(20, 50)
        tm4 = base - dt + random.randint(-5, 5)
        sndx = round(346.0 + random.gauss(0, 0.3 if self.add_noise else 0), 1)
        sndy = round(346.0 + random.gauss(0, 0.3 if self.add_noise else 0), 1)
        sq   = 100
        line = (f"Spd={spd:.2f} Dir={d:.1f} vX={vx:.2f} vY={vy:.2f} sq={sq} "
                f"dr1=0 dr2=0 dr3=0 dr4=0 "
                f"tm1={tm1} tm2={tm2} tm3={tm3} tm4={tm4} "
                f"sndx={sndx} sndy={sndy}")
        return (line + "\r\n").encode()

    def _make_mes_full(self, addr_prefix):
        """10-field response for network command @N M.
        Format: @addr:0 status spd_act spd_avg spd_max spd_min 0 dir_act dir_avg dir_max dir_min
        """
        spd     = max(0.0, self._noise(self.speed,     0.05))
        spd_avg = max(0.0, self._noise(self.speed,     0.10))
        spd_max = max(spd, max(0.0, self._noise(self.speed, 0.20)))
        spd_min = min(spd, max(0.0, self._noise(self.speed, 0.20)))
        d       = self._noise(self.direction, 0.5)  % 360
        d_avg   = self._noise(self.direction, 1.0)  % 360
        d_max   = max(d, self._noise(self.direction, 2.0)) % 360
        d_min   = min(d, self._noise(self.direction, 2.0)) % 360
        line = (f"@{addr_prefix}:0 0 "
                f"{spd:.2f} {spd_avg:.2f} {spd_max:.2f} {spd_min:.2f} 0 "
                f"{d:.0f} {d_avg:.0f} {d_max:.0f} {d_min:.0f}")
        return (line + "\r\n").encode()

    def _make_mes_short(self, msg_id):
        """3-field response for local command 'M <id>'.
        Format: id speed direction
        Matches the 3-field sscanf branch in sensor.c.
        """
        spd = max(0.0, self._noise(self.speed, 0.05))
        d   = self._noise(self.direction, 0.5) % 360
        return (f"{msg_id} {spd:.2f} {d:.0f}\r\n").encode()

    def process(self, raw: bytes):
        """Returns (response_bytes_or_None, log_string)."""
        try:
            line = raw.decode("ascii", errors="replace").strip()
        except Exception:
            return None, f"[parse error] {raw!r}"
        if not line:
            return None, ""

        u = line.upper()

        # ── @N AMES count interval ─────────────────────────────────────────────
        # Start:  @1 AMES 0 1000   (0=infinite, 1000ms interval)
        # Stop:   @1 AMES 0 0
        if u.startswith("@") and "AMES" in u:
            parts = line.split()
            # Extract count and interval (3rd and 4th args after @N AMES)
            try:
                count    = int(parts[2]) if len(parts) > 2 else 0
                interval = int(parts[3]) if len(parts) > 3 else 0
            except (ValueError, IndexError):
                count, interval = 0, 0

            if interval == 0:
                # Stop auto-send
                self.ames_active = False
                return None, f"[CMD] {line!r}  → AMES stopped"
            else:
                # Start auto-send
                self.ames_active   = True
                self.ames_count    = count
                self.ames_interval = interval
                self.ames_sent     = 0
                return None, f"[CMD] {line!r}  → AMES started (interval={interval}ms, count={count or '∞'})"

        if u.startswith("OPEN"):
            self.session_open = True
            resp = f"1, {self.FIRMWARE_VER}, {self.DEVICE_NAME} SESSION OPENED >\r\n"
            return resp.encode(), f"[CMD] {line!r}  → {resp.strip()!r}"

        if u == "VER" or u.startswith("VER "):
            resp = f"{self.DEVICE_NAME} ver {self.FIRMWARE_VER}\r\n"
            return resp.encode(), f"[CMD] {line!r}  → {resp.strip()!r}"

        # Local poll: M <id>  → 3 fields
        if u.startswith("M ") or u == "M":
            parts = line.split()
            msg_id = parts[1] if len(parts) > 1 else "0"
            rb = self._make_mes_short(msg_id)
            return rb, f"[CMD] {line!r}  → {rb.decode().strip()!r}"

        # Network poll: @N M  → 10 fields
        if u.startswith("@") and ("MES" in u or " M" in u or u.endswith("M")):
            parts = line.split()
            addr  = parts[0][1:]
            rb = self._make_mes_full(addr_prefix=addr)
            return rb, f"[CMD] {line!r}  → {rb.decode().strip()!r}"

        if u == "CLOSE":
            self.session_open = False
            return None, f"[CMD] {line!r}  → (session closed)"

        resp = b"UNKNOWN COMMAND\r\n"
        return resp, f"[CMD] {line!r}  → UNKNOWN COMMAND"


# ============================================================
#  Serial worker thread
# ============================================================
class WorkerSignals(QObject):
    done = Signal()


class SerialWorker(threading.Thread):
    def __init__(self, port, baud, emulator, log_q):
        super().__init__(daemon=True)
        self.port     = port
        self.baud     = baud
        self.emulator = emulator
        self.log_q    = log_q
        self._stop    = threading.Event()
        self.signals  = WorkerSignals()

    def stop(self):
        self._stop.set()

    def run(self):
        try:
            ser = serial.Serial(self.port, self.baud, timeout=0.05)
            self._log(f"[SERIAL] Opened {self.port} @ {self.baud} bps", "info")
        except Exception as e:
            self._log(f"[ERROR] Cannot open {self.port}: {e}", "err")
            self.signals.done.emit()
            return

        buf = b""
        last_ames_time = 0.0  # monotonic time of last AMES packet sent

        while not self._stop.is_set():
            # ── Check if an AMES packet is due ──────────────────────────────
            em = self.emulator
            if em.ames_active:
                now_m = time.monotonic()
                interval_s = em.ames_interval / 1000.0
                if now_m - last_ames_time >= interval_s:
                    last_ames_time = now_m
                    pkt = em._make_ames_packet()
                    try:
                        ser.write(pkt)
                        ser.flush()
                        em.ames_sent += 1
                        self._log(
                            f"[AMES #{em.ames_sent}] {pkt.decode(errors='replace').strip()}",
                            "reply"
                        )
                    except Exception as e:
                        self._log(f"[ERROR] AMES write: {e}", "err")
                    # Stop after count packets (0 = infinite)
                    if em.ames_count > 0 and em.ames_sent >= em.ames_count:
                        em.ames_active = False
                        self._log("[AMES] Count reached, stopped.", "info")

            # ── Read incoming commands from MCU ────────────────────────────
            try:
                chunk = ser.read(256)
            except Exception as e:
                self._log(f"[ERROR] Read error: {e}", "err")
                break
            if chunk:
                buf += chunk
                while True:
                    ni = buf.find(b"\n")
                    ri = buf.find(b"\r")
                    if ni == -1 and ri == -1:
                        break
                    idx = min(x for x in (ni, ri) if x != -1)
                    line_b   = buf[:idx]
                    next_pos = idx + 1
                    if next_pos < len(buf) and buf[next_pos] in (ord("\r"), ord("\n")):
                        next_pos += 1
                    buf = buf[next_pos:]
                    if not line_b:
                        continue
                    resp, log_msg = self.emulator.process(line_b)
                    if log_msg:
                        tag = "err" if "[ERROR]" in log_msg else "cmd"
                        self._log(log_msg, tag)
                    if resp:
                        try:
                            time.sleep(0.02)
                            ser.write(resp)
                            ser.flush()
                        except Exception as e:
                            self._log(f"[ERROR] Write: {e}", "err")

        ser.close()
        self._log(f"[SERIAL] Closed {self.port}", "info")
        self.signals.done.emit()

    def _log(self, msg, tag="info"):
        ts = datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]
        self.log_q.put((f"[{ts}] {msg}", tag))


# ============================================================
#  Help tab content
# ============================================================
HELP_COMMANDS = [
    ("Открыть сессию",            "OPEN 1",           "1, 1.0.0, DVU-01 SESSION OPENED >"),
    ("Версия прошивки",           "VER",              "DVU-01 ver 1.0.0"),
    ("Старт AMES (1 Гц, ∞)",     "@1 AMES 0 1000",   "(нет ответа, начало авто-потока)"),
    ("Стоп AMES",                 "@1 AMES 0 0",      "(нет ответа)"),
    ("Запрос данных (локальный)", "M 1",              "1 5.03 181"),
    ("Запрос данных (сетевой)",   "@1 M",             "@1:0 0 5.03 5.01 5.08 4.97 0 181 180 183 179"),
    ("Закрыть сессию",            "CLOSE",            "(нет ответа)"),
]

HELP_PYTHON_SNIPPET = '''\
import serial, time

PORT = "COM3"   # ← укажите нужный порт
BAUD = 9600

s = serial.Serial(PORT, BAUD, timeout=1)
time.sleep(0.1)

def send(cmd):
    s.write((cmd + "\\r\\n").encode())
    resp = s.read_until(b"\\n").decode(errors="replace").strip()
    print(f"  >> {cmd!r:20s}  →  {resp!r}")

send("OPEN 1")
send("VER")
send("M 1")
send("@1 M")
send("CLOSE")
s.close()
'''

HELP_POWERSHELL_SNIPPET = '''\
# PowerShell — через .NET SerialPort
$port = New-Object System.IO.Ports.SerialPort("COM3", 9600)
$port.Open()
$port.WriteLine("OPEN 1")
Start-Sleep -Milliseconds 100
Write-Host $port.ReadExisting()
$port.WriteLine("VER")
Start-Sleep -Milliseconds 100
Write-Host $port.ReadExisting()
$port.WriteLine("M 1")
Start-Sleep -Milliseconds 100
Write-Host $port.ReadExisting()
$port.Close()
'''


# ============================================================
#  Main Window
# ============================================================
class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("DVU-01 Sensor Emulator")
        self.resize(960, 680)
        self.setMinimumSize(780, 560)

        self.emulator    = SensorEmulator()
        self.worker      = None
        self.log_q       = queue.Queue()
        self.reply_count = 0

        # Central widget gets the dark theme; title bar stays native
        central = QWidget()
        central.setStyleSheet(CONTENT_QSS)
        self.setCentralWidget(central)

        root = QVBoxLayout(central)
        root.setContentsMargins(0, 0, 0, 0)
        root.setSpacing(0)

        tabs = QTabWidget()
        tabs.addTab(self._build_emulator_tab(), "  Эмулятор  ")
        tabs.addTab(self._build_help_tab(),     "  Справка   ")
        root.addWidget(tabs)

        self._refresh_ports()

        self.timer = QTimer(self)
        self.timer.timeout.connect(self._drain_log)
        self.timer.start(100)

    # ================================================================
    #  TAB 1 — Emulator
    # ================================================================
    def _build_emulator_tab(self):
        w = QWidget()
        lay = QVBoxLayout(w)
        lay.setContentsMargins(12, 10, 12, 10)
        lay.setSpacing(8)

        # ── Top row ──────────────────────────────────────────────────
        top_row = QHBoxLayout()
        top_row.setSpacing(10)

        # -- Port panel --
        port_gb = QGroupBox("  Порт")
        port_gb.setSizePolicy(QSizePolicy.Minimum, QSizePolicy.Preferred)
        pl = QVBoxLayout(port_gb)
        pl.setSpacing(6)

        pr1 = QHBoxLayout()
        pr1.addWidget(self._lbl("COM-порт:"))
        self.port_cb = QComboBox()
        self.port_cb.setFixedWidth(120)
        pr1.addWidget(self.port_cb)
        ref_btn = QPushButton("⟳")
        ref_btn.setFixedSize(28, 28)
        ref_btn.setStyleSheet(f"QPushButton {{ background:{PANEL}; color:{FG_DIM}; border:none; font-size:13pt; }}"
                              f"QPushButton:hover {{ color:{ACCENT2}; }}")
        ref_btn.clicked.connect(self._refresh_ports)
        pr1.addWidget(ref_btn)
        pl.addLayout(pr1)

        pr2 = QHBoxLayout()
        pr2.addWidget(self._lbl("Скорость:"))
        self.baud_cb = QComboBox()
        self.baud_cb.addItems(["9600", "19200", "38400", "57600", "115200"])
        self.baud_cb.setCurrentText("9600")
        self.baud_cb.setFixedWidth(120)
        pr2.addWidget(self.baud_cb)
        pl.addLayout(pr2)

        top_row.addWidget(port_gb)

        # -- Sensor params --
        val_gb = QGroupBox("  Параметры датчика")
        vl = QVBoxLayout(val_gb)
        vl.setSpacing(8)

        # Speed
        sr = QHBoxLayout()
        sr.addWidget(self._lbl("Скорость ветра (м/с):"))
        self.speed_slider = QSlider(Qt.Horizontal)
        self.speed_slider.setRange(0, 600)
        self.speed_slider.setValue(50)
        self.speed_slider.setFixedWidth(240)
        sr.addWidget(self.speed_slider)
        self.speed_edit = QLineEdit("5.0")
        self.speed_edit.setFixedWidth(72)
        sr.addWidget(self.speed_edit)
        vl.addLayout(sr)
        self.speed_slider.valueChanged.connect(self._on_speed_slider)
        self.speed_edit.editingFinished.connect(self._on_speed_edit)

        # Direction
        dr = QHBoxLayout()
        dr.addWidget(self._lbl("Направление (°):       "))
        self.dir_slider = QSlider(Qt.Horizontal)
        self.dir_slider.setRange(0, 359)
        self.dir_slider.setValue(180)
        self.dir_slider.setFixedWidth(240)
        dr.addWidget(self.dir_slider)
        self.dir_edit = QLineEdit("180")
        self.dir_edit.setFixedWidth(72)
        dr.addWidget(self.dir_edit)
        vl.addLayout(dr)
        self.dir_slider.valueChanged.connect(self._on_dir_slider)
        self.dir_edit.editingFinished.connect(self._on_dir_edit)

        # Noise
        self.noise_chk = QCheckBox("Добавить шум (~±0.05 м/с, ~±0.5°)")
        self.noise_chk.setChecked(True)
        self.noise_chk.stateChanged.connect(lambda s: setattr(self.emulator, "add_noise", bool(s)))
        vl.addWidget(self.noise_chk)

        top_row.addWidget(val_gb, stretch=1)

        # -- Status --
        stat_gb = QGroupBox("  Статус")
        stat_gb.setSizePolicy(QSizePolicy.Minimum, QSizePolicy.Preferred)
        sl = QVBoxLayout(stat_gb)
        sl.setAlignment(Qt.AlignHCenter | Qt.AlignTop)
        sl.setSpacing(6)

        self.dot_lbl = QLabel("●")
        self.dot_lbl.setAlignment(Qt.AlignHCenter)
        self.dot_lbl.setStyleSheet(f"color:#374151; font-size:28pt;")
        sl.addWidget(self.dot_lbl)

        self.status_lbl = QLabel("Не подключён")
        self.status_lbl.setAlignment(Qt.AlignHCenter)
        self.status_lbl.setStyleSheet(f"color:{FG_DIM}; font-size:9pt;")
        sl.addWidget(self.status_lbl)

        self.stats_lbl = QLabel("Ответов: 0")
        self.stats_lbl.setAlignment(Qt.AlignHCenter)
        self.stats_lbl.setStyleSheet(f"color:{FG_DIM}; font-size:9pt;")
        sl.addWidget(self.stats_lbl)

        top_row.addWidget(stat_gb)
        lay.addLayout(top_row)

        # ── Buttons ──────────────────────────────────────────────────
        btn_row = QHBoxLayout()
        btn_row.setSpacing(10)

        self.start_btn = QPushButton("▶  Пуск")
        self.start_btn.setStyleSheet(BTN_PRIMARY)
        self.start_btn.setFixedHeight(36)
        self.start_btn.clicked.connect(self._start)
        btn_row.addWidget(self.start_btn)

        self.stop_btn = QPushButton("■  Стоп")
        self.stop_btn.setStyleSheet(BTN_DANGER)
        self.stop_btn.setFixedHeight(36)
        self.stop_btn.setEnabled(False)
        self.stop_btn.clicked.connect(self._stop)
        btn_row.addWidget(self.stop_btn)

        clear_btn = QPushButton("Очистить лог")
        clear_btn.setStyleSheet(BTN_SECONDARY)
        clear_btn.setFixedHeight(36)
        clear_btn.clicked.connect(self._clear_log)
        btn_row.addWidget(clear_btn)

        btn_row.addStretch()
        lay.addLayout(btn_row)

        # ── Console ──────────────────────────────────────────────────
        con_lbl = QLabel("Консоль:")
        con_lbl.setStyleSheet(f"color:{FG_DIM}; font-size:9pt; font-weight:bold;")
        lay.addWidget(con_lbl)

        self.console = QTextEdit()
        self.console.setReadOnly(True)
        self.console.setStyleSheet(
            f"QTextEdit {{ background:{CON_BG}; color:{CON_FG}; "
            f"border:1px solid #2d2d4e; border-radius:4px; padding:4px; }}"
        )
        mono = QFont("Cascadia Code", 9)
        if not mono.exactMatch():
            mono = QFont("Consolas", 9)
        self.console.setFont(mono)
        lay.addWidget(self.console, stretch=1)

        return w

    # ================================================================
    #  TAB 2 — Help
    # ================================================================
    def _build_help_tab(self):
        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        scroll.setStyleSheet("QScrollArea { border: none; }")

        container = QWidget()
        lay = QVBoxLayout(container)
        lay.setContentsMargins(16, 14, 16, 20)
        lay.setSpacing(14)
        scroll.setWidget(container)

        # ── Section: Commands table ───────────────────────────────────
        lay.addWidget(self._help_section_title("Поддерживаемые команды"))

        desc_lbl = QLabel(
            "Команды завершаются <b>\\r\\n</b> (CR LF). "
            "Нажмите <b>Копировать</b> рядом с командой, чтобы скопировать её в буфер обмена."
        )
        desc_lbl.setWordWrap(True)
        desc_lbl.setStyleSheet(f"color:{FG_DIM}; font-size:9pt;")
        lay.addWidget(desc_lbl)

        for desc, cmd, resp in HELP_COMMANDS:
            row = self._help_cmd_row(desc, cmd, resp)
            lay.addWidget(row)

        lay.addWidget(self._separator())

        # ── Section: Python snippet ───────────────────────────────────
        lay.addWidget(self._help_section_title("Python — тест через pyserial"))
        lay.addWidget(self._help_code_block(HELP_PYTHON_SNIPPET))

        lay.addWidget(self._separator())

        # ── Section: PowerShell snippet ──────────────────────────────
        lay.addWidget(self._help_section_title("PowerShell — тест через .NET SerialPort"))
        lay.addWidget(self._help_code_block(HELP_POWERSHELL_SNIPPET))

        lay.addStretch()
        return scroll

    def _help_section_title(self, text):
        lbl = QLabel(text)
        lbl.setStyleSheet(f"color:{ACCENT2}; font-size:11pt; font-weight:bold;")
        return lbl

    def _separator(self):
        line = QFrame()
        line.setFrameShape(QFrame.HLine)
        line.setStyleSheet(f"color: #3f3f5e;")
        return line

    def _help_cmd_row(self, desc, cmd, resp):
        """One row: description | command (copyable) | expected response."""
        frame = QWidget()
        frame.setStyleSheet(f"QWidget {{ background:{PANEL}; border-radius:6px; }}")
        h = QHBoxLayout(frame)
        h.setContentsMargins(10, 7, 10, 7)
        h.setSpacing(10)

        # Description
        d_lbl = QLabel(desc)
        d_lbl.setFixedWidth(200)
        d_lbl.setStyleSheet(f"color:{FG}; font-size:9pt;")
        h.addWidget(d_lbl)

        # Command
        cmd_edit = QLineEdit(cmd)
        cmd_edit.setReadOnly(True)
        mono = QFont("Consolas", 10)
        cmd_edit.setFont(mono)
        cmd_edit.setStyleSheet(
            f"QLineEdit {{ background:{ENTRY_BG}; color:{CON_CMD}; "
            f"border:1px solid #3f3f5e; border-radius:4px; padding:2px 8px; }}"
        )
        h.addWidget(cmd_edit, stretch=1)

        # Copy button
        copy_btn = QPushButton("Копировать")
        copy_btn.setStyleSheet(BTN_COPY)
        copy_btn.setFixedHeight(28)
        copy_btn.clicked.connect(lambda checked=False, c=cmd: self._copy_to_clipboard(c))
        h.addWidget(copy_btn)

        # Arrow
        arr = QLabel("→")
        arr.setStyleSheet(f"color:{FG_DIM};")
        h.addWidget(arr)

        # Response
        resp_lbl = QLabel(resp)
        mono2 = QFont("Consolas", 9)
        resp_lbl.setFont(mono2)
        resp_lbl.setStyleSheet(f"color:{CON_FG}; font-size:9pt;")
        resp_lbl.setWordWrap(False)
        h.addWidget(resp_lbl, stretch=1)

        return frame

    def _help_code_block(self, code: str):
        """Editable code block with a copy-all button."""
        frame = QWidget()
        frame.setStyleSheet(f"QWidget {{ background:{PANEL}; border-radius:6px; }}")
        v = QVBoxLayout(frame)
        v.setContentsMargins(0, 0, 0, 0)
        v.setSpacing(0)

        # Header bar with Copy button
        hdr = QWidget()
        hdr.setStyleSheet(f"background:{ENTRY_BG}; border-radius:6px 6px 0 0;")
        hdr_lay = QHBoxLayout(hdr)
        hdr_lay.setContentsMargins(10, 4, 8, 4)
        hdr_lay.addStretch()
        copy_all = QPushButton("Копировать всё")
        copy_all.setStyleSheet(BTN_COPY)
        copy_all.setFixedHeight(24)
        v.addWidget(hdr)
        hdr_lay.addWidget(copy_all)

        edit = QTextEdit()
        edit.setPlainText(code.strip())
        edit.setReadOnly(True)
        edit.setStyleSheet(
            f"QTextEdit {{ background:{CON_BG}; color:{CON_FG}; "
            f"border: none; border-radius: 0 0 6px 6px; padding:8px; }}"
        )
        mono = QFont("Consolas", 9)
        edit.setFont(mono)
        edit.setFixedHeight(max(120, code.count("\n") * 18 + 30))
        v.addWidget(edit)

        copy_all.clicked.connect(lambda: self._copy_to_clipboard(edit.toPlainText()))
        return frame

    # ================================================================
    #  Helpers
    # ================================================================
    def _lbl(self, text):
        l = QLabel(text)
        return l

    def _copy_to_clipboard(self, text: str):
        QApplication.clipboard().setText(text)
        self._append(f"[INFO] Скопировано в буфер: {text!r}", "info")

    def _refresh_ports(self):
        self.port_cb.clear()
        for p in serial.tools.list_ports.comports():
            self.port_cb.addItem(p.device)

    # ── Controls ─────────────────────────────────────────────────────
    def _start(self):
        port = self.port_cb.currentText()
        if not port:
            self._append("[ERROR] Выберите COM-порт!", "err")
            return
        baud = int(self.baud_cb.currentText())
        self.worker = SerialWorker(port, baud, self.emulator, self.log_q)
        self.worker.signals.done.connect(self._on_worker_done)
        self.worker.start()
        self.start_btn.setEnabled(False)
        self.stop_btn.setEnabled(True)
        self.dot_lbl.setStyleSheet(f"color:{GREEN}; font-size:28pt;")
        self.status_lbl.setText(f"{port}  {baud}")

    def _stop(self):
        if self.worker:
            self.worker.stop()
        self.start_btn.setEnabled(True)
        self.stop_btn.setEnabled(False)
        self.dot_lbl.setStyleSheet("color:#374151; font-size:28pt;")
        self.status_lbl.setText("Не подключён")

    def _on_worker_done(self):
        self.worker = None
        self.start_btn.setEnabled(True)
        self.stop_btn.setEnabled(False)
        self.dot_lbl.setStyleSheet("color:#374151; font-size:28pt;")
        self.status_lbl.setText("Не подключён")

    # ── Sensor value handlers ─────────────────────────────────────────
    def _on_speed_slider(self, val):
        v = val / 10.0
        self.emulator.speed = v
        self.speed_edit.setText(f"{v:.1f}")

    def _on_speed_edit(self):
        try:
            v = max(0.0, min(60.0, float(self.speed_edit.text())))
            self.emulator.speed = v
            self.speed_slider.blockSignals(True)
            self.speed_slider.setValue(int(v * 10))
            self.speed_slider.blockSignals(False)
            self.speed_edit.setText(f"{v:.1f}")
        except ValueError:
            pass

    def _on_dir_slider(self, val):
        self.emulator.direction = float(val)
        self.dir_edit.setText(str(val))

    def _on_dir_edit(self):
        try:
            v = float(self.dir_edit.text()) % 360
            self.emulator.direction = v
            self.dir_slider.blockSignals(True)
            self.dir_slider.setValue(int(v))
            self.dir_slider.blockSignals(False)
            self.dir_edit.setText(f"{v:.0f}")
        except ValueError:
            pass

    # ── Log ──────────────────────────────────────────────────────────
    def _drain_log(self):
        while not self.log_q.empty():
            msg, tag = self.log_q.get_nowait()
            self._append(msg, tag)

    def _append(self, text: str, tag: str = "info"):
        color_map = {"cmd": CON_CMD, "err": CON_ERR, "info": CON_INFO, "reply": CON_FG}
        color = color_map.get(tag, CON_FG)
        cur = self.console.textCursor()
        cur.movePosition(QTextCursor.End)
        fmt = QTextCharFormat()
        fmt.setForeground(QColor(color))
        cur.setCharFormat(fmt)
        cur.insertText(text + "\n")
        self.console.setTextCursor(cur)
        self.console.ensureCursorVisible()
        if tag == "cmd":
            self.reply_count += 1
            self.stats_lbl.setText(f"Ответов: {self.reply_count}")

    def _clear_log(self):
        self.console.clear()

    def closeEvent(self, event):
        self._stop()
        event.accept()


# ============================================================
if __name__ == "__main__":
    app = QApplication(sys.argv)
    # No custom style/palette → native Windows title bar is preserved
    win = MainWindow()
    win.show()
    sys.exit(app.exec())
