# http://doc.qt.io/qt-5/qt.html

import os
import queue
import re
import socket
import struct
import subprocess
import sys
import time
import traceback
import uuid
from collections import namedtuple
from datetime import datetime
from functools import partial
from hashlib import sha256
from math import log10, nan

import numpy as np
import pyperclip
import requests
import serial
import serial.tools.list_ports
import simplejson
import stackprinter
from loguru import logger
from PyQt5.QtCore import QMutex, Qt, QThreadPool, QTimer
from PyQt5.QtGui import QFont, QIcon, QPalette, QKeySequence
from PyQt5.QtWidgets import (
    QApplication,
    QButtonGroup,
    QCheckBox,
    QComboBox,
    QDesktopWidget,
    QDialog,
    QDoubleSpinBox,
    QFileDialog,
    QFrame,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QInputDialog,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QProgressBar,
    QPushButton,
    QRadioButton,
    QShortcut,
    QSizePolicy,
    QSlider,
    QSpacerItem,
    QSpinBox,
    QStatusBar,
    QTabWidget,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)
import qtmodern.styles
import qtmodern.windows

from bytes_helper import bytes2Float, bytesPuttyPrint
from dc201_pack import DC201_PACK, DC201_ParamInfo, DC201ErrorCode, parse_1440, write_firmware_pack_BL, write_firmware_pack_FC, write_firmware_pack_SA
from deal_openpyxl import ILLU_CC_DataInfo, TEMP_CC_DataInfo, check_file_permission, dump_CC, dump_correct_record, dump_sample, load_CC  # insert_sample
from mengy_color_table import ColorGreens, ColorPurples, ColorReds
from qt_modern_dialog import ModernDialog, ModernMessageBox
from qt_serial import SerialRecvWorker, SerialSendWorker
from sample_data import SAMPLE_SET_INFOS, MethodEnum, SampleDB, WaveEnum
from sample_graph import CC_Graph, SampleGraph, TemperatureGraph, point_line_equation_map
from update import get_latest_app_bin
from version import VERSION

BARCODE_NAMES = ("B1", "B2", "B3", "B4", "B5", "B6", "QR")
TEMPERAUTRE_NAMES = ("下加热体:", "上加热体:")
LINE_COLORS = ("b", "g", "r", "c", "m", "y", "k", "w")
LINE_SYMBOLS = ("o", "s", "t", "d", "+")
TEMP_RAW_COLORS = (
    (100, 30, 22),
    (118, 68, 138),
    (41, 128, 185),
    (118, 215, 196),
    (247, 220, 111),
    (211, 84, 0),
    (253, 254, 254),
    (30, 132, 73),
    (236, 112, 99),
    (210, 180, 222),
    (156, 100, 12),
)
TEMP_RAW_SYMBOL_CONFIG = (("o", 4, "b"), ("s", 4, "g"), ("t", 4, "r"), ("d", 4, "c"), ("+", 4, "m"), ("o", 4, "y"), ("s", 4, "k"), ("t", 4, "w"), ("d", 4, "r"))
METHOD_NAMES = ("无项目", "速率法", "终点法", "两点终点法")
WAVE_NAMES = ("610", "550", "405")
SampleConf = namedtuple("SampleConf", "method wave point_num set_info")
HEATER_PID_FQ = 1000 / 100
HEATER_PID_PS = [1, HEATER_PID_FQ, 1 / HEATER_PID_FQ, 1, 1, 1]


ICON_PATH = "./icos/tt.ico"
FLASH_CONF_DATA_PATH = "./data/flash.json"
CONFIG_PATH = "./conf/config.json"
CONFIG = dict()
try:
    with open(CONFIG_PATH, "r", encoding="utf-8") as f:
        CONFIG = simplejson.load(f)
    with open(CONFIG_PATH, "w", encoding="utf-8") as f:
        simplejson.dump(CONFIG, f, indent=4)
    if "pd_criterion" not in CONFIG.keys() or "sh_criterion" not in CONFIG.keys():
        raise ValueError("lost key")
except Exception:
    logger.error(f"load conf failed \n{stackprinter.format()}")
    CONFIG = dict()
    CONFIG["log"] = dict(rotation="4 MB", retention=16)
    CONFIG["pd_criterion"] = {"610": [6000000, 14000000], "550": [6000000, 14000000], "405": [6000000, 14000000]}
    CONFIG["sh_criterion"] = {"610": (1434, 3080, 4882, 6894, 8818, 10578), "550": (2181, 3943, 5836, 7989, 10088, 12032), "405": (0, 0, 0, 0, 0, 0)}
    CONFIG["app_bin_chunk_size"] = 4096
    try:
        with open(CONFIG_PATH, "w", encoding="utf-8") as f:
            simplejson.dump(CONFIG, f, indent=4)
    except Exception:
        logger.error(f"dump conf failed \n{stackprinter.format()}")

rotation = CONFIG.get("log", {}).get("rotation", "4 MB")
retention = CONFIG.get("log", {}).get("retention", 16)
logger.add("./log/dc201.log", rotation=rotation, retention=retention, enqueue=True, encoding="utf8")

DB_EXCEL_PATH_RE = re.compile(r"s(\d+)n(\d+)")
TMER_INTERVAL = 200
APP_BIN_CHUNK_SIZE = CONFIG.get("app_bin_chunk_size", 4096)  # min 256 max 4096


class QHLine(QFrame):
    def __init__(self):
        super(QHLine, self).__init__()
        self.setFrameShape(QFrame.HLine)
        self.setFrameShadow(QFrame.Sunken)


class QVLine(QFrame):
    def __init__(self):
        super(QVLine, self).__init__()
        self.setFrameShape(QFrame.VLine)
        self.setFrameShadow(QFrame.Sunken)


class MainWindow(QMainWindow):
    def __init__(self, *args, **kwargs):
        super(MainWindow, self).__init__(*args, **kwargs)
        self.setWindowTitle(f"DC201 工装测试 {VERSION}")
        self.serial = serial.Serial(port=None, baudrate=115200, timeout=0.01)
        self.task_queue = queue.Queue()
        self.henji_queue = queue.Queue()
        self.last_firm_path = None
        self.last_bl_path = None
        self.last_samplefr_path = None
        self.threadpool = QThreadPool()
        self.id_card_data = bytearray(4096)
        self.storge_time_start = 0
        self.out_flash_start = 0
        self.out_flash_length = 0
        self.out_flash_data = bytearray()
        self.temp_start_time = None
        self.dd = DC201_PACK()
        self.pack_index = 1
        self.device_id = ""
        self.version = ""
        self.device_datetime = datetime.now()
        self.serial_recv_worker = None
        self.serial_send_worker = None
        self.warn_msgbox_cnt = 0
        self.sample_db = SampleDB("sqlite:///data/db.sqlite3", device_id=self.device_id)
        self.sample_record_current_label = None
        self.flash_json_data = None
        self.last_falsh_save_dir = "./"
        try:
            current_machine_id = subprocess.check_output("wmic csproduct get uuid").decode().split("\n")[1].strip()
        except Exception:
            current_machine_id = uuid.uuid3(uuid.NAMESPACE_DNS, socket.gethostname())
        self.data_xlsx_path = f"data/{current_machine_id}.xlsx"
        logger.debug(f"self.data_xlsx_path | {os.path.abspath(self.data_xlsx_path)}")
        self.initUI()
        self.center()

    def _serialSendPack(self, cmd_type, payload=None, device_id=0x13):
        try:
            pack = self.dd.buildPack(device_id, self.getPackIndex(), cmd_type, payload)
        except Exception:
            logger.error(f"build pack exception \n{stackprinter.format()}")
            raise
        self.task_queue.put(pack)
        if not self.serial.isOpen():
            logger.debug(f"try send | {bytesPuttyPrint(pack)}")

    def _clearTaskQueue(self):
        while True:
            try:
                self.task_queue.get_nowait()
            except queue.Empty:
                break
            except Exception:
                logger.error(f"clear task queue exception \n{stackprinter.format()}")
                break

    def _getHeater(self):
        self._serialSendPack(0xD3)
        self._serialSendPack(0xD3, (0, 0, 4))
        self._serialSendPack(0xD3, (1, 0, 4))

    def _getDebuFlag(self):
        self._serialSendPack(0xD4)

    def _getDeviceID(self):
        self._serialSendPack(0xDF)

    def _getStatus(self):
        self.sample_fr_bt.setToolTip("")
        self._serialSendPack(0x07)
        self._getHeater()
        self._getDebuFlag()
        self._getDeviceID()
        self._serialSendPack(0xDC, (3,))
        self._serialSendPack(0x92)

    def _setColor(self, wg, nbg=None, nfg=None):
        palette = wg.palette()
        bgc = palette.color(QPalette.Background)
        fgc = palette.color(QPalette.Foreground)
        # Convert the QColors to a string hex for use in the Stylesheet.
        bg = bgc.name()
        fg = fgc.name()
        # logger.info(f"origin clolr is back | {bg} | front | {fg}")
        if nbg is not None:
            bg = nbg
        if nfg is not None:
            fg = nfg
        if nfg is None and nbg is None:
            wg.setStyleSheet("")
        else:
            wg.setStyleSheet(f"background-color: {bg}; color: {fg}")

    def _clear_widget_style_sheet(self, widget):
        widget.setStyleSheet("")
        layout = widget.layout()
        for i in range(layout.count()):
            layout.itemAt(i).widget().setStyleSheet("")

    def initUI(self):
        self.createBarcode()
        self.createMotor()
        self.createSerial()
        self.create_Sample_LED()
        self.createMatplot()
        self.createStatusBar()
        self.createSysConf()
        self.createStorgeDialog()
        self.createSelfCheckDialog()
        widget = QWidget()
        layout = QHBoxLayout(widget)

        right_ly = QVBoxLayout()
        right_ly.setContentsMargins(0, 5, 0, 0)
        right_ly.setSpacing(0)
        right_ly.addWidget(self.barcode_gb)
        right_ly.addWidget(self.motor_gb)
        right_ly.addWidget(self.sys_conf_gb)
        temp_ly = QHBoxLayout()
        temp_ly.addWidget(self.serial_gb)
        temp_ly.addWidget(self.sample_led_gb)
        right_ly.addLayout(temp_ly)

        layout.addLayout(right_ly, stretch=0)
        layout.addWidget(self.matplot_wg, stretch=1)
        self.setWindowIcon(QIcon(ICON_PATH))
        self.setCentralWidget(widget)
        self.resize(1024, 587)

    def center(self):
        # logger.debug(f"invoke center in ModernWidget")
        qr = self.frameGeometry()
        cp = QDesktopWidget().availableGeometry().center()
        qr.moveCenter(cp)
        self.move(qr.topLeft())

    def resizeEvent(self, event):
        logger.debug(f"windows size | {self.size()}")
        pass

    def closeEvent(self, event):
        logger.debug("invoke close event")
        if self.serial_recv_worker is not None:
            self.serial_recv_worker.signals.owari.emit()
        if self.serial_send_worker is not None:
            self.serial_send_worker.signals.owari.emit()
        self.threadpool.waitForDone(1000)
        sys.exit()

    def mouseDoubleClickEvent(self, event):
        self._getStatus()

    def createStatusBar(self):
        self.status_bar = QStatusBar(self)
        self.status_bar.layout().setContentsMargins(5, 0, 5, 0)
        self.status_bar.layout().setSpacing(0)

        temperautre_wg = QWidget()
        temperautre_ly = QHBoxLayout(temperautre_wg)
        temperautre_ly.setContentsMargins(5, 0, 5, 0)
        temperautre_ly.setSpacing(5)
        self.temperautre_lbs = [QLabel("-" * 8) for _ in TEMPERAUTRE_NAMES]
        for i, name in enumerate(TEMPERAUTRE_NAMES):
            temperautre_ly.addWidget(QLabel(name))
            temperautre_ly.addWidget(self.temperautre_lbs[i])
        temperautre_wg.mousePressEvent = self.onTemperautreLabelClick

        self.temperature_plot_dg = QDialog(self)
        self.temperature_plot_dg.setWindowTitle("温度记录")
        temperature_plot_ly = QVBoxLayout(self.temperature_plot_dg)

        self.temp_graph_tab = QTabWidget(self.temperature_plot_dg)
        self.temperature_plot_graph = TemperatureGraph(parent=self.temp_graph_tab)
        self.temp_graph_tab.addTab(self.temperature_plot_graph.win, "加热体")

        self.temperature_raw_graph = TemperatureGraph(parent=self.temp_graph_tab)
        for i in range(9):
            self.temperature_raw_graph.plot_data_new(name=f"#{i + 1}")
        self.temp_graph_tab.addTab(self.temperature_raw_graph.win, "ADC 采样")

        self.temperature_B_PID_graph = TemperatureGraph(parent=self.temp_graph_tab)
        self.temperature_B_PID_graph.plot_data_new(name="B_Out")
        self.temperature_B_PID_graph.plot_data_new(name="B_Op")
        self.temperature_B_PID_graph.plot_data_new(name="B_Oi")
        self.temperature_B_PID_graph.plot_data_new(name="B_Od")
        self.temp_graph_tab.addTab(self.temperature_B_PID_graph.win, "下加热体 PID")

        self.temperature_T_PID_graph = TemperatureGraph(parent=self.temp_graph_tab)
        self.temperature_T_PID_graph.plot_data_new(name="T_Out")
        self.temperature_T_PID_graph.plot_data_new(name="T_Op")
        self.temperature_T_PID_graph.plot_data_new(name="T_Oi")
        self.temperature_T_PID_graph.plot_data_new(name="T_Od")
        self.temp_graph_tab.addTab(self.temperature_T_PID_graph.win, "上加热体 PID")

        temperature_plot_ly.addWidget(self.temp_graph_tab)
        self.temperature_plot_reset_bt = QPushButton("默认参数")
        self.temperature_plot_clear_bt = QPushButton("清零")
        temperature_heater_ctl_ly = QVBoxLayout()
        temperature_heater_ctl_btm_ly = QHBoxLayout()
        self.temperature_heater_btm_ks_bts = [QPushButton(i) for i in ("读", "写")]
        self.temperature_heater_btm_ks_sps = [QDoubleSpinBox() for _ in range(4)]
        for i, sp in enumerate(self.temperature_heater_btm_ks_sps):
            sp.setDecimals(2)
            if i < 3:
                sp.setRange(0, 99999999)
                sp.setMaximumWidth(90)
            else:
                sp.setRange(0, 75)
                sp.setMaximumWidth(60)
                sp.setSuffix("℃")
            temperature_heater_ctl_btm_ly.addWidget(QLabel(("Kp", "Ki", "Kd", "目标")[i]))
            temperature_heater_ctl_btm_ly.addWidget(sp)
        for bt in self.temperature_heater_btm_ks_bts:
            bt.setMaximumWidth(45)
            bt.clicked.connect(partial(self.onHeaterCtlRW, pbt=bt))
            temperature_heater_ctl_btm_ly.addWidget(bt)
        self.temperature_heater_btm_cb = QCheckBox("下加热体使能")
        temperature_heater_ctl_btm_ly.addWidget(self.temperature_heater_btm_cb)
        temperature_heater_ctl_top_ly = QHBoxLayout()
        self.temperature_heater_top_ks_bts = [QPushButton(i) for i in ("读", "写")]
        self.temperature_heater_top_ks_sps = [QDoubleSpinBox() for _ in range(4)]
        for i, sp in enumerate(self.temperature_heater_top_ks_sps):
            sp.setDecimals(2)
            if i < 3:
                sp.setRange(0, 99999999)
                sp.setMaximumWidth(90)
            else:
                sp.setRange(0, 75)
                sp.setMaximumWidth(60)
                sp.setSuffix("℃")
            temperature_heater_ctl_top_ly.addWidget(QLabel(("Kp", "Ki", "Kd", "目标")[i]))
            temperature_heater_ctl_top_ly.addWidget(sp)
        for bt in self.temperature_heater_top_ks_bts:
            bt.setMaximumWidth(45)
            bt.clicked.connect(partial(self.onHeaterCtlRW, pbt=bt))
            temperature_heater_ctl_top_ly.addWidget(bt)
        self.temperature_heater_top_cb = QCheckBox("上加热体使能")
        temperature_heater_ctl_top_ly.addWidget(self.temperature_heater_top_cb)
        temperature_heater_ctl_ly.addLayout(temperature_heater_ctl_btm_ly)
        temperature_heater_ctl_ly.addLayout(temperature_heater_ctl_top_ly)
        temp_ly = QHBoxLayout()
        temp_ly.setSpacing(3)
        temp_ly.setContentsMargins(3, 3, 3, 3)
        tempbt_ly = QVBoxLayout()
        tempbt_ly.addWidget(self.temperature_plot_reset_bt)
        tempbt_ly.addWidget(self.temperature_plot_clear_bt)
        temp_ly.addLayout(tempbt_ly)
        temp_ly.addStretch(1)
        temp_ly.addLayout(temperature_heater_ctl_ly)
        temperature_plot_ly.addLayout(temp_ly)

        overshoot_btm_ly = QHBoxLayout()
        self.temperature_overshoot_btm_sps = []
        self.temperature_overshoot_btm_read_bt = QPushButton("读", maximumWidth=30, clicked=self.onTemperatureOvershootRead)
        self.temperature_overshoot_btm_write_bt = QPushButton("写", maximumWidth=30, clicked=self.onTemperatureOvershootWrite)
        overshoot_top_ly = QHBoxLayout()
        self.temperature_overshoot_top_sps = []
        self.temperature_overshoot_top_read_bt = QPushButton("读", maximumWidth=30, clicked=self.onTemperatureOvershootRead)
        self.temperature_overshoot_top_write_bt = QPushButton("写", maximumWidth=30, clicked=self.onTemperatureOvershootWrite)
        for name in ("最大偏差", "水平维持", "总持续", "k", "b", "A", "C"):
            overshoot_btm_ly.addWidget(QLabel(name))
            sp = QDoubleSpinBox(minimum=-500, maximum=500, decimals=3, maximumWidth=60)
            overshoot_btm_ly.addWidget(sp)
            self.temperature_overshoot_btm_sps.append(sp)
            overshoot_top_ly.addWidget(QLabel(name))
            sp = QDoubleSpinBox(minimum=-500, maximum=500, decimals=3, maximumWidth=60)
            overshoot_top_ly.addWidget(sp)
            self.temperature_overshoot_top_sps.append(sp)

        overshoot_btm_ly.addWidget(self.temperature_overshoot_btm_read_bt)
        overshoot_btm_ly.addWidget(self.temperature_overshoot_btm_write_bt)
        overshoot_top_ly.addWidget(self.temperature_overshoot_top_read_bt)
        overshoot_top_ly.addWidget(self.temperature_overshoot_top_write_bt)
        temperature_plot_ly.addLayout(overshoot_btm_ly)
        temperature_plot_ly.addLayout(overshoot_top_ly)

        self.temperautre_wg_raw_lbs = [QLabel("-" * 4) for _ in range(9)]
        temp_wg = QWidget()
        temp_ly = QHBoxLayout(temp_wg)
        for i, l in enumerate(self.temperautre_wg_raw_lbs):
            temp_ly.addWidget(l)
        temperature_plot_ly.addWidget(temp_wg)

        temp_ly = QHBoxLayout()
        temp_ly.addWidget(QLabel("温度校正参数: "))
        temp_ly.setContentsMargins(3, 3, 3, 3)
        temp_ly.setSpacing(3)
        self.out_flash_param_temp_sps = [
            QDoubleSpinBox(self, maximumWidth=90, minimum=-5, maximum=5, decimals=3, singleStep=0.035, suffix="℃") for _ in range(3)
        ]
        for i, sp in enumerate(self.out_flash_param_temp_sps):
            temp_ly.addWidget(QLabel(("上加热体", "下加热体", "环境")[i]), stretch=0, alignment=Qt.AlignLeft)
            temp_ly.addWidget(sp, stretch=1)
        temp_ly.addStretch(1)
        self.out_flash_param_temp_read_bt = QPushButton("读取", clicked=self.on_out_flash_param_temp_read)
        self.out_flash_param_temp_write_bt = QPushButton("写入", clicked=self.on_out_flash_param_temp_write)
        temp_ly.addWidget(self.out_flash_param_temp_read_bt)
        temp_ly.addWidget(self.out_flash_param_temp_write_bt)
        temperature_plot_ly.addLayout(temp_ly)

        self.temperature_heater_btm_cb.clicked.connect(self.onTemperatureHeaterChanged)
        self.temperature_heater_top_cb.clicked.connect(self.onTemperatureHeaterChanged)
        self.temperature_plot_clear_bt.clicked.connect(self.onTemperatureTabGraphClear)
        self.temperature_plot_reset_bt.clicked.connect(self.onTemperatureCtlResetPara)
        self.onTemperautreDataClear()

        motor_tray_position_wg = QWidget()
        motor_tray_position_ly = QHBoxLayout(motor_tray_position_wg)
        motor_tray_position_ly.setContentsMargins(5, 0, 5, 0)
        motor_tray_position_ly.setSpacing(5)
        self.motor_tray_position = QLabel("******")
        motor_tray_position_ly.addWidget(QLabel("托盘："))
        motor_tray_position_ly.addWidget(self.motor_tray_position)

        kirakira_wg = QWidget()
        kirakira_ly = QHBoxLayout(kirakira_wg)
        kirakira_ly.setContentsMargins(5, 0, 5, 0)
        kirakira_ly.setSpacing(5)
        self.kirakira_recv_lb = QLabel("    ")
        self.kirakira_recv_time = time.time()
        self.kirakira_send_lb = QLabel("    ")
        self.kirakira_send_time = time.time()
        kirakira_ly.addWidget(QLabel("发送"))
        kirakira_ly.addWidget(self.kirakira_send_lb)
        kirakira_ly.addWidget(QLabel("接收"))
        kirakira_ly.addWidget(self.kirakira_recv_lb)

        self.version_lb = QLabel("版本: *.*")
        self.version_bl_lb = QLabel("BL: *.*")
        self.device_id_lb = QLabel("ID: *.*")

        self.status_bar.addWidget(temperautre_wg, 0)
        self.status_bar.addWidget(motor_tray_position_wg, 0)
        self.status_bar.addWidget(kirakira_wg, 0)
        self.status_bar.addWidget(self.version_lb, 0)
        self.status_bar.addWidget(self.version_bl_lb, 0)
        self.status_bar.addWidget(self.device_id_lb, 0)
        self.setStatusBar(self.status_bar)
        self.temperature_plot_dg.resize(600, 600)
        self.temperature_plot_dg = ModernDialog(self.temperature_plot_dg, self)

    def onTemperautreLabelClick(self, event):
        self._getHeater()
        self.temperature_plot_dg.show()

    def onTemperatureHeaterChanged(self, event):
        data = 0
        if self.temperature_heater_btm_cb.isChecked():
            data |= 1 << 0
        else:
            data &= ~(1 << 0)
        if self.temperature_heater_top_cb.isChecked():
            data |= 1 << 1
        else:
            data &= ~(1 << 1)
        self._serialSendPack(0xD3, (data,))

    def onTemperatureOvershootRead(self, event):
        sender = self.sender()
        if sender is self.temperature_overshoot_btm_read_bt:
            self._serialSendPack(0xD3, (0x04,))
        elif sender is self.temperature_overshoot_top_read_bt:
            self._serialSendPack(0xD3, (0x05,))

    def onTemperatureOvershootWrite(self, event):
        sender = self.sender()
        if sender is self.temperature_overshoot_btm_write_bt:
            values = [i.value() for i in self.temperature_overshoot_btm_sps[:3]]
            payload_bytes = b"".join([struct.pack("f", i) for i in values])
            payload = [0x04] + [i for i in payload_bytes]
            self._serialSendPack(0xD3, payload)
        elif sender is self.temperature_overshoot_top_write_bt:
            values = [i.value() for i in self.temperature_overshoot_top_sps[:3]]
            payload_bytes = b"".join([struct.pack("f", i) for i in values])
            payload = [0x05] + [i for i in payload_bytes]
            self._serialSendPack(0xD3, payload)

    def onHeaterCtlRW(self, event=None, pbt=None):
        logger.debug(f"invoke onHeaterCtlRW | event {event} | pbt {pbt}")
        if pbt in self.temperature_heater_btm_ks_bts:
            idx = self.temperature_heater_btm_ks_bts.index(pbt)
            if idx == 0:
                self._serialSendPack(0xD3, (0, 0, 4))
            else:
                data = b""
                for i, sp in enumerate(self.temperature_heater_btm_ks_sps):
                    data += struct.pack("f", sp.value() / HEATER_PID_PS[i])
                    self._setColor(sp, nfg="red")
                self._serialSendPack(0xD3, (0, 0, 4, *data))
                QTimer.singleShot(1000, lambda: self._serialSendPack(0xD3, (0, 0, 4)))
        elif pbt in self.temperature_heater_top_ks_bts:
            idx = self.temperature_heater_top_ks_bts.index(pbt)
            if idx == 0:
                self._serialSendPack(0xD3, (1, 0, 4))
            else:
                data = b""
                for i, sp in enumerate(self.temperature_heater_top_ks_sps):
                    data += struct.pack("f", sp.value() / HEATER_PID_PS[i])
                    self._setColor(sp, nfg="red")
                self._serialSendPack(0xD3, (1, 0, 4, *data))
                QTimer.singleShot(1000, lambda: self._serialSendPack(0xD3, (1, 0, 4)))
        else:
            return

    def updateTemperatureHeater(self, info):
        raw_bytes = info.content
        if len(raw_bytes) == 8:
            data = raw_bytes[6]
            if data & (1 << 0):
                self.temperature_heater_btm_cb.setChecked(True)
            else:
                self.temperature_heater_btm_cb.setChecked(False)
            if data & (1 << 1):
                self.temperature_heater_top_cb.setChecked(True)
            else:
                self.temperature_heater_top_cb.setChecked(False)
        elif len(raw_bytes) == 26:
            bt = raw_bytes[6]
            logger.debug(f"get heater conf value | bt {bt}")
            for i in range(4):
                value = struct.unpack("f", raw_bytes[9 + i * 4 : 13 + i * 4])[0]
                value = value * HEATER_PID_PS[i]
                if bt == 0:
                    sp = self.temperature_heater_btm_ks_sps[i]
                    sp.setValue(value)
                    self._setColor(sp)
                else:
                    sp = self.temperature_heater_top_ks_sps[i]
                    sp.setValue(value)
                    self._setColor(sp)
        elif len(raw_bytes) == 36:
            if raw_bytes[6] == 4:
                for i, sp in enumerate(self.temperature_overshoot_btm_sps):
                    value = struct.unpack("f", raw_bytes[7 + 4 * i : 11 + 4 * i])[0]
                    logger.info(f"ghet temperature overshoot param | {i+1} | {value}")
                    sp.setValue(value)
            elif raw_bytes[6] == 5:
                for i, sp in enumerate(self.temperature_overshoot_top_sps):
                    value = struct.unpack("f", raw_bytes[7 + 4 * i : 11 + 4 * i])[0]
                    logger.info(f"ghet temperature overshoot param | {i+1} | {value}")
                    sp.setValue(value)
        else:
            logger.info(f"get other temperatur debug pack | {len(raw_bytes)} | {info.text}")

    def onTemperatureCtlResetPara(self, event):
        for i, k in enumerate(("Kp", "Ki", "Kd", "target")):
            v = CONFIG.get("temp_ctl", {}).get("btm", {}).get(k)
            if v is not None:
                self.temperature_heater_btm_ks_sps[i].setValue(v)
            v = CONFIG.get("temp_ctl", {}).get("top", {}).get(k)
            if v is not None:
                self.temperature_heater_top_ks_sps[i].setValue(v)

    def onTemperatureTabGraphClear(self, event=None):
        cuttent_wg = self.temp_graph_tab.currentWidget()
        if 1 or cuttent_wg is self.temperature_plot_graph.win:
            self.onTemperautreDataClear()
            self.temperature_raw_graph.clear_plot()
            for i in range(9):
                self.temperature_raw_graph.plot_data_new(name=f"#{i + 1}")

            self.temperature_B_PID_graph.clear_plot()
            self.temperature_B_PID_graph.plot_data_new(name="B_Out")
            self.temperature_B_PID_graph.plot_data_new(name="B_Op")
            self.temperature_B_PID_graph.plot_data_new(name="B_Oi")
            self.temperature_B_PID_graph.plot_data_new(name="B_Od")

            self.temperature_T_PID_graph.clear_plot()
            self.temperature_T_PID_graph.plot_data_new(name="T_Out")
            self.temperature_T_PID_graph.plot_data_new(name="T_Op")
            self.temperature_T_PID_graph.plot_data_new(name="T_Oi")
            self.temperature_T_PID_graph.plot_data_new(name="T_Od")

    def onTemperautreDataClear(self, event=None):
        self.temp_start_time = None
        self.temperature_plot_graph.clear_plot()
        self.temperature_plot_graph.plot_data_new(name="下加热体", color="FF0000")
        self.temperature_plot_graph.plot_data_new(name="上加热体", color="0000FF")
        self.temperature_plot_graph.plot_data_new(name="B_Set", color="fc7100")
        self.temperature_plot_graph.plot_data_new(name="B_Out", color="ff00dd")
        self.temperature_plot_graph.plot_data_new(name="T_Set", color="17afeb")
        self.temperature_plot_graph.plot_data_new(name="T_Out", color="17d2eb")

    def updateTemperautre(self, info):
        if self.temp_start_time is None:
            self.temp_start_time = time.perf_counter()
            tiem_point = 0
        else:
            tiem_point = time.perf_counter() - self.temp_start_time
        temp_btm = int.from_bytes(info.content[6:8], byteorder="little") / 100
        temp_top = int.from_bytes(info.content[8:10], byteorder="little") / 100
        self.temperature_plot_graph.plot_data_update(0, tiem_point, temp_btm)
        self.temperature_plot_graph.plot_data_update(1, tiem_point, temp_top)
        if temp_btm != 128:
            self.temperautre_lbs[0].setText(f"{temp_btm:03.2f}℃")
            self.temperautre_lbs[0].setStyleSheet("")
        else:
            self.temperautre_lbs[0].setText("数据异常")
            self.temperautre_lbs[0].setStyleSheet("background-color: red;")
        if temp_top != 128:
            self.temperautre_lbs[1].setText(f"{temp_top:03.2f}℃")
            self.temperautre_lbs[1].setStyleSheet("")
        else:
            self.temperautre_lbs[1].setText("数据异常")
            self.temperautre_lbs[1].setStyleSheet("background-color: red;")
        self.temperature_plot_dg.setWindowTitle(
            f"温度记录 | 下 {self.temperautre_lbs[0].text()} | 上 {self.temperautre_lbs[1].text()} | 环境 {self.temperautre_wg_raw_lbs[-1].text()}"
        )

    def updateTemperautreRaw(self, info):
        payload_length = len(info.content) - 7
        if payload_length not in (36, 54, 94):
            logger.error(f"error temp raw info | {info}")
            return
        if self.temp_start_time is None:
            self.temp_start_time = time.perf_counter()
            time_point = 0
        else:
            time_point = time.perf_counter() - self.temp_start_time
        if payload_length == 94:
            temp_point_list = struct.unpack("f" * 10, info.content[6 + 54 : 6 + 94])
            self.temperature_B_PID_graph.plot_data_update(0, time_point, temp_point_list[1])
            self.temperature_B_PID_graph.plot_data_update(1, time_point, temp_point_list[2])
            self.temperature_B_PID_graph.plot_data_update(2, time_point, temp_point_list[3])
            self.temperature_B_PID_graph.plot_data_update(3, time_point, temp_point_list[4])
            self.temperature_T_PID_graph.plot_data_update(0, time_point, temp_point_list[6])
            self.temperature_T_PID_graph.plot_data_update(1, time_point, temp_point_list[7])
            self.temperature_T_PID_graph.plot_data_update(2, time_point, temp_point_list[8])
            self.temperature_T_PID_graph.plot_data_update(3, time_point, temp_point_list[9])
            self.temperature_plot_graph.plot_data_update(2, time_point, temp_point_list[0])
            self.temperature_plot_graph.plot_data_update(3, time_point, temp_point_list[1])
            self.temperature_plot_graph.plot_data_update(4, time_point, temp_point_list[5])
            self.temperature_plot_graph.plot_data_update(5, time_point, temp_point_list[6])
        for idx in range(9):
            temp_value = struct.unpack("f", info.content[6 + 4 * idx : 10 + 4 * idx])[0]
            if payload_length in (54, 94):
                adc_raw = struct.unpack("H", info.content[10 + 32 + 2 * idx : 12 + 32 + 2 * idx])[0]
                if adc_raw != 0:
                    r = 10 * (4095 - adc_raw) / adc_raw
                else:
                    r = 10 * 4095
                self.temperautre_wg_raw_lbs[idx].setText(f"#{idx + 1} {temp_value:.3f}℃")
                self.temperautre_wg_raw_lbs[idx].setToolTip(f"ADC {adc_raw} 电阻 {r:.6f} kΩ")
            elif payload_length == 36:
                self.temperautre_wg_raw_lbs[idx].setText(f"#{idx + 1} {temp_value:.3f}℃ A")
            self.temperature_raw_graph.plot_data_update(idx, time_point, temp_value)

    def on_out_flash_param_temp_read(self, event=None):
        data = (*(struct.pack("H", 0)), *(struct.pack("H", 3)))
        self._serialSendPack(0xDD, data)

    def on_out_flash_param_temp_write(self, event=None):
        data = []
        for idx, sp in enumerate(self.out_flash_param_temp_sps):
            for d in struct.pack("f", sp.value() * -1):
                data.append(d)
        for i in range(0, len(data), 224):
            sl = data[i : i + 224]
            start = [*struct.pack("H", i // 4)]
            num = [*struct.pack("H", len(sl) // 4)]
            self._serialSendPack(0xDD, start + num + sl)
        data = (0xFF, 0xFF)  # 保存参数 写入Flash
        self._serialSendPack(0xDD, data)
        for sp in self.out_flash_param_temp_sps:
            self._setColor(sp, nfg="red")
        QTimer.singleShot(500, partial(self.on_out_flash_param_temp_read, event=False))

    def updateMotorTrayPosition(self, info):
        position = info.content[6]
        if position == 0:
            self.motor_tray_position.setStyleSheet("background-color: red;")
            self.motor_tray_position.setText("故障")
        elif position == 1:
            self.motor_tray_position.setStyleSheet("background-color: green;")
            self.motor_tray_position.setText("检测位")
        elif position == 2:
            self.motor_tray_position.setStyleSheet("background-color: yellow;")
            self.motor_tray_position.setText("加样位")
        else:
            self.motor_tray_position.setStyleSheet("background-color: white;")
            self.motor_tray_position.setText("错误报文")

    def updateVersionLabel(self, info):
        self.version = f"{bytes2Float(info.content[6:10]):.3f}"
        self.version_lb.setText(f"版本: {self.version}")

    def updateVersionLabelBootloader(self, info):
        raw_bytes = info.content
        year, moth, day, ver = raw_bytes[6:10]
        self.version_bl_lb.setText(f"BL: {2000 + year}-{moth:02d}-{day:02d} V{ver:03d}")

    def udpateDeviceIDLabel(self, info):
        logger.debug(f"udpateDeviceIDLabel | {info.text}")
        raw_bytes = info.content
        if len(raw_bytes) < 37:
            logger.error(f"udpateDeviceIDLabel no enough length | {len(raw_bytes)} | {info.text}")
            return
        self.device_id = "-".join([f"{struct.unpack('>I', raw_bytes[6 + 4 * i : 10 + 4 * i])[0]:08X}" for i in range(3)])
        self.sample_db.device_id = self.device_id
        self.device_id_lb.setText(f"ID: {self.device_id}")
        date_str = f'{raw_bytes[26: 37].decode("ascii", errors="ignore")} - {raw_bytes[18: 26].decode("ascii", errors="ignore")}'
        self.device_datetime = datetime.strptime(date_str, "%b %d %Y - %H:%M:%S")
        logger.debug(f"get datetime obj | {self.device_datetime}")
        self.version_lb.setToolTip(f"V{self.version}.{datetime.strftime(self.device_datetime, '%Y%m%d.%H%M%S')}")

    def onSampleDataBase2Excel(self, event):
        fd = QFileDialog()
        file_path, _ = fd.getSaveFileName(filter="Excel 工作簿 (*.xlsx)", directory=os.path.join(self.last_falsh_save_dir, f"采样数据-{self.device_id}-s0n10.xlsx"))
        if not file_path:
            return
        logger.debug(f"onSampleDataBase2Excel | {file_path}")
        rmo = DB_EXCEL_PATH_RE.search(file_path)
        if rmo is None or len(rmo.groups()) != 2:
            start = 0
            num = 2 ** 32
        else:
            gr = rmo.groups()
            start = int(gr[0])
            num = int(gr[1]) if int(gr[1]) > 0 else 2 ** 32
        _, fault = dump_sample(self.sample_db.iter_all_data(start, num), file_path)
        if fault:
            msg = ModernMessageBox(self)
            msg.setTextInteractionFlags(Qt.TextSelectableByMouse)
            msg.setIcon(QMessageBox.Warning)
            msg.setWindowTitle(f"保存失败 {file_path}")
            msg.setText(str(fault))
            msg.setDetailedText(repr(fault))
            msg.exec_()

    def onSampleSetChanged(self, item_idx, w_idx):
        logger.debug(f"onSampleSetChanged | item_idx {item_idx} | w_idx {w_idx}")
        ssi = SAMPLE_SET_INFOS[item_idx]
        self.matplot_conf_houhou_cs[w_idx].blockSignals(True)
        self.matplot_conf_wavelength_cs[w_idx].blockSignals(True)
        self.matplot_conf_point_sps[w_idx].blockSignals(True)
        self.matplot_conf_houhou_cs[w_idx].setCurrentIndex(ssi.method.value)
        self.matplot_conf_wavelength_cs[w_idx].setCurrentIndex(ssi.wave.value - 1)
        self.matplot_conf_point_sps[w_idx].setValue(ssi.points_num)
        self.matplot_conf_houhou_cs[w_idx].blockSignals(False)
        self.matplot_conf_wavelength_cs[w_idx].blockSignals(False)
        self.matplot_conf_point_sps[w_idx].blockSignals(False)

    def onSampleSubChanged(self, value, w_idx):
        for i, ssi in enumerate(SAMPLE_SET_INFOS):
            if (
                self.matplot_conf_houhou_cs[w_idx].currentIndex() == ssi.method.value
                and self.matplot_conf_wavelength_cs[w_idx].currentIndex() == (ssi.wave.value - 1)
                and self.matplot_conf_point_sps[w_idx].value() == ssi.points_num
            ):
                self.matplot_conf_set_cs[w_idx].blockSignals(True)
                self.matplot_conf_set_cs[w_idx].setCurrentIndex(i)
                self.matplot_conf_set_cs[w_idx].blockSignals(False)
                break
        else:
            self.matplot_conf_set_cs[w_idx].blockSignals(True)
            self.matplot_conf_set_cs[w_idx].setCurrentIndex(0)
            self.matplot_conf_set_cs[w_idx].blockSignals(False)

    def createBarcode(self):
        self.barcode_gb = QGroupBox("测试通道")
        barcode_ly = QGridLayout(self.barcode_gb)
        barcode_ly.setContentsMargins(3, 3, 3, 3)
        self.barcode_lbs = [QLabel("*" * 10) for i in range(7)]
        self.motor_scan_bts = [QPushButton(BARCODE_NAMES[i]) for i in range(7)]
        self.matplot_conf_houhou_cs = [QComboBox() for i in range(6)]
        self.matplot_conf_wavelength_cs = [QComboBox() for i in range(6)]
        self.matplot_conf_point_sps = [QSpinBox() for i in range(6)]
        self.matplot_conf_set_cs = [QComboBox() for i in range(6)]
        self.matplot_timer_lbs = [QLabel(f"TT{i}", maximumWidth=40) for i in range(6)]
        self.matplot_timer = QTimer(self)
        self.matplot_timer.timeout.connect(self.update_matplot_timer)
        self.matplot_timer_time_list = [time.time()] * 6
        self.barcode_scan_bt = QPushButton("扫码")
        self.barcode_scan_bt.setMaximumWidth(50)
        self.matplot_start_bt = QPushButton("测试(R)", shortcut="Ctrl+R")
        self.matplot_start_bt.setMaximumWidth(50)
        self.matplot_cancel_bt = QPushButton("取消(N)", shortcut="Ctrl+N")
        self.matplot_cancel_bt.setMaximumWidth(50)
        self.matplot_period_tv_cb = QCheckBox("&NL", maximumWidth=40)
        self.matplot_period_tv_cb.setTristate(True)
        self.matplot_period_tv_cb.stateChanged.connect(lambda x: self.matplot_period_tv_cb.setText(("&NL", "&PD", "&MX")[x]))
        self.lamp_bp_bt = QPushButton("BP", maximumWidth=30)
        self.lamp_bp_bt.clicked.connect(self.onLampBP)
        self.lamp_sl_bt = QPushButton("SL", maximumWidth=30)
        self.lamp_sl_bt.clicked.connect(self.onLampSL)
        self.lamp_ag_cb = QCheckBox("AG", maximumWidth=60)
        for i in range(7):
            self.motor_scan_bts[i].setMaximumWidth(45)
            barcode_ly.addWidget(self.motor_scan_bts[i], i, 0)
            barcode_ly.addWidget(self.barcode_lbs[i], i, 1)
            if i < 6:
                self.matplot_conf_houhou_cs[i].addItems(METHOD_NAMES)
                self.matplot_conf_houhou_cs[i].setMaximumWidth(75)
                self.matplot_conf_houhou_cs[i].setCurrentIndex(1)
                self.matplot_conf_wavelength_cs[i].addItems(WAVE_NAMES)
                if i > 0:
                    self.matplot_conf_wavelength_cs[i].setMaxCount(2)
                self.matplot_conf_wavelength_cs[i].setMaximumWidth(60)
                self.matplot_conf_wavelength_cs[i].setCurrentIndex(0)
                self.matplot_conf_point_sps[i].setRange(0, 120)
                self.matplot_conf_point_sps[i].setMaximumWidth(60)
                self.matplot_conf_point_sps[i].setValue(6)
                for j, ssi in enumerate(SAMPLE_SET_INFOS):
                    self.matplot_conf_set_cs[i].addItem(ssi.short_name)
                    if i > 0 and ssi.wave.value == 3:
                        self.matplot_conf_set_cs[i].model().item(j).setEnabled(False)
                    self.matplot_conf_set_cs[i].setItemData(
                        j,
                        f"{ssi.full_name} | {WAVE_NAMES[ssi.wave.value - 1]} | {METHOD_NAMES[ssi.method.value]} | {ssi.points_num / 6:.1f} min",
                        Qt.ToolTipRole,
                    )
                self.matplot_conf_set_cs[i].currentIndexChanged.connect(partial(self.onSampleSetChanged, w_idx=i))
                self.matplot_conf_houhou_cs[i].currentIndexChanged.connect(partial(self.onSampleSubChanged, w_idx=i))
                self.matplot_conf_wavelength_cs[i].currentIndexChanged.connect(partial(self.onSampleSubChanged, w_idx=i))
                self.matplot_conf_point_sps[i].valueChanged.connect(partial(self.onSampleSubChanged, w_idx=i))

                barcode_ly.addWidget(self.matplot_conf_houhou_cs[i], i, 2)
                barcode_ly.addWidget(self.matplot_conf_wavelength_cs[i], i, 3)
                barcode_ly.addWidget(self.matplot_conf_point_sps[i], i, 4)
                barcode_ly.addWidget(self.matplot_conf_set_cs[i], i, 5)
                barcode_ly.addWidget(self.matplot_timer_lbs[i], i, 6)
            else:
                temp_ly = QHBoxLayout()
                temp_ly.setSpacing(3)
                temp_ly.setContentsMargins(3, 3, 3, 3)
                temp_ly.addWidget(self.barcode_scan_bt)
                temp_ly.addWidget(self.matplot_start_bt)
                temp_ly.addWidget(self.matplot_cancel_bt)
                temp_ly.addWidget(self.matplot_period_tv_cb)
                temp_ly.addWidget(self.lamp_bp_bt)
                temp_ly.addWidget(self.lamp_sl_bt)
                temp_ly.addWidget(self.lamp_ag_cb)
                barcode_ly.addLayout(temp_ly, i, 2, 1, 6)
        self.sample_record_idx_sp = QSpinBox()
        self.sample_record_idx_sp.setRange(0, 99999999)
        self.sample_record_idx_sp.setMaximumWidth(75)
        self.sample_record_label = QLabel("***********")
        matplot_record_ly = QHBoxLayout()
        matplot_record_ly.setContentsMargins(3, 1, 3, 1)
        matplot_record_ly.setSpacing(1)
        self.sample_record_pre_bt = QPushButton(f"序号[{self.sample_db.get_label_cnt() - 1}]")
        self.sample_record_pre_bt.clicked.connect(self.onSampleDataBase2Excel)
        self.sample_record_pre_bt.setMaximumWidth(60)
        matplot_record_ly.addWidget(self.sample_record_pre_bt, 0)
        matplot_record_ly.addWidget(self.sample_record_idx_sp, 0)
        matplot_record_ly.addWidget(QVLine(), 0)
        matplot_record_ly.addWidget(self.sample_record_label)
        barcode_ly.addLayout(matplot_record_ly, 7, 0, 1, 6)
        self.stary_test_bt_flag = 0
        self.stary_test_bt = QPushButton("杂散光", maximumWidth=75, enabled=False, clicked=self.onStaryTest)
        self.out_flash_param_parse_bt = QPushButton("校正曲线", clicked=self.onOutFlashParamCC_parse, maximumWidth=90)
        barcode_ly.addWidget(self.stary_test_bt, 7, 4, 1, 1)
        barcode_ly.addWidget(self.out_flash_param_parse_bt, 7, 5, 1, 1)
        self.barcode_scan_bt.clicked.connect(self.onBarcodeScan)
        self.matplot_start_bt.clicked.connect(self.onMatplotStart)
        self.matplot_cancel_bt.clicked.connect(self.onMatplotCancel)
        for i in range(7):
            self.motor_scan_bts[i].clicked.connect(partial(self.onMotorScan, idx=i))
        self.sample_record_idx_sp.valueChanged.connect(self.onSampleRecordReadBySp)
        self.sample_record_label.mousePressEvent = self.onSampleLabelClick

    def onStaryTest(self, event):
        self._serialSendPack(0xDC, (0,))
        self.stary_test_bt_flag = 0
        self.stary_test_bt.setDisabled(True)

    def initBarcodeScan(self):
        for lb in self.barcode_lbs:
            lb.setText(("*" * 10))

    def onBarcodeScan(self, event):
        self.initBarcodeScan()
        self._serialSendPack(0x01)

    def updateBarcode(self, info):
        channel = info.content[6]
        if channel <= 0 or channel > 7:
            logger.error("barcode scan result chanel index over range")
            return
        length = info.content[7]
        if length == 0:
            text = "None"
        else:
            try:
                raw_bytes = info.content[8:-1]
                text = raw_bytes.decode("ascii")
            except Exception:
                text = bytesPuttyPrint(raw_bytes)
        chunks, chunk_size = len(text) // 13, 13
        tip = "\n".join(text[i : i + chunk_size] for i in range(0, chunks, chunk_size))
        self.barcode_lbs[channel - 1].setToolTip(tip)
        if len(text) > 13:
            self.barcode_lbs[channel - 1].setText(f"{text[:10]}...")
        else:
            self.barcode_lbs[channel - 1].setText(text)

    def createMotor(self):
        self.motor_gb = QGroupBox("电机控制")
        motor_ly = QGridLayout(self.motor_gb)

        self.motor_heater_up_bt = QPushButton("上加热体抬升(U)", shortcut="Ctrl+U")
        self.motor_heater_up_bt.setMaximumWidth(120)
        self.motor_heater_down_bt = QPushButton("上加热体下降(D)", shortcut="Ctrl+D")
        self.motor_heater_down_bt.setMaximumWidth(120)

        self.motor_white_pd_bt = QPushButton("白板PD位置(P)", shortcut="Ctrl+P")
        self.motor_white_pd_bt.setMaximumWidth(120)
        self.motor_white_od_bt = QPushButton("白物质位置(W)", shortcut="Ctrl+W")
        self.motor_white_od_bt.setMaximumWidth(120)

        self.motor_tray_in_bt = QPushButton("托盘进仓(A-1)", shortcut="Alt+1")
        self.motor_tray_in_bt.setMaximumWidth(120)
        self.motor_tray_scan_bt = QPushButton("托盘扫码(A-2)", shortcut="Alt+2")
        self.motor_tray_scan_bt.setMaximumWidth(120)
        self.motor_tray_out_bt = QPushButton("托盘出仓(A-3)", shortcut="Alt+3")
        self.motor_tray_out_bt.setMaximumWidth(120)
        self.motor_tray_debug_cb = QPushButton("电机调试")
        self.motor_tray_debug_cb.setMaximumWidth(120)

        motor_ly.addWidget(self.motor_heater_up_bt, 0, 0)
        motor_ly.addWidget(self.motor_heater_down_bt, 1, 0)
        motor_ly.addWidget(self.motor_white_pd_bt, 0, 1)
        motor_ly.addWidget(self.motor_white_od_bt, 1, 1)
        motor_ly.addWidget(self.motor_tray_in_bt, 0, 2)
        motor_ly.addWidget(self.motor_tray_out_bt, 1, 2)
        motor_ly.addWidget(self.motor_tray_scan_bt, 0, 3)
        motor_ly.addWidget(self.motor_tray_debug_cb, 1, 3)

        self.motor_heater_up_bt.clicked.connect(self.onMotorHeaterUp)
        self.motor_heater_down_bt.clicked.connect(self.onMotorHeaterDown)
        self.motor_white_pd_bt.clicked.connect(self.onMotorWhitePD)
        self.motor_white_od_bt.clicked.connect(self.onMotorWhiteOD)
        self.motor_tray_in_bt.clicked.connect(self.onMotorTrayIn)
        self.motor_tray_scan_bt.clicked.connect(self.onMotorTrayScan)
        self.motor_tray_out_bt.clicked.connect(self.onMotorTrayOut)
        self.motor_tray_debug_cb.clicked.connect(self.onMotorDebug)

        self.motor_debug_dg = QDialog(self)
        self.motor_debug_dg.setWindowTitle("电机调试")
        self.motor_debug_ly = QVBoxLayout(self.motor_debug_dg)

        scan_ly = QHBoxLayout()
        scan_ly.setContentsMargins(3, 3, 3, 3)
        self.motor_debug_scan_status_lb = QLabel("0x0000")
        self.motor_debug_scan_pos_lb = QLabel("000000")
        self.motor_debug_scan_bg = QButtonGroup(self.motor_debug_dg)
        self.motor_debug_scan_fwd = QRadioButton("左")
        self.motor_debug_scan_fwd.setChecked(True)
        self.motor_debug_scan_rev = QRadioButton("右")
        self.motor_debug_scan_bg.addButton(self.motor_debug_scan_fwd)
        self.motor_debug_scan_bg.addButton(self.motor_debug_scan_rev)
        self.motor_debug_scan_dis_sl = QSlider(Qt.Horizontal)
        self.motor_debug_scan_dis_sl.setMinimumWidth(180)
        self.motor_debug_scan_dis_sl.setRange(-10000, 10000)
        self.motor_debug_scan_dis_sl.setValue(0)
        self.motor_debug_scan_dis = QSpinBox()
        self.motor_debug_scan_dis.setRange(0, 10000)
        self.motor_debug_scan_dis.setMaximumWidth(60)
        self.motor_debug_scan_bt = QPushButton("下发")
        self.motor_debug_scan_bt.setAutoDefault(False)
        self.motor_debug_scan_bt.setDefault(False)
        self.motor_debug_scan_bt.setMaximumWidth(60)

        self.motor_debug_aging_gb = QGroupBox("电机老化")
        self.motor_debug_aging_gb.setCheckable(True)
        self.motor_debug_aging_gb.setChecked(False)
        self.motor_debug_aging_gb.clicked.connect(self.onMotorDebugAgingGB)
        self.motor_debug_aging_bg = QButtonGroup()
        motor_debug_aging_ly = QHBoxLayout(self.motor_debug_aging_gb)
        self.motor_debug_scan_aging_bt = QRadioButton("扫码电机")
        self.motor_debug_tray_aging_bt = QRadioButton("托盘电机")
        self.motor_debug_heater_aging_bt = QRadioButton("加热体电机")
        self.motor_debug_white_aging_bt = QRadioButton("白板电机")
        self.motor_debug_all_aging_bt = QRadioButton("全部电机")
        self.motor_debug_aging_bg.addButton(self.motor_debug_tray_aging_bt)
        self.motor_debug_aging_bg.addButton(self.motor_debug_scan_aging_bt)
        self.motor_debug_aging_bg.addButton(self.motor_debug_heater_aging_bt)
        self.motor_debug_aging_bg.addButton(self.motor_debug_white_aging_bt)
        self.motor_debug_aging_bg.addButton(self.motor_debug_all_aging_bt)
        motor_debug_aging_ly.addWidget(self.motor_debug_scan_aging_bt)
        motor_debug_aging_ly.addWidget(self.motor_debug_tray_aging_bt)
        motor_debug_aging_ly.addWidget(self.motor_debug_heater_aging_bt)
        motor_debug_aging_ly.addWidget(self.motor_debug_white_aging_bt)
        motor_debug_aging_ly.addWidget(self.motor_debug_all_aging_bt)
        self.motor_debug_aging_bg.buttonToggled.connect(self.onMotorDebugAging)
        self.motor_debug_aging_bts = (
            self.motor_debug_scan_aging_bt,
            self.motor_debug_tray_aging_bt,
            self.motor_debug_heater_aging_bt,
            self.motor_debug_white_aging_bt,
            self.motor_debug_all_aging_bt,
        )

        scan_ly.addWidget(QLabel("扫码电机"))
        scan_ly.addWidget(QVLine())
        scan_ly.addWidget(self.motor_debug_scan_status_lb)
        scan_ly.addWidget(QVLine())
        scan_ly.addWidget(self.motor_debug_scan_pos_lb)
        scan_ly.addWidget(QVLine())
        scan_ly.addWidget(self.motor_debug_scan_dis_sl)
        scan_ly.addWidget(QVLine())
        scan_ly.addWidget(self.motor_debug_scan_fwd)
        scan_ly.addWidget(self.motor_debug_scan_rev)
        scan_ly.addWidget(QLabel("距离"))
        scan_ly.addWidget(self.motor_debug_scan_dis)
        scan_ly.addWidget(self.motor_debug_scan_bt)
        self.motor_debug_ly.addLayout(scan_ly)

        tray_ly = QHBoxLayout()
        tray_ly.setContentsMargins(3, 3, 3, 3)
        self.motor_debug_tray_status_lb = QLabel("0x0000")
        self.motor_debug_tray_pos_lb = QLabel("000000")
        self.motor_debug_tray_bg = QButtonGroup(self.motor_debug_dg)
        self.motor_debug_tray_fwd = QRadioButton("进")
        self.motor_debug_tray_fwd.setChecked(True)
        self.motor_debug_tray_rev = QRadioButton("出")
        self.motor_debug_tray_bg.addButton(self.motor_debug_tray_fwd)
        self.motor_debug_tray_bg.addButton(self.motor_debug_tray_rev)
        self.motor_debug_tray_dis_sl = QSlider(Qt.Horizontal)
        self.motor_debug_tray_dis_sl.setMinimumWidth(180)
        self.motor_debug_tray_dis_sl.setRange(-10000, 10000)
        self.motor_debug_tray_dis_sl.setValue(0)
        self.motor_debug_tray_dis = QSpinBox()
        self.motor_debug_tray_dis.setRange(0, 10000)
        self.motor_debug_tray_dis.setMaximumWidth(60)
        self.motor_debug_tray_bt = QPushButton("下发")
        self.motor_debug_tray_bt.setAutoDefault(False)
        self.motor_debug_tray_bt.setDefault(False)
        self.motor_debug_tray_bt.setMaximumWidth(60)

        tray_ly.addWidget(QLabel("托盘电机"))
        tray_ly.addWidget(QVLine())
        tray_ly.addWidget(self.motor_debug_tray_status_lb)
        tray_ly.addWidget(QVLine())
        tray_ly.addWidget(self.motor_debug_tray_pos_lb)
        tray_ly.addWidget(QVLine())
        tray_ly.addWidget(self.motor_debug_tray_dis_sl)
        tray_ly.addWidget(QVLine())
        tray_ly.addWidget(self.motor_debug_tray_fwd)
        tray_ly.addWidget(self.motor_debug_tray_rev)
        tray_ly.addWidget(QLabel("距离"))
        tray_ly.addWidget(self.motor_debug_tray_dis)
        tray_ly.addWidget(self.motor_debug_tray_bt)
        self.motor_debug_ly.addLayout(tray_ly)

        self.motor_debug_ly.addWidget(self.motor_debug_aging_gb)

        self.motor_debug_scan_bg.buttonToggled.connect(self.onMotorDebugScanBGChanged)
        self.motor_debug_tray_bg.buttonToggled.connect(self.onMotorDebugTrayBGChanged)
        self.motor_debug_scan_dis.valueChanged.connect(self.onMotorDebugScanSpinBoxChanged)
        self.motor_debug_tray_dis.valueChanged.connect(self.onMotorDebugTraySpinBoxChanged)
        self.motor_debug_scan_dis_sl.valueChanged.connect(self.onMotorDebugScanSliderChanged)
        self.motor_debug_tray_dis_sl.valueChanged.connect(self.onMotorDebugTraySliderChanged)
        self.motor_debug_scan_bt.clicked.connect(self.onMotorDebugScan)
        self.motor_debug_tray_bt.clicked.connect(self.onMotorDebugTray)
        self.motor_debug_dg = ModernDialog(self.motor_debug_dg, self)

    def onMotorHeaterUp(self, event):
        self._serialSendPack(0xD0, (2, 0))

    def onMotorHeaterDown(self, event):
        self._serialSendPack(0xD0, (2, 1))

    def onMotorWhitePD(self, event):
        self._serialSendPack(0xD0, (3, 0))

    def onMotorWhiteOD(self, event):
        self._serialSendPack(0xD0, (3, 1))

    def onMotorTrayIn(self, event):
        self._serialSendPack(0x05)

    def onMotorTrayScan(self, event):
        self._serialSendPack(0xD0, (2, 0))
        QTimer.singleShot(1500, lambda: self._serialSendPack(0xD0, (1, 1)))

    def onMotorTrayOut(self, event):
        self._serialSendPack(0x04)

    def onMotorDebug(self, event):
        self.motor_debug_dg.show()

    def onMotorDebugScanBGChanged(self, event):
        value = self.motor_debug_scan_dis.value()
        if self.motor_debug_scan_fwd.isChecked():
            value = value * -1
        elif self.motor_debug_scan_rev.isChecked():
            value = value
        else:
            return
        self.motor_debug_scan_dis_sl.setValue(value)

    def onMotorDebugTrayBGChanged(self, event):
        value = self.motor_debug_tray_dis.value()
        if self.motor_debug_tray_fwd.isChecked():
            value = value * -1
        elif self.motor_debug_tray_rev.isChecked():
            value = value
        else:
            return
        self.motor_debug_tray_dis_sl.setValue(value)

    def onMotorDebugScanSpinBoxChanged(self, event):
        if self.motor_debug_scan_fwd.isChecked():
            value = event * -1
        elif self.motor_debug_scan_rev.isChecked():
            value = event
        else:
            return
        self.motor_debug_scan_dis_sl.setValue(value)

    def onMotorDebugTraySpinBoxChanged(self, event):
        if self.motor_debug_tray_fwd.isChecked():
            value = event * -1
        elif self.motor_debug_tray_rev.isChecked():
            value = event
        else:
            return
        self.motor_debug_tray_dis_sl.setValue(value)

    def onMotorDebugScanSliderChanged(self, event):
        if event < 0:
            self.motor_debug_scan_fwd.setChecked(True)
        else:
            self.motor_debug_scan_rev.setChecked(True)
        self.motor_debug_scan_dis.setValue(abs(event))

    def onMotorDebugTraySliderChanged(self, event):
        if event < 0:
            self.motor_debug_tray_fwd.setChecked(True)
        else:
            self.motor_debug_tray_rev.setChecked(True)
        self.motor_debug_tray_dis.setValue(abs(event))

    def onMotorDebugScan(self, event):
        if self.motor_debug_scan_fwd.isChecked():
            ori = 1
        elif self.motor_debug_scan_rev.isChecked():
            ori = 0
        else:
            logger.error("error motor_debug_scan radio button status")
            return
        dis = self.motor_debug_scan_dis.value()
        data = struct.pack("=BBI", 0, ori, dis)
        self._serialSendPack(0xD0, data)

    def onMotorDebugTray(self, event):
        if self.motor_debug_tray_fwd.isChecked():
            ori = 1
        elif self.motor_debug_tray_rev.isChecked():
            ori = 0
        else:
            logger.error("error motor_debug_scan radio button status")
            return
        dis = self.motor_debug_tray_dis.value()
        data = struct.pack("=BBI", 1, ori, dis)
        self._serialSendPack(0xD0, data)

    def updateMotorDebug(self, info):
        raw_bytes = info.content
        idx = raw_bytes[6]
        status = struct.unpack("H", raw_bytes[7:9])[0]
        pos = struct.unpack("I", raw_bytes[9:13])[0]
        if idx == 0:
            self.motor_debug_scan_status_lb.setText(f"0x{status:04X}")
            self.motor_debug_scan_pos_lb.setText(f"{pos}")
        elif idx == 1:
            self.motor_debug_tray_status_lb.setText(f"0x{status:04X}")
            self.motor_debug_tray_pos_lb.setText(f"{pos}")
        elif idx == 2:
            pass
        elif idx == 3:
            cnt = struct.unpack("I", raw_bytes[7:11])[0]
            status = raw_bytes[11]
            if status != 0:
                logger.error(f"white motor run cnt | {cnt} | status {status}")
            else:
                logger.success(f"white motor run cnt | {cnt} | status {status}")

    def onMotorDebugAging(self, event):
        if event in self.motor_debug_aging_bts:
            idx = self.motor_debug_aging_bts.index(event)
            if not event.isChecked():
                self._serialSendPack(0xD0, (idx, 0xFF))
            else:
                self._serialSendPack(0xD0, (idx, 0xFE))

    def onMotorDebugAgingGB(self, event):
        self._serialSendPack(0xD0, (0xFF,))
        self.motor_debug_aging_bg.setExclusive(False)
        for bt in self.motor_debug_aging_bts:
            bt.setChecked(False)
        self.motor_debug_aging_bg.setExclusive(True)

    def onMotorScan(self, event=None, idx=0):
        logger.debug(f"click motor scan | event {event} | idx {idx}")
        self.barcode_lbs[idx].setText("*" * 10)
        self._serialSendPack(0xD0, (0, (1 << idx), (1 << idx)))

    def sample_record_parse_raw_data(self, total, raw_data, last_sd=None):
        data = []
        data_len = len(raw_data)
        log10_data = []
        if total == 0:
            logger.error(f"sample data total error | {total}")
            return data, log10_data
        if data_len / total == 2:
            for i in range(0, data_len, 2):
                data.append(struct.unpack("H", raw_data[i : i + 2])[0])
        elif data_len / total == 4:
            for i in range(0, data_len, 4):
                data.append(struct.unpack("I", raw_data[i : i + 4])[0])
            if last_sd:
                for i in range(0, data_len, 4):
                    ld = struct.unpack("I", last_sd.raw_data[i : i + 4])[0]
                    if data[i // 4] == 0 or ld / data[i // 4] <= 0:
                        log10_data.append(nan)
                    else:
                        log10_data.append(log10(ld / data[i // 4]) * 10000)
        elif data_len / total == 10:
            for i in range(total):
                data.append(struct.unpack("I", raw_data[i * 10 + 0 : i * 10 + 4])[0])
            for i in range(total):
                data.append(struct.unpack("I", raw_data[i * 10 + 4 : i * 10 + 8])[0])
            for i in range(total):
                data.append(struct.unpack("H", raw_data[i * 10 + 8 : i * 10 + 10])[0])
        elif data_len / total == 12:
            for i in range(total):
                data.append(struct.unpack("I", raw_data[i * 12 + 0 : i * 12 + 4])[0])
            for i in range(total):
                data.append(struct.unpack("I", raw_data[i * 12 + 4 : i * 12 + 8])[0])
            for i in range(total):
                data.append(struct.unpack("H", raw_data[i * 12 + 8 : i * 12 + 10])[0])
            for i in range(total):
                data.append(struct.unpack("H", raw_data[i * 12 + 10 : i * 12 + 12])[0])
        else:
            logger.error(f"sample data length error | {data_len} | {total}")
        return data, log10_data

    def sample_record_plot_by_index(self, idx):
        label = self.sample_db.get_label_by_index(idx)
        self.sample_record_current_label = label
        if label is None:
            logger.warning("hit the label limit")
            cnt = self.sample_db.get_label_cnt()
            self.sample_record_idx_sp.setRange(0, cnt - 1)
            self.sample_record_pre_bt.setText(f"序号[{cnt - 1}]")
            return
        # logger.debug(f"get Label {label}")
        # logger.debug(f"get sample datas | {label.sample_datas}")
        self.sample_record_label.setText(f"{label.name}")
        self.sample_record_label.setToolTip(f"{label.datetime}")
        self.plot_graph.clear_plot()
        for i in range(6):
            self.matplot_conf_houhou_cs[i].setCurrentIndex(0)
            self.matplot_conf_wavelength_cs[i].setCurrentIndex(0)
            self.matplot_conf_point_sps[i].setValue(0)
        for sd in label.sample_datas:
            channel_idx = sd.channel - 1
            data = self.sample_record_parse_raw_data(sd.total, sd.raw_data)[0]
            self.temp_saple_data[channel_idx] = data
            self.plot_graph.plot_data_new(data=data, name=f"B-{channel_idx + 1}")
            self.matplot_conf_houhou_cs[channel_idx].setCurrentIndex(sd.method.value)
            self.matplot_conf_wavelength_cs[channel_idx].setCurrentIndex(sd.wave.value - 1)
            self.matplot_conf_point_sps[channel_idx].setValue(sd.total)
            set_info_name_list = [i.short_name for i in SAMPLE_SET_INFOS]
            if sd.set_info in set_info_name_list:
                name_idx = set_info_name_list.index(sd.set_info)
                if name_idx == 0:
                    continue
                self.matplot_conf_set_cs[channel_idx].blockSignals(True)
                self.matplot_conf_set_cs[channel_idx].setCurrentIndex(name_idx)
                self.matplot_conf_set_cs[channel_idx].blockSignals(False)

    def correctSampleData(self, channel, wave, origin_data_list):
        result = []
        # data = self._getOutFlashParamCC_Data()
        if not self.device_id:
            data = load_CC("data/flash.xlsx")
        else:
            data = load_CC(f"data/flash_{self.device_id}.xlsx")
            if data is None:
                data = load_CC("data/flash.xlsx")
        logger.debug(f"channel {repr(channel)} | wave {repr(wave)} | data | {data}")
        if data is None:
            data = self._getOutFlashParamCC_Data()
        for d in data:
            if isinstance(d, ILLU_CC_DataInfo) and d.wave == int(wave):
                channel_points = d.channel_pointses[channel - 1]
                standard_points = d.standard_pointses[channel - 1]
                result = [point_line_equation_map(channel_points, standard_points, origin_data) for origin_data in origin_data_list]
                logger.info(f"{channel_points} -> {standard_points} | {result}")
                break
        else:
            logger.warning(f"return origin data | {origin_data_list}")
            result = [i for i in origin_data_list]
        return result

    def onSampleLabelClick(self, event):
        def data_format_list(data, t=int):
            if t is int:
                result = ", ".join(f"{i:#5d}" for i in data)
            elif t is float:
                result = ", ".join(f"{i:7.2f}" for i in data)
            if len(result) > 250:
                result = result[:147] + "..."
            return result

        label = self.sample_record_current_label
        if label is None:
            return
        sds = sorted(label.sample_datas, key=lambda x: x.channel)
        logger.debug(f"sds length | {len(sds)}")
        msg = ModernMessageBox(self)
        msg.setTextInteractionFlags(Qt.TextSelectableByMouse)
        msg.setIcon(QMessageBox.Information)
        msg.setWindowTitle(f"采样数据 | {label.datetime} | {label.name}")
        data_infos = []
        last_sd = None
        for i, sd in enumerate(sds):
            if last_sd and last_sd.channel == sd.channel and len(last_sd.raw_data) / sd.total == 4:
                if label.name.startswith("Lamp BP") and i % 2 == 0:
                    last_sd = None
                real_data, log10_data = self.sample_record_parse_raw_data(sd.total, sd.raw_data, last_sd)
            else:
                real_data, log10_data = self.sample_record_parse_raw_data(sd.total, sd.raw_data)
            if len(real_data) == 0:
                continue
            if last_sd and last_sd.channel != sd.channel:
                data_infos.append("=" * 24)
            last_sd = sd
            if label.name.startswith("Lamp BP"):
                g = i // (len(sds) // 13)
                wave_name = WAVE_NAMES[(g - 3) % 2 if g > 2 else g % 3]
            elif label.name.startswith("Correct "):
                wave_name = WAVE_NAMES[(i - 3) % 2] if i >= 3 else WAVE_NAMES[i % 3]
            else:
                wave_name = sd.wave.name[-3:]
            if len(sd.raw_data) / sd.total == 12:
                data_infos.append(f"通道 {sd.channel} | {wave_name} | {data_format_list(real_data)}")
                continue
            ys = np.array(real_data, dtype=np.int32)
            std = np.std(ys)
            mean = np.mean(ys)
            cv = std / mean
            if len(real_data) >= 12:
                ys_6 = np.array(sorted(real_data[6:])[1:-1], dtype=np.int32)
                std_6 = np.std(ys_6)
                mean_6 = np.mean(ys_6)
                cv_6 = std_6 / mean_6
                data_infos.append(
                    f"通道 {sd.channel} | {wave_name} | {data_format_list(real_data)} | (cv = {cv:.4f}, std = {std:.4f}, mean = {mean:.4f}) | "
                    f"(cv_6 = {cv_6:.4f}, std_6 = {std_6:.4f}, mean_6 = {mean_6:.4f})"
                )
            else:
                data_infos.append(f"通道 {sd.channel} | {wave_name} | {data_format_list(real_data)} | (cv = {cv:.4f}, std = {std:.4f}, mean = {mean:.4f})")
            if log10_data:
                ys = np.array(log10_data, dtype=np.float)
                std = np.std(ys)
                mean = np.mean(ys)
                cv = std / mean
                log10_data_c = self.correctSampleData(sd.channel, wave_name, log10_data)
                data_infos.append(
                    f"OD_o {sd.channel} | {wave_name} | {data_format_list(log10_data, float)} | (cv = {cv:.4f}, std = {std:.4f}, mean = {mean:.4f})\n"
                    f"OD_c {sd.channel} | {wave_name} | {data_format_list(log10_data_c, float)}"
                )
        logger.debug(f"data_infos length | {len(data_infos)}")
        # https://stackoverflow.com/a/10977872
        font = QFont("Consolas")
        font.setFixedPitch(True)
        msg.setFont(font)
        msg.setText("\n".join(data_infos))
        detail_head_text = f"时间 | {label.datetime}\n标签 | {label.name}\n版本 | {label.version}\n容量 | {len(sds)}"
        detail_text = f"\n{'=' * 24}\n".join(
            (
                f"通道 {sd.channel} | {sd.datetime} | {METHOD_NAMES[sd.method.value]} | {WAVE_NAMES[sd.wave.value - 1]} | 点数 {sd.total}\n"
                f"{self.sample_record_parse_raw_data(sd.total, sd.raw_data)[0]}\n{bytesPuttyPrint(sd.raw_data)}"
            )
            for sd in sds
        )
        msg.setDetailedText(f"{detail_head_text}\n{'*' * 40}\n{detail_text}")
        # https://stackoverflow.com/a/50549396
        if isinstance(msg, QMessageBox):
            layout = msg.layout()
            layout.addItem(QSpacerItem(1000, 0, QSizePolicy.Minimum, QSizePolicy.Expanding), layout.rowCount(), 0, 1, layout.columnCount())
        msg.exec_()

    def onSampleRecordReadBySp(self, value):
        self.sample_record_plot_by_index(value)

    def updateSampleLED(self, info):
        for i, sp in enumerate(self.sample_led_sps):
            sp.setValue(int.from_bytes(info.content[6 + i * 2 : 8 + i * 2], byteorder="little"))
            self._setColor(sp)

    def onSampleLED_Read(self, event=None):
        self._serialSendPack(0x32)

    def updateStaryOffset(self, info):
        payload = info.content[6:-1]
        num = len(payload) // 4
        if num != 18:
            logger.error(f"stary data length error | {info.text}")
            return
        c_data = ["             610      550      405"]
        for i in range(6):
            l_data = []
            for j in range(3):
                start = 12 * i + 4 * j
                v = struct.unpack("I", payload[start : start + 4])[0]
                l_data.append(f"{v:>6d}")
            m = " ".join(l_data)
            c = f"通道-{i + 1}: {m}"
            logger.success(f"杂散光数据 | {c}")
            c_data.append(c)
        self.stary_test_bt.setToolTip("\n".join(c_data))

    def keyPressEvent(self, event):
        key = event.key()
        if not self.sample_led_write_bt.isEnabled():
            if (
                (key == Qt.Key_Up and self.sample_led_write_bt_flag in (0x00, 0x01))
                or (key == Qt.Key_Down and self.sample_led_write_bt_flag in (0x03, 0x07))
                or (key == Qt.Key_Left and self.sample_led_write_bt_flag in (0x0F, 0x1F))
                or (key == Qt.Key_Right and self.sample_led_write_bt_flag in (0x3F, 0x7F))
            ):
                self.sample_led_write_bt_flag = (self.sample_led_write_bt_flag << 1) + 1
                if self.sample_led_write_bt_flag == 0xFF:
                    self.sample_led_write_bt.setEnabled(True)
            else:
                self.sample_led_write_bt_flag = 0
            logger.debug(f"keyPressEvent | 0X{key:x} | {self.sample_led_write_bt_flag:08b}")
        if not self.stary_test_bt.isEnabled():
            if (
                (key == Qt.Key_Down and self.stary_test_bt_flag in (0x00, 0x01))
                or (key == Qt.Key_Up and self.stary_test_bt_flag in (0x03, 0x07))
                or (key == Qt.Key_Right and self.stary_test_bt_flag in (0x0F, 0x1F))
                or (key == Qt.Key_Left and self.stary_test_bt_flag in (0x3F, 0x7F))
            ):
                self.stary_test_bt_flag = (self.stary_test_bt_flag << 1) + 1
                if self.stary_test_bt_flag == 0xFF:
                    self.stary_test_bt.setEnabled(True)
            else:
                self.stary_test_bt_flag = 0

    def onSampleLED_Write(self, event=None):
        data = bytearray()
        for idx, sp in enumerate(self.sample_led_sps):
            self._setColor(sp, nfg="red")
            data += struct.pack("H", sp.value())
        self._serialSendPack(0x33, data)
        QTimer.singleShot(800, self.onSampleLED_Read)

    def onSample_White_Magnify(self, event=None):
        self.sample_white_magnify_qd.show()

    def updateSampleWhiteMagnify(self, info):
        for i, sp in enumerate(self.sample_white_magnify_sps):
            sp.setValue(struct.unpack("f", info.content[6 + i * 4 : 10 + i * 4])[0])
            self._setColor(sp)

    def onSample_White_Magnify_Read(self, event=None):
        self._serialSendPack(0x37)

    def onSample_White_Magnify_Write(self, event=None):
        data = bytearray()
        for idx, sp in enumerate(self.sample_white_magnify_sps):
            self._setColor(sp, nfg="red")
            data += struct.pack("f", sp.value())
        self._serialSendPack(0x38, data)
        QTimer.singleShot(800, self.onSample_White_Magnify_Read)

    def create_Sample_LED(self):
        self.sample_led_gb = QGroupBox("采样板LED")
        sample_led_ly = QVBoxLayout(self.sample_led_gb)
        self.sample_led_sps = [QSpinBox(minimum=10, maximum=65535, maximumWidth=60) for _ in range(3)]
        sp_ly = QHBoxLayout()
        for idx, sp in enumerate(self.sample_led_sps):
            sp_ly.addWidget(sp)
        bt_ly = QHBoxLayout()
        self.sample_led_read_bt = QPushButton("读取", clicked=self.onSampleLED_Read)
        self.sample_led_write_bt = QPushButton("写入", clicked=self.onSampleLED_Write, enabled=False)
        self.sample_led_write_bt_flag = 0
        self.sample_write_magnify_bt = QPushButton("放大倍数", clicked=self.onSample_White_Magnify)
        bt_ly.addWidget(self.sample_led_read_bt)
        bt_ly.addWidget(self.sample_led_write_bt)
        bt_ly.addWidget(self.sample_write_magnify_bt)
        sample_led_ly.addLayout(sp_ly)
        sample_led_ly.addLayout(bt_ly)
        self.sample_white_magnify_qd = QDialog(self)
        self.sample_white_magnify_qd.setWindowTitle("采集板白板PD放大倍数")
        sample_white_magnify_qb_ly = QVBoxLayout(self.sample_white_magnify_qd)
        self.sample_white_magnify_sps = []
        for i in range(3):
            temp_ly = QHBoxLayout()
            temp_ly.addWidget(QLabel(WAVE_NAMES[i]))
            for j in range(6):
                sp = QDoubleSpinBox(minimum=0.1, maximum=10, value=1.0, singleStep=0.5)
                self.sample_white_magnify_sps.append(sp)
                temp_ly.addWidget(sp)
            sample_white_magnify_qb_ly.addLayout(temp_ly)
        temp_ly = QHBoxLayout()
        self.sample_white_magnify_r_bt = QPushButton("读取", clicked=self.onSample_White_Magnify_Read)
        self.sample_white_magnify_w_bt = QPushButton("写入", clicked=self.onSample_White_Magnify_Write)
        temp_ly.addWidget(self.sample_white_magnify_r_bt)
        temp_ly.addWidget(self.sample_white_magnify_w_bt)
        sample_white_magnify_qb_ly.addLayout(temp_ly)

    def createSerial(self):
        self.serial_gb = QGroupBox("串口")
        serial_ly = QGridLayout(self.serial_gb)
        serial_ly.setContentsMargins(3, 3, 3, 3)
        serial_ly.setSpacing(0)
        self.serial_switch_bt = QPushButton("打开串口")
        self.serial_refresh_bt = QPushButton("刷新(F5)", shortcut="F5")
        self.serial_post_co = QComboBox()
        self.serialRefreshPort()
        serial_ly.addWidget(self.serial_post_co, 0, 0, 1, 1)
        serial_ly.addWidget(self.serial_refresh_bt, 0, 1, 1, 1)
        serial_ly.addWidget(self.serial_switch_bt, 1, 0, 1, 2)
        self.serial_refresh_bt.clicked.connect(self.onSerialRefresh)
        self.serial_switch_bt.setCheckable(True)
        self.serial_switch_bt.clicked.connect(self.onSerialSwitch)
        self.sample_record_lable_name = None

    def serialRefreshPort(self):
        self.serial_post_co.clear()
        for i, serial_port in enumerate(serial.tools.list_ports.comports()):
            self.serial_post_co.addItem(f"{serial_port.device}")
            self.serial_post_co.setItemData(i, serial_port.description, Qt.ToolTipRole)
        if self.serial_post_co.count() == 0:
            self.serial_post_co.addItem(f"{'None':^20s}")

    def onSerialRefresh(self, event):
        self.serialRefreshPort()

    def onSerialSwitch(self, event):
        logger.debug(f"serial switch button event | {event}")
        if event is True:
            old_port = self.serial.port
            self.serial.port = self.serial_post_co.currentText()
            try:
                self.serial.open()
            except (serial.SerialException, Exception):
                exctype, value = sys.exc_info()[:2]
                trace_back_text = stackprinter.format()
                self.onSerialWorkerError((exctype, value, trace_back_text))
                return
            self.serial_post_co.setEnabled(False)
            self.serial_refresh_bt.setEnabled(False)
            self.serial_switch_bt.setText("关闭串口")
            self._clearTaskQueue()
            self.serial_recv_worker = SerialRecvWorker(self.serial, self.henji_queue, QMutex())
            self.serial_recv_worker.signals.finished.connect(self.onSerialWorkerFinish)
            self.serial_recv_worker.signals.error.connect(self.onSerialWorkerError)
            self.serial_recv_worker.signals.serial_statistic.connect(self.onSerialStatistic)
            self.serial_recv_worker.signals.result.connect(self.onSerialRecvWorkerResult)
            self.threadpool.start(self.serial_recv_worker)

            self.serial_send_worker = SerialSendWorker(self.serial, self.task_queue, self.henji_queue, self.serial_recv_worker)
            self.serial_send_worker.signals.finished.connect(self.onSerialWorkerFinish)
            self.serial_send_worker.signals.error.connect(self.onSerialWorkerError)
            self.serial_send_worker.signals.serial_statistic.connect(self.onSerialStatistic)
            self.serial_send_worker.signals.result.connect(self.onSerialSendWorkerResult)
            self.threadpool.start(self.serial_send_worker)
            self._getStatus()
            logger.info(f"port update {old_port} --> {self.serial.port}")
        else:
            self.serial_recv_worker.signals.owari.emit()
            self.serial_send_worker.signals.owari.emit()
            self.threadpool.waitForDone(500)
            self.serial.close()
            self.serial_post_co.setEnabled(True)
            self.serial_refresh_bt.setEnabled(True)
            self.serial_switch_bt.setText("打开串口")

    def getPackIndex(self):
        self.pack_index += 1
        if self.pack_index <= 0 or self.pack_index > 255:
            self.pack_index = 1
        return self.pack_index

    def onSerialWorkerFinish(self):
        logger.info("emit from serial worker finish signal")

    def onSerialWorkerError(self, s):
        logger.error(f"emit from serial worker error signal | {s}")
        self.serial_recv_worker.signals.owari.emit()
        self.serial_send_worker.signals.owari.emit()
        self.threadpool.waitForDone(1000)
        msg = ModernMessageBox(self)
        msg.setIcon(QMessageBox.Critical)
        msg.setWindowTitle("串口通讯故障")
        msg.setText(repr(s[1]))
        msg.setDetailedText(s[2])
        msg.exec_()
        self.serial.close()
        self.serial_post_co.setEnabled(True)
        self.serial_refresh_bt.setEnabled(True)
        self.serial_switch_bt.setText("打开串口")
        self.serial_switch_bt.setChecked(False)

    def onSerialRecvWorkerResult(self, info):
        # logger.info(f"emit from serial worker result signal | {info.text}")
        cmd_type = info.content[5]
        if cmd_type == 0x00:
            self.updateVersionLabelBootloader(info)
        elif cmd_type == 0xA0:
            self.updateTemperautre(info)
        elif cmd_type == 0xB0:
            self.updateMotorTrayPosition(info)
        elif cmd_type == 0xB1:
            self.updateIDCardData(info)
        elif cmd_type == 0xB2:
            self.updateBarcode(info)
        elif cmd_type == 0xB3:
            self.updateMatplotData(info)
        elif cmd_type == 0xB5:
            self.showWarnInfo(info)
        elif cmd_type == 0xB6:
            self.onSampleOver()
        elif cmd_type == 0xD1:
            self.updateOutFlashData(info)
        elif cmd_type == 0xB7:
            self.updateVersionLabel(info)
        elif cmd_type == 0xC0:
            logger.success(f"recv pack from correct | {info.text}")
        elif cmd_type == 0xD0:
            self.updateMotorDebug(info)
        elif cmd_type == 0xD3:
            self.updateTemperatureHeater(info)
        elif cmd_type == 0xD4:
            self.updateDebugFlag(info)
        elif cmd_type == 0xDA:
            self.updateSelfCheckDialog(info)
        elif cmd_type == 0xDD:
            self.updateOutFlashParam(info)
        elif cmd_type == 0xEE:
            self.updateTemperautreRaw(info)
        elif cmd_type == 0xDF:
            self.udpateDeviceIDLabel(info)
        elif cmd_type == 0x32:
            self.updateSampleLED(info)
        elif cmd_type == 0x35:
            self.updateStaryOffset(info)
        elif cmd_type == 0x37:
            self.updateSampleWhiteMagnify(info)
        elif cmd_type == 0x92:
            payload = info.content[6:-1]
            logger.info(f"get sample board version | {bytesPuttyPrint(payload)}")
            v = ""
            if len(payload) == 1:
                v = f"BL v{payload[0]}"
            elif len(payload) == 3:
                v = f"APP v{payload[0]}.{payload[1]}.{payload[2]}"
            self.sample_fr_bt.setToolTip(f"采集板软件版本 {v}")

    def onSerialSendWorkerResult(self, write_result):
        result, write_data, info = write_result
        if write_data[5] == 0x0F:
            logger.success(f" result | {result} | write {bytesPuttyPrint(write_data)} | info | {info}")
            if result:
                file_path = self.upgrade_dg_lb.text()
                self.upgrade_dg_bt.setText("升级中")
                self.firm_start_time = time.time()
                self.firm_wrote_size = 0
                file_size = os.path.getsize(file_path)
                self.firm_size = file_size + (APP_BIN_CHUNK_SIZE - file_size % APP_BIN_CHUNK_SIZE)
                for pack in write_firmware_pack_FC(self.dd, file_path, chunk_size=APP_BIN_CHUNK_SIZE):
                    self.task_queue.put(pack)
                self.upgrade_dg_bt.setText("重启中")
                self.upgrade_dg_bt.setEnabled(False)
            else:
                self.upgrade_dg_bt.setText("重试")
                self.upgrade_dg_bt.setEnabled(True)
                self.upgrade_dg_bt.setChecked(False)
                self._clearTaskQueue()
            return
        elif write_data[5] == 0xFC:
            if result:
                self.firm_wrote_size += len(write_data) - 8
                self.upgrade_pr.setValue(int(self.firm_wrote_size * 100 / self.firm_size))
                time_usage = time.time() - self.firm_start_time
                logger.info(
                    f"firm write | complete {self.firm_wrote_size / self.firm_size:.2%} | {self.firm_wrote_size} / {self.firm_size} Byte | in {time_usage:.2f}S"
                )
                if write_data[6] == 0:
                    title = f"固件升级 结束 | {self.firm_wrote_size} / {self.firm_size} Byte | {time_usage:.2f} S"
                    QTimer.singleShot(7000, self._getStatus)
                    self.upgrade_dg_bt.setText("もう一回")
                    self.upgrade_dg_bt.setEnabled(True)
                    self.upgrade_dg_bt.setChecked(False)
                else:
                    title = f"固件升级中... | {self.firm_wrote_size} Byte | {time_usage:.2f} S"
                    self.upgrade_dg_bt.setText("升级中")
                self.upgrade_dg.setWindowTitle(title)
            else:
                self.upgrade_dg_bt.setText("重试")
                self.upgrade_dg_bt.setEnabled(True)
                self.upgrade_dg_bt.setChecked(False)
                self._clearTaskQueue()
            return
        elif write_data[5] == 0xDE:
            if result:
                size = struct.unpack("H", write_data[10:12])[0]
                self.bl_wrote_size += size
                self.upgbl_pr.setValue(int(self.bl_wrote_size * 100 / self.bl_size))
                time_usage = time.time() - self.bl_start_time
                if size == 0:
                    title = f"Bootloader升级 结束 | {self.bl_wrote_size} / {self.bl_size} Byte | {time_usage:.2f} S"
                    self.upgbl_dg_bt.setText("もう一回")
                    self.upgbl_dg_bt.setEnabled(True)
                    self.upgbl_dg_bt.setChecked(False)
                    QTimer.singleShot(7000, self._getStatus)
                else:
                    title = f"Bootloader升级 中... | {self.bl_wrote_size} / {self.bl_size} Byte | {time_usage:.2f} S"
                self.upgbl_dg.setWindowTitle(title)
            else:
                self.upgbl_dg_bt.setText("重试")
                self.upgbl_dg_bt.setEnabled(True)
                self.upgbl_dg_bt.setChecked(False)
                self._clearTaskQueue()
            return
        elif write_data[5] == 0xD9:
            logger.info(f"ID卡信息包 | {write_result}")
        elif write_data[5] == 0x91:
            if result:
                offset = struct.unpack("I", write_data[6:10])[0]
                size = struct.unpack("B", write_data[10:11])[0]
                total = struct.unpack("I", write_data[11:15])[0]
                logger.debug(f"sample firmware upgrade pack | offset {offset} size {size} total {total}")
                self.samplefr_wrote_size += size
                self.upgsample_pr.setValue(int(self.samplefr_wrote_size * 100 / self.samplefr_size))
                time_usage = time.time() - self.samplefr_start_time
                if offset + size == total:
                    title = f"采样板固件升级 结束 | {self.samplefr_wrote_size} / {self.samplefr_size} Byte | {time_usage:.2f} S"
                    self.upgsample_dg_bt.setText("もう一回")
                    self.upgsample_dg_bt.setEnabled(True)
                    self.upgsample_dg_bt.setChecked(False)
                else:
                    title = f"采样板固件升级 中... | {self.samplefr_wrote_size} / {self.samplefr_size} Byte | {time_usage:.2f} S"
                self.upgsample_dg.setWindowTitle(title)
            else:
                self.upgsample_dg_bt.setText("重试")
                self.upgsample_dg_bt.setEnabled(True)
                self.upgsample_dg_bt.setChecked(False)
                self._clearTaskQueue()
            return
        logger.info(f"other | {write_result}")

    def onSerialStatistic(self, info):
        if info[0] == "w" and time.time() - self.kirakira_recv_time > 0.1:
            self.kirakira_recv_time = time.time()
            self.kirakira_send_lb.setStyleSheet("background-color : green; color : #3d3d3d;")
            QTimer.singleShot(100, lambda: self.kirakira_send_lb.setStyleSheet("background-color : white; color : #3d3d3d;"))
        elif info[0] == "r" and time.time() - self.kirakira_send_time > 0.1:
            self.kirakira_send_time = time.time()
            self.kirakira_recv_lb.setStyleSheet("background-color : red; color : #3d3d3d;")
            QTimer.singleShot(100, lambda: self.kirakira_recv_lb.setStyleSheet("background-color : white; color : #3d3d3d;"))

    def createMatplot(self):
        self.matplot_data = dict()
        self.matplot_wg = QWidget(self)
        matplot_ly = QVBoxLayout(self.matplot_wg)
        matplot_ly.setSpacing(1)
        matplot_ly.setContentsMargins(2, 2, 2, 2)
        self.plot_graph = SampleGraph(parent=self)
        matplot_ly.addWidget(self.plot_graph.win)
        self.temp_saple_data = [None] * 6
        for i in range(6):
            shortcut_switch_cb = QShortcut(QKeySequence(f"Ctrl+{i + 1}"), self)
            shortcut_switch_cb.activated.connect(partial(self.on_matplot_hide, idx=i))
        self.matplot_shot_bits = (1 << 6) - 1

    def on_matplot_hide(self, idx):
        flag = 1 << idx
        if (self.matplot_shot_bits & flag) == flag:
            self.plot_graph.hide_plot(idx)
            for num in range(1, len(self.plot_graph.plot_data_confs) // 6):
                self.plot_graph.hide_plot(idx + 6 * num)
            self.matplot_shot_bits = self.matplot_shot_bits & ((1 << 6) - 1 - flag)
        else:
            self.plot_graph.show_plot(idx)
            for num in range(1, len(self.plot_graph.plot_data_confs) // 6):
                self.plot_graph.show_plot(idx + 6 * num)
            self.matplot_shot_bits = self.matplot_shot_bits | flag

    def update_matplot_timer(self):
        for idx, lb in enumerate(self.matplot_timer_lbs):
            v = float(lb.text())
            v = v - TMER_INTERVAL / 1000
            if v > 0:
                lb.setText(f"{v:.1f}")
            else:
                lb.setText("0")

    def onMatplotStart(self, event, name_text=None):
        self._getDeviceID()
        self.initBarcodeScan()
        press_result = False
        if name_text is None:
            if self.lamp_ag_cb.isChecked():
                name_text = f"Aging {datetime.now().strftime('%Y%m%d%H%M%S')}"
            else:
                name_text, press_result = QInputDialog.getText(self, "测试标签", "输入标签名称", QLineEdit.Normal, datetime.now().strftime("%Y%m%d%H%M%S"))
                if not press_result:
                    logger.info("cancel sample test")
                    return
        if len(name_text) > 0:
            self.sample_record_lable_name = name_text
        else:
            self.sample_record_lable_name = datetime.now().strftime("%Y%m%d%H%M%S")
        logger.debug(f"set label name | {name_text} | {press_result} | {self.sample_record_lable_name}")
        conf = []
        self.sample_confs = []
        self.sample_datas = []
        check_state = self.matplot_period_tv_cb.checkState()
        for i in range(6):
            conf.append(self.matplot_conf_houhou_cs[i].currentIndex())
            conf.append(self.matplot_conf_wavelength_cs[i].currentIndex() + 1)
            points_num = self.matplot_conf_point_sps[i].value()
            if check_state == 2:
                conf.append(points_num if points_num <= 20 else 20)
            else:
                conf.append(points_num)
            if conf[-3] == 0 or conf[-1] == 0:
                v = 0
            else:
                v = conf[-1] * 10 + 7.8
            self.matplot_timer_lbs[i].setText(f"{v:.1f}")
            self.sample_confs.append(SampleConf(conf[-3], conf[-2], conf[-1], self.matplot_conf_set_cs[i].currentText()))
        logger.debug(f"get matplot cnf | {conf}")
        self.sample_label = self.sample_db.build_label(
            name=self.sample_record_lable_name, version=f"{self.version}.{datetime.strftime(self.device_datetime, '%Y%m%d.%H%M%S')}", device_id=self.device_id
        )
        self._serialSendPack(0x01)
        if self.matplot_period_tv_cb.isChecked():
            conf.append(check_state if check_state != 2 else 3)
            self._serialSendPack(0x08, conf)
        else:
            self._serialSendPack(0x03, conf)
        self.plot_graph.clear_plot()
        self.matplot_data.clear()
        self.matplot_timer.start(TMER_INTERVAL)
        self.matplot_timer_time_list = [time.time()] * 6

    def onCorrectMatplotStart(self):
        self.initBarcodeScan()
        self.sample_record_lable_name = f"Correct {datetime.now().strftime('%Y%m%d%H%M%S')}"
        logger.debug(f"set correct label name | {self.sample_record_lable_name}")
        conf = []
        self.sample_confs = []
        self.sample_datas = []
        for i in range(6):
            conf.append(1)
            conf.append(1)
            conf.append(12)
            self.sample_confs.append(SampleConf(conf[-3], conf[-2], conf[-1], "定标"))
        logger.debug(f"get matplot cnf | {conf}")
        self.sample_label = self.sample_db.build_label(
            name=self.sample_record_lable_name, version=f"{self.version}.{datetime.strftime(self.device_datetime, '%Y%m%d.%H%M%S')}", device_id=self.device_id
        )
        self.plot_graph.clear_plot()
        self.matplot_data.clear()

    def stop_matplot_timer(self):
        self.matplot_timer.stop()
        for idx, lb in enumerate(self.matplot_timer_lbs):
            lb.setText(f"TT{idx}")

    def onMatplotCancel(self, event):
        self._serialSendPack(0x02)
        self.stop_matplot_timer()
        self.debug_flag_cbs[5].setChecked(False)

    def onBootload(self, event):
        # self._serialSendPack(0x0F)
        # self._serialSendPack(0xDC)
        self.upgbl_dg = QDialog(self)
        self.upgbl_dg.setWindowTitle("Bootloader升级")
        dg_ly = QVBoxLayout(self.upgbl_dg)

        upgbl_temp_ly = QHBoxLayout()
        upgbl_temp_ly.addWidget(QLabel("Bootloader路径"))
        self.upgbl_dg_lb = QLineEdit()
        if self.last_bl_path is not None and os.path.isfile(self.last_bl_path):
            self.upgbl_dg_lb.setText(self.last_bl_path)
            self.upgbl_dg.setWindowTitle(f"Bootloader升级 | {self._getFileHash_SHA256(self.last_bl_path)}")
        self.upgbl_dg_fb_bt = QPushButton("...")
        upgbl_temp_ly.addWidget(self.upgbl_dg_lb)
        upgbl_temp_ly.addWidget(self.upgbl_dg_fb_bt)
        dg_ly.addLayout(upgbl_temp_ly)
        self.upgbl_dg_fb_bt.clicked.connect(self.onUpgBLDialogFileSelect)

        upgbl_temp_ly = QHBoxLayout()
        self.upgbl_pr = QProgressBar(self)
        upgbl_temp_ly.addWidget(QLabel("进度"))
        upgbl_temp_ly.addWidget(self.upgbl_pr)
        self.upgbl_dg_bt = QPushButton("开始")
        upgbl_temp_ly.addWidget(self.upgbl_dg_bt)
        dg_ly.addLayout(upgbl_temp_ly)

        self.upgbl_pr.setMaximum(100)
        self.upgbl_dg_bt.setCheckable(True)
        self.upgbl_dg_bt.clicked.connect(self.onUpgBLDialog)

        self.upgbl_dg.resize(600, 75)
        self.upgbl_dg = ModernDialog(self.upgbl_dg, self)
        self.upgbl_dg.exec_()

    def onUpgBLDialog(self, event):
        terminal = False
        if event is True:
            file_path = self.upgbl_dg_lb.text()
            if file_path.startswith("http") and file_path.endswith("bin"):
                if os.path.isfile("temp_bootloader.bin"):
                    os.remove("temp_bootloader.bin")
                try:
                    resp = requests.get(file_path, timeout=10)
                    if resp.status_code != 200:
                        terminal = True
                except Exception as e:
                    self.upgbl_dg_lb.setText(f"url 处理异常 | {repr(e)}")
                    terminal = True
                if terminal:
                    self.upgbl_dg_bt.setChecked(False)
                    self.upgbl_dg.setWindowTitle("固件升级")
                    return
                file_path = "temp_bootloader.bin"
                self.upgbl_dg_lb.setText(file_path)
                with open("temp_bootloader.bin", "wb") as f:
                    f.write(resp.content)
            if os.path.isfile(file_path):
                self.upgbl_dg_bt.setEnabled(False)
                self.bl_wrote_size = 0
                self.bl_start_time = time.time()
                for pack in write_firmware_pack_BL(self.dd, file_path, chunk_size=224):
                    self.task_queue.put(pack)
            else:
                logger.error(f"invalid file path | {file_path}")
                self.upgbl_dg_bt.setChecked(False)
                self.upgbl_dg_lb.setText("请输入有效路径")
                self.upgbl_dg.setWindowTitle("Bootloader升级")
        elif event is False:
            self.upgbl_dg.close()

    def onUpgBLDialogFileSelect(self, event):
        fd = QFileDialog()
        file_path, _ = fd.getOpenFileName(filter="BIN 文件 (*.bin)")
        if file_path:
            file_size = os.path.getsize(file_path)
            if file_size > 2 ** 16:
                self.upgbl_dg_bt.setChecked(False)
                self.upgbl_dg_lb.setText("文件大小超过64K")
                self.upgbl_dg.setWindowTitle("Bootloader升级")
            else:
                self.upgbl_dg_lb.setText(file_path)
                self.last_bl_path = file_path
                self.upgbl_dg.setWindowTitle(f"Bootloader升级 | {self._getFileHash_SHA256(file_path)}")
                self.bl_size = file_size

    def onSampleFr(self, event):
        # self._serialSendPack(0x0F)
        # self._serialSendPack(0xDC)
        self.upgsample_dg = QDialog(self)
        self.upgsample_dg.setWindowTitle("采样板固件升级")
        dg_ly = QVBoxLayout(self.upgsample_dg)

        temp_ly = QHBoxLayout()
        temp_ly.addWidget(QLabel("采样板固件路径"))
        self.upgsample_dg_lb = QLineEdit()
        if self.last_samplefr_path is not None and os.path.isfile(self.last_samplefr_path):
            self.upgsample_dg_lb.setText(self.last_samplefr_path)
            self.upgsample_dg.setWindowTitle(f"采样板固件升级 | {self._getFileHash_SHA256(self.last_samplefr_path)}")
        self.upgbl_dg_fb_bt = QPushButton("...")
        temp_ly.addWidget(self.upgsample_dg_lb)
        temp_ly.addWidget(self.upgbl_dg_fb_bt)
        dg_ly.addLayout(temp_ly)
        self.upgbl_dg_fb_bt.clicked.connect(self.onUpgSampleDialogFileSelect)

        temp_ly = QHBoxLayout()
        self.upgsample_pr = QProgressBar(self)
        temp_ly.addWidget(QLabel("进度"))
        temp_ly.addWidget(self.upgsample_pr)
        self.upgsample_dg_bt = QPushButton("开始")
        temp_ly.addWidget(self.upgsample_dg_bt)
        dg_ly.addLayout(temp_ly)

        self.upgsample_pr.setMaximum(100)
        self.upgsample_dg_bt.setCheckable(True)
        self.upgsample_dg_bt.clicked.connect(self.onUpgSampleDialog)

        self.upgsample_dg.resize(600, 75)
        self.upgsample_dg = ModernDialog(self.upgsample_dg, self)
        self.upgsample_dg.exec_()

    def onUpgSampleDialog(self, event):
        terminal = False
        if event is True:
            file_path = self.upgsample_dg_lb.text()
            if file_path.startswith("http") and file_path.endswith("bin"):
                if os.path.isfile("temp_sample.bin"):
                    os.remove("temp_sample.bin")
                try:
                    resp = requests.get(file_path, timeout=10)
                    if resp.status_code != 200:
                        terminal = True
                except Exception as e:
                    self.upgsample_dg_lb.setText(f"url 处理异常 | {repr(e)}")
                    terminal = True
                if terminal:
                    self.upgsample_dg_bt.setChecked(False)
                    self.upgsample_dg.setWindowTitle("固件升级")
                    return
                file_path = "temp_sample.bin"
                self.upgsample_dg_lb.setText(file_path)
                with open("temp_sample.bin", "wb") as f:
                    f.write(resp.content)
            if os.path.isfile(file_path):
                self.upgsample_dg_bt.setEnabled(False)
                self.samplefr_wrote_size = 0
                self.samplefr_start_time = time.time()
                self._serialSendPack(0x90, device_id=0x46)
                for pack in write_firmware_pack_SA(self.dd, file_path, chunk_size=224):
                    self.task_queue.put(pack)
            else:
                logger.error(f"invalid file path | {file_path}")
                self.upgsample_dg_bt.setChecked(False)
                self.upgsample_dg_lb.setText("请输入有效路径")
                self.upgsample_dg.setWindowTitle("采样板固件升级")
        elif event is False:
            self.upgsample_dg.close()

    def onUpgSampleDialogFileSelect(self, event):
        fd = QFileDialog()
        file_path, _ = fd.getOpenFileName(filter="BIN 文件 (*.bin)")
        if file_path:
            file_size = os.path.getsize(file_path)
            if file_size > 4 ** 16:
                self.upgsample_dg_bt.setChecked(False)
                self.upgsample_dg_lb.setText("文件大小超过128K")
                self.upgsample_dg.setWindowTitle("采样板固件升级")
            else:
                self.upgsample_dg_lb.setText(file_path)
                self.last_samplefr_path = file_path
                self.upgsample_dg.setWindowTitle(f"采样板固件升级 | {self._getFileHash_SHA256(file_path)}")
                self.samplefr_size = file_size

    def onReboot(self, event):
        self._serialSendPack(0xDC)
        QTimer.singleShot(3000, self._getStatus)

    def onSelftest_temp_top_gb_Clicked(self, event):
        self._clear_widget_style_sheet(self.selftest_temp_top_gb)
        self._serialSendPack(0xDA, (0x01,))

    def onSelftest_temp_btm_gb_Clicked(self, event):
        self._clear_widget_style_sheet(self.selftest_temp_btm_gb)
        self._serialSendPack(0xDA, (0x02,))

    def onSelftest_temp_env_gb_Clicked(self, event):
        self._clear_widget_style_sheet(self.selftest_temp_env_gb)
        self._serialSendPack(0xDA, (0x03,))

    def onSelftest_motor_heater_gb_Clicked(self, event):
        self._clear_widget_style_sheet(self.selftest_motor_heater_gb)
        self._serialSendPack(0xDA, (0x07,))

    def onSelftest_motor_tray_gb_Clicked(self, event):
        self._clear_widget_style_sheet(self.selftest_motor_tray_gb)
        self._serialSendPack(0xDA, (0x08,))

    def onSelftest_motor_white_gb_Clicked(self, event):
        self._clear_widget_style_sheet(self.selftest_motor_white_gb)
        self._serialSendPack(0xDA, (0x06,))

    def onSelftest_motor_scan_m_Clicked(self, event):
        self._setColor(self.selftest_motor_scan_m)
        self._serialSendPack(0xDA, (0x09,))
        self._clear_widget_style_sheet(self.selftest_motor_scan_gb)

    def onSelftest_motor_scan_l_Clicked(self, event):
        self._setColor(self.selftest_motor_scan_l)
        self.selftest_motor_scan_l.setText("*" * 10)
        self._serialSendPack(0xDA, (0x0A,))
        self._clear_widget_style_sheet(self.selftest_motor_scan_gb)

    def onSelftest_storge_lb_f_Clicked(self, event):
        self._setColor(self.selftest_storge_lb_f)
        self._serialSendPack(0xDA, (0x04,))

    def onSelftest_storge_lb_c_Clicked(self, event):
        self._setColor(self.selftest_storge_lb_c)
        self._serialSendPack(0xDA, (0x05,))

    def onSelftest_lamp_610_gb_Clicked(self, event):
        for idx, lb in enumerate(self.selftest_lamp_610_lbs):
            self._setColor(lb)
            lb.setText(f"{idx + 1}")
        self._serialSendPack(0xDA, (0x0B, 1))

    def onSelftest_lamp_550_gb_Clicked(self, event):
        for idx, lb in enumerate(self.selftest_lamp_550_lbs):
            self._setColor(lb)
            lb.setText(f"{idx + 1}")
        self._serialSendPack(0xDA, (0x0B, 2))

    def onSelftest_lamp_405_gb_Clicked(self, event):
        for idx, lb in enumerate(self.selftest_lamp_405_lbs):
            self._setColor(lb)
            lb.setText(f"{idx + 1}")
        self._serialSendPack(0xDA, (0x0B, 4))

    def createSelfCheckDialog(self):
        self.selftest_dg = QDialog(self)
        self.selftest_dg.resize(467, 181)
        self.selftest_dg.resizeEvent = lambda x: logger.debug(f"windows size | {self.selftest_dg.size()}")
        self.selftest_dg.setWindowTitle("自检测试")
        self.selftest_temp_lbs = [QLabel("**.**") for _ in range(9)]
        selftest_dg_ly = QVBoxLayout(self.selftest_dg)
        selftest_temp_ly = QHBoxLayout()
        self.selftest_temp_top_gb = QGroupBox("上加热体温度")
        self.selftest_temp_top_gb.setLayout(QHBoxLayout())
        self.selftest_temp_top_gb.layout().setContentsMargins(3, 3, 3, 3)
        for lb in self.selftest_temp_lbs[0:6]:
            lb.setAlignment(Qt.AlignCenter)
            lb.mouseReleaseEvent = self.onSelftest_temp_top_gb_Clicked
            self.selftest_temp_top_gb.layout().addWidget(lb)
        self.selftest_temp_btm_gb = QGroupBox("下加热体温度")
        self.selftest_temp_btm_gb.setLayout(QHBoxLayout())
        self.selftest_temp_btm_gb.layout().setContentsMargins(3, 3, 3, 3)
        for lb in self.selftest_temp_lbs[6:8]:
            lb.setAlignment(Qt.AlignCenter)
            lb.mouseReleaseEvent = self.onSelftest_temp_btm_gb_Clicked
            self.selftest_temp_btm_gb.layout().addWidget(lb)
        self.selftest_temp_env_gb = QGroupBox("环境")
        self.selftest_temp_env_gb.setLayout(QHBoxLayout())
        self.selftest_temp_env_gb.layout().setContentsMargins(3, 3, 3, 3)
        for lb in self.selftest_temp_lbs[8:9]:
            lb.setAlignment(Qt.AlignCenter)
            lb.mouseReleaseEvent = self.onSelftest_temp_env_gb_Clicked
            self.selftest_temp_env_gb.layout().addWidget(lb)
        selftest_temp_ly.addWidget(self.selftest_temp_top_gb, stretch=6)
        selftest_temp_ly.addWidget(self.selftest_temp_btm_gb, stretch=2)
        selftest_temp_ly.addWidget(self.selftest_temp_env_gb, stretch=1)

        selftest_motor_ly = QHBoxLayout()
        self.selftest_motor_lbs = [QLabel(t) for t in ("上", "下", "进", "出", "PD", "白")]
        self.selftest_motor_heater_gb = QGroupBox("上加热体")
        self.selftest_motor_heater_gb.setLayout(QHBoxLayout())
        self.selftest_motor_heater_gb.layout().setContentsMargins(3, 3, 3, 3)
        for lb in self.selftest_motor_lbs[0:2]:
            lb.setAlignment(Qt.AlignCenter)
            lb.mouseReleaseEvent = self.onSelftest_motor_heater_gb_Clicked
            self.selftest_motor_heater_gb.layout().addWidget(lb)
        self.selftest_motor_tray_gb = QGroupBox("托盘")
        self.selftest_motor_tray_gb.setLayout(QHBoxLayout())
        self.selftest_motor_tray_gb.layout().setContentsMargins(3, 3, 3, 3)
        for lb in self.selftest_motor_lbs[2:4]:
            lb.setAlignment(Qt.AlignCenter)
            lb.mouseReleaseEvent = self.onSelftest_motor_tray_gb_Clicked
            self.selftest_motor_tray_gb.layout().addWidget(lb)
        self.selftest_motor_white_gb = QGroupBox("白板")
        self.selftest_motor_white_gb.setLayout(QHBoxLayout())
        self.selftest_motor_white_gb.layout().setContentsMargins(3, 3, 3, 3)
        for lb in self.selftest_motor_lbs[4:6]:
            lb.setAlignment(Qt.AlignCenter)
            lb.mouseReleaseEvent = self.onSelftest_motor_white_gb_Clicked
            self.selftest_motor_white_gb.layout().addWidget(lb)
        self.selftest_motor_scan_gb = QGroupBox("扫码")
        self.selftest_motor_scan_gb.setLayout(QHBoxLayout())
        self.selftest_motor_scan_gb.layout().setContentsMargins(3, 3, 3, 3)
        self.selftest_motor_scan_m = QLabel("电机")
        self.selftest_motor_scan_m.mouseReleaseEvent = self.onSelftest_motor_scan_m_Clicked
        self.selftest_motor_scan_l = QLabel("*" * 10)
        self.selftest_motor_scan_l.mouseReleaseEvent = self.onSelftest_motor_scan_l_Clicked
        self.selftest_motor_scan_m.setAlignment(Qt.AlignCenter)
        self.selftest_motor_scan_l.setAlignment(Qt.AlignCenter)
        self.selftest_motor_scan_gb.layout().addWidget(self.selftest_motor_scan_m, stretch=1)
        self.selftest_motor_scan_gb.layout().addWidget(QVLine())
        self.selftest_motor_scan_gb.layout().addWidget(self.selftest_motor_scan_l, stretch=2)
        selftest_motor_ly.addWidget(self.selftest_motor_heater_gb, stretch=1)
        selftest_motor_ly.addWidget(self.selftest_motor_tray_gb, stretch=1)
        selftest_motor_ly.addWidget(self.selftest_motor_white_gb, stretch=1)
        selftest_motor_ly.addWidget(self.selftest_motor_scan_gb, stretch=3)

        selftest_storge_ly = QHBoxLayout()
        self.selftest_storge_lb_f = QLabel("片外 Flash")
        self.selftest_storge_lb_f.mouseReleaseEvent = self.onSelftest_storge_lb_f_Clicked
        self.selftest_storge_lb_c = QLabel("ID Code 卡")
        self.selftest_storge_lb_c.mouseReleaseEvent = self.onSelftest_storge_lb_c_Clicked
        selftest_storge_gb = QGroupBox("存储")
        selftest_storge_gb.setLayout(QHBoxLayout())
        selftest_storge_gb.layout().setContentsMargins(3, 3, 3, 3)
        self.selftest_storge_lb_f.setAlignment(Qt.AlignCenter)
        self.selftest_storge_lb_c.setAlignment(Qt.AlignCenter)
        selftest_storge_gb.layout().addWidget(self.selftest_storge_lb_f)
        selftest_storge_gb.layout().addWidget(self.selftest_storge_lb_c)
        selftest_storge_ly.addWidget(selftest_storge_gb)

        selftest_lamp_ly = QGridLayout()
        self.selftest_lamp_610_gb = QGroupBox("610")
        self.selftest_lamp_610_gb.setLayout(QHBoxLayout())
        self.selftest_lamp_610_gb.layout().setContentsMargins(0, 3, 0, 3)
        self.selftest_lamp_610_gb.layout().setSpacing(5)
        self.selftest_lamp_550_gb = QGroupBox("550")
        self.selftest_lamp_550_gb.setLayout(QHBoxLayout())
        self.selftest_lamp_550_gb.layout().setContentsMargins(0, 3, 0, 3)
        self.selftest_lamp_550_gb.layout().setSpacing(5)
        self.selftest_lamp_405_gb = QGroupBox("405")
        self.selftest_lamp_405_gb.setLayout(QHBoxLayout())
        self.selftest_lamp_405_gb.layout().setContentsMargins(0, 3, 0, 3)
        self.selftest_lamp_405_gb.layout().setSpacing(5)

        self.selftest_lamp_610_lbs = []
        self.selftest_lamp_550_lbs = []
        self.selftest_lamp_405_lbs = []
        for i in range(6):
            lb = QLabel(f"{i + 1:^5d}")
            self.selftest_lamp_610_gb.layout().addWidget(lb, alignment=Qt.AlignCenter)
            self.selftest_lamp_610_lbs.append(lb)
        for i in range(6):
            lb = QLabel(f"{i + 1:^5d}")
            self.selftest_lamp_550_gb.layout().addWidget(lb, alignment=Qt.AlignCenter)
            self.selftest_lamp_550_lbs.append(lb)
        for i in range(1):
            lb = QLabel(f"{i + 1:^5d}")
            self.selftest_lamp_405_gb.layout().addWidget(lb, alignment=Qt.AlignCenter)
            self.selftest_lamp_405_lbs.append(lb)
        selftest_lamp_ly.addWidget(self.selftest_lamp_610_gb, 0, 0, 1, 6)
        selftest_lamp_ly.addWidget(self.selftest_lamp_550_gb, 0, 6, 1, 6)
        selftest_lamp_ly.addWidget(self.selftest_lamp_405_gb, 0, 12, 1, 1)
        self.selftest_lamp_610_gb.mouseReleaseEvent = self.onSelftest_lamp_610_gb_Clicked
        self.selftest_lamp_550_gb.mouseReleaseEvent = self.onSelftest_lamp_550_gb_Clicked
        self.selftest_lamp_405_gb.mouseReleaseEvent = self.onSelftest_lamp_405_gb_Clicked

        selftest_dg_ly.addLayout(selftest_temp_ly)
        selftest_dg_ly.addLayout(selftest_motor_ly)
        selftest_dg_ly.addLayout(selftest_storge_ly)
        selftest_dg_ly.addLayout(selftest_lamp_ly)
        self.selftest_dg = ModernDialog(self.selftest_dg, self)

    def updateSelfCheckDialog(self, info):
        raw_bytes = info.content
        item = raw_bytes[6]
        result = raw_bytes[7]
        if item == 1:
            if result > 0:
                self.selftest_temp_top_gb.setStyleSheet("QGroupBox:title {color: red};")
            else:
                self.selftest_temp_top_gb.setStyleSheet("QGroupBox:title {color: green};")
            for i, lb in enumerate(self.selftest_temp_lbs[0:6]):
                temp = struct.unpack("f", raw_bytes[8 + 4 * i : 12 + 4 * i])[0]
                lb.setText(f"{temp:.2f}")
                if 36.7 < temp < 37.3:
                    self._setColor(lb, nbg="green")
                else:
                    self._setColor(lb, nbg="red")
        elif item == 2:
            if result > 0:
                self.selftest_temp_btm_gb.setStyleSheet("QGroupBox:title {color: red};")
            else:
                self.selftest_temp_btm_gb.setStyleSheet("QGroupBox:title {color: green};")
            for i, lb in enumerate(self.selftest_temp_lbs[6:8]):
                temp = struct.unpack("f", raw_bytes[8 + 4 * i : 12 + 4 * i])[0]
                lb.setText(f"{temp:.2f}")
                if 36.7 < temp < 37.3:
                    self._setColor(lb, nbg="green")
                else:
                    self._setColor(lb, nbg="red")
        elif item == 3:
            if result > 0:
                self.selftest_temp_env_gb.setStyleSheet("QGroupBox:title {color: red};")
            else:
                self.selftest_temp_env_gb.setStyleSheet("QGroupBox:title {color: green};")
            for i, lb in enumerate(self.selftest_temp_lbs[8:9]):
                temp = struct.unpack("f", raw_bytes[8 + 4 * i : 12 + 4 * i])[0]
                lb.setText(f"{temp:.2f}")
                if 16 < temp < 46:
                    self._setColor(lb, nbg="green")
                else:
                    self._setColor(lb, nbg="red")
        elif item == 4:
            if result > 0:
                self._setColor(self.selftest_storge_lb_f, nbg="red")
            else:
                self._setColor(self.selftest_storge_lb_f, nbg="green")
        elif item == 5:
            if result > 0:
                self._setColor(self.selftest_storge_lb_c, nbg="red")
            else:
                self._setColor(self.selftest_storge_lb_c, nbg="green")
        elif item == 6:
            if result > 0:
                self.selftest_motor_white_gb.setStyleSheet("QGroupBox:title {color: red};")
            else:
                self.selftest_motor_white_gb.setStyleSheet("QGroupBox:title {color: green};")
            for i, lb in enumerate(self.selftest_motor_lbs[4:6]):
                data = raw_bytes[8 + i]
                if data == 0:
                    self._setColor(lb, nbg="green")
                else:
                    self._setColor(lb, nbg="red")
        elif item == 7:
            if result > 0:
                self.selftest_motor_heater_gb.setStyleSheet("QGroupBox:title {color: red};")
            else:
                self.selftest_motor_heater_gb.setStyleSheet("QGroupBox:title {color: green};")
            for i, lb in enumerate(self.selftest_motor_lbs[0:2]):
                data = raw_bytes[8 + i]
                if data == 0:
                    self._setColor(lb, nbg="green")
                else:
                    self._setColor(lb, nbg="red")
        elif item == 8:
            if result > 0:
                self.selftest_motor_tray_gb.setStyleSheet("QGroupBox:title {color: red};")
            else:
                self.selftest_motor_tray_gb.setStyleSheet("QGroupBox:title {color: green};")
            for i, lb in enumerate(self.selftest_motor_lbs[2:4]):
                data = raw_bytes[8 + i]
                if data == 0:
                    self._setColor(lb, nbg="green")
                else:
                    self._setColor(lb, nbg="red")
        elif item == 9:
            if result > 0:
                self._setColor(self.selftest_motor_scan_m, nbg="red")
                self.selftest_motor_scan_gb.setStyleSheet("QGroupBox:title {color: red};")
            else:
                self._setColor(self.selftest_motor_scan_m, nbg="green")
        elif item == 10:
            if result > 0:
                if raw_bytes[8]:
                    logger.error(f"get raw byte in self check barcode | {bytesPuttyPrint(raw_bytes[9 : 9 + raw_bytes[8]])}")
                self._setColor(self.selftest_motor_scan_l, nbg="red")
                self.selftest_motor_scan_gb.setStyleSheet("QGroupBox:title {color: red};")
                text = bytesPuttyPrint(raw_bytes[6:-1])
                self.selftest_motor_scan_l.setText(text)
            else:
                try:
                    text = raw_bytes[9 : 9 + raw_bytes[8]].decode()
                except Exception:
                    text = bytesPuttyPrint(raw_bytes)
                    logger.warning(f"decode failed use bytes result | {text}")
                else:
                    self._setColor(self.selftest_motor_scan_l, nbg="green")
                    self.selftest_motor_scan_gb.setStyleSheet("QGroupBox:title {color: green};")
                self.selftest_motor_scan_l.setText(text)
        elif item == 11:
            if len(raw_bytes) != 62:
                logger.error(f"self check pd data length error | {bytesPuttyPrint(raw_bytes)}")
            mask = raw_bytes[7]
            wh_except = raw_bytes[8]
            logger.debug(f"get self check pd wh_except | {wh_except}")
            values = []
            for i in range(13):
                value = struct.unpack("I", raw_bytes[9 + 4 * i : 9 + 4 * (i + 1)])[0]
                logger.debug(f"get self check pd data | {i} | {value}")
                values.append(value)
            now = datetime.now()
            if mask & 0x01:
                self.selftest_lamp_610_gb.setToolTip(f"{[i for i in values[0:6]]}, {now}")
                mi, ma = (min(CONFIG["pd_criterion"]["610"]), max(CONFIG["pd_criterion"]["610"]))
                for idx, v in enumerate(values[0:6]):
                    self.selftest_lamp_610_lbs[idx].setText(f"{idx+1}: {v:>8d}")
                    if v > ma or v < mi:
                        self._setColor(self.selftest_lamp_610_lbs[idx], nbg="red")
                    else:
                        self._setColor(self.selftest_lamp_610_lbs[idx], nbg="green")
            if mask & 0x02:
                self.selftest_lamp_550_gb.setToolTip(f"{[i for i in values[6:12]]}, {now}")
                mi, ma = (min(CONFIG["pd_criterion"]["550"]), max(CONFIG["pd_criterion"]["550"]))
                for idx, v in enumerate(values[6:12]):
                    self.selftest_lamp_550_lbs[idx].setText(f"{idx+1}: {v:>8d}")
                    if v > ma or v < mi:
                        self._setColor(self.selftest_lamp_550_lbs[idx], nbg="red")
                    else:
                        self._setColor(self.selftest_lamp_550_lbs[idx], nbg="green")
            if mask & 0x04:
                self.selftest_lamp_405_gb.setToolTip(f"{[i for i in values[12:]]}, {now}")
                mi, ma = (min(CONFIG["pd_criterion"]["405"]), max(CONFIG["pd_criterion"]["405"]))
                for idx, v in enumerate(values[12:]):
                    self.selftest_lamp_405_lbs[idx].setText(f"{idx+1}: {v:>8d}")
                    if v > ma or v < mi:
                        self._setColor(self.selftest_lamp_405_lbs[idx], nbg="red")
                    else:
                        self._setColor(self.selftest_lamp_405_lbs[idx], nbg="green")

    def onSelfCheck(self, event):
        button = event.button()
        logger.debug(f"invoke onSelfCheck | event {event.type()} | {button}")
        if button == Qt.LeftButton:
            for lb in self.selftest_temp_lbs:
                lb.setText("**.**")
            for idx, lb in enumerate(self.selftest_lamp_610_lbs):
                self._setColor(lb)
                lb.setText(f"{idx + 1}")
            for idx, lb in enumerate(self.selftest_lamp_550_lbs):
                self._setColor(lb)
                lb.setText(f"{idx + 1}")
            for idx, lb in enumerate(self.selftest_lamp_405_lbs):
                self._setColor(lb)
                lb.setText(f"{idx + 1}")
            self._clear_widget_style_sheet(self.selftest_temp_top_gb)
            self._clear_widget_style_sheet(self.selftest_temp_btm_gb)
            self._clear_widget_style_sheet(self.selftest_temp_env_gb)
            self._clear_widget_style_sheet(self.selftest_motor_heater_gb)
            self._clear_widget_style_sheet(self.selftest_motor_tray_gb)
            self._clear_widget_style_sheet(self.selftest_motor_white_gb)
            self._setColor(self.selftest_motor_scan_m)
            self._setColor(self.selftest_motor_scan_l)
            self.selftest_motor_scan_l.setText("*" * 10)
            self._setColor(self.selftest_storge_lb_f)
            self._setColor(self.selftest_storge_lb_c)
            self._serialSendPack(0xDA)
            self.selftest_dg.show()
        elif button == Qt.RightButton:
            logger.debug("show self test dialog")
            self.selftest_dg.show()

    def onUpgrade(self, event):
        self.upgrade_dg = QDialog(self)
        self.upgrade_dg.setWindowTitle("固件升级")
        self.upgrade_dg_ly = QVBoxLayout(self.upgrade_dg)

        upgrade_temp_ly = QHBoxLayout()
        upgrade_temp_ly.addWidget(QLabel("固件路径"))
        self.upgrade_dg_lb = QLineEdit()
        if self.last_firm_path is not None and os.path.isfile(self.last_firm_path):
            self.upgrade_dg_lb.setText(self.last_firm_path)
            self.upgrade_dg.setWindowTitle(f"固件升级 | {self._getFileHash_SHA256(self.last_firm_path)}")
        self.upgrade_dg_fb_bt = QPushButton("...", maximumWidth=40)
        self.upgrade_dg_ck_bt = QPushButton("检查", maximumWidth=40)
        upgrade_temp_ly.addWidget(self.upgrade_dg_lb)
        upgrade_temp_ly.addWidget(self.upgrade_dg_fb_bt)
        upgrade_temp_ly.addWidget(self.upgrade_dg_ck_bt)
        self.upgrade_dg_ly.addLayout(upgrade_temp_ly)
        self.upgrade_dg_fb_bt.clicked.connect(self.onUpgradeDialogFileSelect)
        self.upgrade_dg_ck_bt.clicked.connect(self.onUPgradeDialogCheck)

        upgrade_temp_ly = QHBoxLayout()
        self.upgrade_pr = QProgressBar(self)
        upgrade_temp_ly.addWidget(QLabel("进度"))
        upgrade_temp_ly.addWidget(self.upgrade_pr)
        self.upgrade_dg_bt = QPushButton("开始")
        upgrade_temp_ly.addWidget(self.upgrade_dg_bt)
        self.upgrade_dg_ly.addLayout(upgrade_temp_ly)

        self.upgrade_pr.setMaximum(100)
        self.upgrade_dg_bt.setCheckable(True)
        self.upgrade_dg_bt.clicked.connect(self.onUpgradeDialog)

        self.upgrade_dg.resize(600, 75)
        self.upgrade_dg = ModernDialog(self.upgrade_dg, self)
        self.upgrade_dg.exec_()

    def onUpgradeDialog(self, event):
        terminal = False
        if event is True:
            file_path = self.upgrade_dg_lb.text()
            if file_path.startswith("http") and file_path.endswith("bin"):
                if os.path.isfile("temp_application.bin"):
                    os.remove("temp_application.bin")
                try:
                    resp = requests.get(file_path, timeout=10)
                    if resp.status_code != 200:
                        terminal = True
                except Exception as e:
                    self.upgrade_dg_lb.setText(f"url 处理异常 | {repr(e)}")
                    terminal = True
                if terminal:
                    self.upgrade_dg_bt.setChecked(False)
                    self.upgrade_dg.setWindowTitle("固件升级")
                    return
                file_path = "temp_application.bin"
                self.upgrade_dg_lb.setText(file_path)
                with open("temp_application.bin", "wb") as f:
                    f.write(resp.content)
            if os.path.isfile(file_path):
                self.upgrade_dg_bt.setEnabled(False)
                self._serialSendPack(0x0F)
            else:
                self.upgrade_dg_bt.setChecked(False)
                self.upgrade_dg_lb.setText("请输入有效路径")
                self.upgrade_dg.setWindowTitle("固件升级")
        elif event is False:
            QTimer.singleShot(3000, self._getStatus)
            self.upgrade_dg.close()

    def _getFileHash_SHA256(self, file_path):
        if os.path.isfile(file_path):
            s = sha256()
            with open(file_path, "rb") as f:
                while True:
                    data = f.read(8 * 1024)
                    if not data:
                        break
                    s.update(data)
            return s.hexdigest()

    def onUpgradeDialogFileSelect(self, event):
        fd = QFileDialog()
        file_path, _ = fd.getOpenFileName(filter="BIN 文件 (*.bin)")
        if file_path:
            self.upgrade_dg_lb.setText(file_path)
            self.last_firm_path = file_path
            self.upgrade_dg.setWindowTitle(f"固件升级 | {self._getFileHash_SHA256(file_path)}")

    def onUPgradeDialogCheck(self, event):
        content, filename, datetime_obj, version = get_latest_app_bin()
        if filename:
            total_seconds = (datetime_obj - self.device_datetime).total_seconds()
            logger.debug(f"{self.device_datetime} | {datetime_obj} | {total_seconds}")
            if total_seconds < 10:
                self.upgrade_dg.setWindowTitle("固件升级 | 无可更新版本")
                return
            r_file_path = f"data/{filename}"
            with open(r_file_path, "wb") as f:
                f.write(content)
            file_path = os.path.abspath(r_file_path)
            self.upgrade_dg_lb.setText(file_path)
            self.last_firm_path = file_path
            self.upgrade_dg.setWindowTitle(f"固件升级 | {self._getFileHash_SHA256(file_path)}")
        else:
            self.upgrade_dg.setWindowTitle("固件升级 | 检查失败")

    def onSampleOver(self):
        self.stop_matplot_timer()
        if not self.sample_record_lable_name:
            return
        if self.sample_record_lable_name.startswith("Correct "):
            logger.success("correct sample over read flash back")
            self.onOutFlashParamRead()
        records = []
        for k in sorted(self.matplot_data.keys()):
            v = self.matplot_data.get(k, [0])
            records.append(f"{k} | {v}")
        cnt = self.sample_db.get_label_cnt()
        if cnt > 1:
            self.sample_record_idx_sp.setRange(0, cnt - 1)
            self.sample_record_idx_sp.setValue(cnt - 1)
            self.sample_record_pre_bt.setText(f"序号[{cnt - 1}]")
        else:
            self.sample_record_plot_by_index(0)
        if not os.path.isfile(self.data_xlsx_path) or not check_file_permission(self.data_xlsx_path):
            dump_sample(self.sample_db.iter_all_data(num=20), self.data_xlsx_path)
        elif (not self.lamp_ag_cb.isChecked()) and (not self.debug_flag_cbs[5].isChecked()):
            # insert_sample(self.sample_db.iter_from_label(), self.data_xlsx_path)
            dump_sample(self.sample_db.iter_all_data(num=20), self.data_xlsx_path)
        if self.lamp_ag_cb.isChecked():
            self.onMatplotStart(False, f"Aging {datetime.now().strftime('%Y%m%d%H%M%S')}")
        if self.debug_flag_cbs[5].isChecked():
            self.sample_record_lable_name = datetime.now().strftime("%Y%m%d%H%M%S")
            logger.debug(f"create new label | {self.sample_record_lable_name}")
            self.sample_label = self.sample_db.build_label(
                name=self.sample_record_lable_name,
                version=f"{self.version}.{datetime.strftime(self.device_datetime, '%Y%m%d.%H%M%S')}",
                device_id=self.device_id,
            )
        try:
            pyperclip.copy("\n".join(records))
        except Exception:
            pass

    def updateMatplotData(self, info):
        length = info.content[6]
        channel = info.content[7]
        logger.success(f"channel finish | {time.time() - self.matplot_timer_time_list[(channel - 1) % 6]:.2f} S")
        if len(info.content) - 9 == 2 * length:  # 普通采样 unsigned int
            data = tuple((struct.unpack("H", info.content[8 + i * 2 : 10 + i * 2])[0] for i in range(length)))
        elif len(info.content) - 9 == 4 * length:  # PD OD unsigned long
            data = tuple((struct.unpack("I", info.content[8 + i * 4 : 12 + i * 4])[0] for i in range(length)))
        elif len(info.content) - 9 == 10 * length:  # PD OD Sample  unsigned long unsigned long unsigned short
            data = []
            for i in range(length):
                data.append(struct.unpack("I", info.content[8 + i * 10 + 0 : 8 + i * 10 + 4])[0])
            for i in range(length):
                data.append(struct.unpack("I", info.content[8 + i * 10 + 4 : 8 + i * 10 + 8])[0])
            for i in range(length):
                data.append(struct.unpack("H", info.content[8 + i * 10 + 8 : 8 + i * 10 + 10])[0])
            logger.debug(f"hit data | {data} | {bytesPuttyPrint(info.content)}")
        elif len(info.content) - 9 == 12 * length:  # PD OD Sample  unsigned long unsigned long unsigned short
            data = []
            for i in range(length):
                data.append(struct.unpack("I", info.content[8 + i * 12 + 0 : 8 + i * 12 + 4])[0])
            for i in range(length):
                data.append(struct.unpack("I", info.content[8 + i * 12 + 4 : 8 + i * 12 + 8])[0])
            for i in range(length):
                data.append(struct.unpack("H", info.content[8 + i * 12 + 8 : 8 + i * 12 + 10])[0])
            for i in range(length):
                data.append(struct.unpack("H", info.content[8 + i * 12 + 10 : 8 + i * 12 + 12])[0])
            logger.debug(f"hit data | {data} | {bytesPuttyPrint(info.content)}")
        else:
            logger.error(f"error data length | {len(info.content)} --> {length} | {info.text}")
            return
        self.plot_graph.plot_data_new(data=data, name=f"B-{channel}")
        self.matplot_data[channel] = data
        if self.sample_record_lable_name.startswith("Correct ") and len(self.sample_datas) >= 6:
            wave = self.sample_confs[channel - 1].wave + 1
            logger.debug(f"change wave from {self.sample_confs[channel - 1].wave} to {wave}")
        else:
            wave = self.sample_confs[channel - 1].wave
        method = self.sample_confs[channel - 1].method
        set_info = self.sample_confs[channel - 1].set_info
        raw_data = info.content[8:-1]
        sample_data = self.sample_db.build_sample_data(
            datetime.now(), channel=channel, set_info=set_info, method=MethodEnum(method), wave=WaveEnum(wave), total=length, raw_data=raw_data
        )
        self.sample_datas.append(sample_data)
        logger.debug(f"get data in channel | {channel} | {data}")
        self.sample_db.bind_label_sample_data(self.sample_label, sample_data)

    def createStorgeDialog(self):
        self.id_card_data_dg = QDialog(self)
        self.id_card_data_dg.setWindowTitle("ID Card")
        self.id_card_data_dg.resize(730, 370)
        self.id_card_data_dg.resizeEvent = lambda x: logger.debug(f"windows size | {self.id_card_data_dg.size()}")
        id_card_data_ly = QVBoxLayout(self.id_card_data_dg)
        self.id_card_data_te = QTextEdit()
        id_card_data_ly.addWidget(self.id_card_data_te)

        id_card_temp_ly = QHBoxLayout()
        self.id_card_data_read_bt = QPushButton("读取")
        self.id_card_data_write_bt = QPushButton("写入")
        id_card_temp_ly.addWidget(self.id_card_data_read_bt)
        id_card_temp_ly.addWidget(self.id_card_data_write_bt)
        id_card_data_ly.addLayout(id_card_temp_ly)

        self.id_card_data_read_bt.clicked.connect(self.onID_CardRead)
        self.id_card_data_write_bt.clicked.connect(self.onID_CardWrite)
        self.id_card_data_dg = ModernDialog(self.id_card_data_dg, self)

        self.out_flash_data_dg = QDialog(self)
        self.out_flash_data_dg.setWindowTitle("外部Flash")
        self.out_flash_data_dg.resize(730, 370)
        out_flash_data_ly = QVBoxLayout(self.out_flash_data_dg)
        self.out_flash_data_te = QTextEdit()
        out_flash_temp_ly = QHBoxLayout()
        self.out_flash_data_addr = QSpinBox(minimum=0, maximum=8 * 2 ** 20, maximumWidth=90, value=4096, singleStep=4096)
        self.out_flash_data_num = QSpinBox(minimum=0, maximum=8 * 2 ** 20, maximumWidth=90, value=636, singleStep=1440 * 6 + 120 * 6 - 636)
        self.out_flash_data_read_bt = QPushButton("读取", clicked=self.onOutFlashRead, maximumWidth=90)

        out_flash_temp_ly.addWidget(QLabel("地址", maximumWidth=60))
        out_flash_temp_ly.addWidget(self.out_flash_data_addr)
        out_flash_temp_ly.addWidget(QLabel("数量", maximumWidth=60))
        out_flash_temp_ly.addWidget(self.out_flash_data_num)
        out_flash_temp_ly.addSpacing(2)
        out_flash_temp_ly.addWidget(self.out_flash_data_read_bt)

        out_flash_param_gb = QGroupBox("系统参数")
        out_flash_param_ly = QVBoxLayout(out_flash_param_gb)
        out_flash_param_ly.setSpacing(5)
        self.out_flash_param_read_bt = QPushButton("读取", clicked=self.onOutFlashParamRead, maximumWidth=90)
        self.out_flash_param_write_bt = QPushButton("写入", clicked=self.onOutFlashParamWrite, maximumWidth=90)
        self.out_flash_param_clear_o_bt = QPushButton("清除测试点", clicked=self.onOutFlashParamCC_O, maximumWidth=120)
        self.out_flash_param_clear_s_bt = QPushButton("清除标准点", clicked=self.onOutFlashParamCC_S, maximumWidth=120)
        self.out_flash_param_clear_s_d_bt = QPushButton("预设标准点", clicked=self.onOutFlashParamCC_S_D, maximumWidth=120)
        self.out_flash_param_dump_bt = QPushButton("Dump", clicked=self.onOutFlashParamCC_Dump, maximumWidth=90)
        self.out_flash_param_load_bt = QPushButton("Load", clicked=self.onOutFlashParamCC_Load, maximumWidth=90)
        self.out_flash_param_debug_bt = QPushButton("Debug", clicked=self.onOutFlashParamCC_Debug, maximumWidth=90)
        self.out_flash_param_check_bt = QPushButton("Check", clicked=self.onOutFlashParamCC_Check, maximumWidth=90)

        out_flash_param_od_cc_wg = QGroupBox("OD校正参数")
        out_flash_param_od_cc_ly = QVBoxLayout(out_flash_param_od_cc_wg)
        self.out_flash_param_cc_sps = [QDoubleSpinBox(self, minimum=0, maximum=99999999, decimals=0, value=0) for i in range(156)]
        for i in range(6):
            if i == 0:
                pp = 3
            else:
                pp = 2
            out_flash_param_od_cc_ly.addWidget(QHLine())
            for j in range(pp):
                wave = WAVE_NAMES[j]
                temp_ly = QHBoxLayout()
                if j == 0:
                    channel = f"通道{i + 1}"
                else:
                    channel = "     "
                temp_ly.addWidget(QLabel(f"{channel} {wave}"))
                for k in range(6):
                    temp_ly.addWidget(QVLine())
                    head = k * 1 + j * 12 + i * 24 + 36 - 12 * pp
                    # self.out_flash_param_cc_sps[head].setMaximumWidth(45)
                    # self.out_flash_param_cc_sps[head + 6].setMaximumWidth(60)
                    temp_ly.addWidget(self.out_flash_param_cc_sps[head])
                    temp_ly.addWidget(self.out_flash_param_cc_sps[head + 6])
                out_flash_param_od_cc_ly.addLayout(temp_ly)
        self.updateOutFlashParamSpinBG()
        out_flash_param_ly.addWidget(out_flash_param_od_cc_wg)

        out_flash_data_ly.addWidget(self.out_flash_data_te)
        out_flash_data_ly.addLayout(out_flash_temp_ly)
        out_flash_data_ly.addWidget(out_flash_param_gb)

        temp_ly = QHBoxLayout()
        temp_ly.setContentsMargins(5, 0, 5, 0)
        temp_ly.setSpacing(5)
        temp_ly.addWidget(self.out_flash_param_read_bt)
        temp_ly.addWidget(self.out_flash_param_write_bt)
        temp_ly.addWidget(self.out_flash_param_clear_o_bt)
        temp_ly.addWidget(self.out_flash_param_clear_s_bt)
        temp_ly.addWidget(self.out_flash_param_clear_s_d_bt)
        temp_ly.addWidget(self.out_flash_param_dump_bt)
        temp_ly.addWidget(self.out_flash_param_load_bt)
        temp_ly.addWidget(self.out_flash_param_debug_bt)
        temp_ly.addWidget(self.out_flash_param_check_bt)
        out_flash_data_ly.addLayout(temp_ly)

        self.out_flash_data_dg = ModernDialog(self.out_flash_data_dg, self)

    def onOutFlashParamCC_O(self, event):
        """清除测试点"""
        for idx, sp in enumerate(self.out_flash_param_cc_sps):
            if idx % 12 >= 6:
                sp.setValue(0)

    def onOutFlashParamCC_S(self, event):
        """清除标准点"""
        for idx, sp in enumerate(self.out_flash_param_cc_sps):
            if idx % 12 < 6:
                sp.setValue(0)

    def onOutFlashParamCC_S_D(self, event):
        """预设标准点"""
        for idx, sp in enumerate(self.out_flash_param_cc_sps):
            if idx % 12 < 6:
                if idx // 12 == 0:
                    sp.setValue(CONFIG.get("sh_criterion", {}).get("610", 0)[idx % 6])
                elif idx // 12 == 1:
                    sp.setValue(CONFIG.get("sh_criterion", {}).get("550", 0)[idx % 6])
                elif idx // 12 == 2:
                    sp.setValue(CONFIG.get("sh_criterion", {}).get("405", 0)[idx % 6])
                elif idx // 12 >= 3:
                    sp.setValue(self.out_flash_param_cc_sps[idx - 12 * ((idx + 12) // 24 * 2 - 1)].value())

    def _getOutFlashParamCC_Data(self):
        data = [
            TEMP_CC_DataInfo(
                top=self.out_flash_param_temp_sps[0].value(), btm=self.out_flash_param_temp_sps[1].value(), env=self.out_flash_param_temp_sps[2].value()
            )
        ]
        chanel_d = dict()
        for c in range(6):  # channel
            waves = 3 if c == 0 else 2
            c_idx = c * 24 + 12 - (12 if c == 0 else 0)
            chanel_d[f"CH-{c+1}"] = dict()
            for w in range(waves):  # 610 550 405?
                pairs = []
                w_idx = c_idx + 12 * w
                for s in range(6):  # s1 ~ s6
                    idx = w_idx + s
                    theo = int(self.out_flash_param_cc_sps[idx].value())
                    test = int(self.out_flash_param_cc_sps[idx + 6].value())
                    pairs.append((theo, test))
                chanel_d[f"CH-{c+1}"][WAVE_NAMES[w]] = pairs
        for wave in WAVE_NAMES:
            standard_pointses = []
            channel_pointses = []
            for c in range(6):
                channel_points = [chanel_d[f"CH-{c + 1}"][wave][i][1] for i in range(6)]
                channel_pointses.append(channel_points)
                standard_points = [chanel_d[f"CH-{c + 1}"][wave][i][0] for i in range(6)]
                standard_pointses.append(standard_points)
                if wave == WAVE_NAMES[-1]:
                    break
            data.append(ILLU_CC_DataInfo(wave=wave, standard_pointses=standard_pointses, channel_pointses=channel_pointses))
        return data

    def onOutFlashParamCC_Dump(self, event):
        fd = QFileDialog()
        file_path, _ = fd.getSaveFileName(filter="Excel 工作簿 (*.xlsx)", directory=os.path.join(self.last_falsh_save_dir, f"flash_{self.device_id}.xlsx"))
        if not file_path:
            return
        else:
            if file_path:
                self.last_falsh_save_dir = os.path.split(file_path)[0]
        data = self._getOutFlashParamCC_Data()
        dump_CC(data, file_path)

    def onOutFlashParamCC_Load(self, event):
        fd = QFileDialog()
        file_path, _ = fd.getOpenFileName(filter="Excel 工作簿 (*.xlsx)")
        if not file_path:
            return
        data = load_CC(file_path)
        if not data:
            return
        for d in data:
            if isinstance(d, TEMP_CC_DataInfo):
                self.out_flash_param_temp_sps[0].setValue(d.top)
                self.out_flash_param_temp_sps[1].setValue(d.btm)
                self.out_flash_param_temp_sps[2].setValue(d.env)
            elif isinstance(d, ILLU_CC_DataInfo):
                w_idx = WAVE_NAMES.index(str(d.wave))
                for c_idx, standard_points in enumerate(d.standard_pointses):
                    start = w_idx * 12 + 24 * c_idx + (12 if c_idx > 0 else 0)
                    for i in range(6):
                        self.out_flash_param_cc_sps[start + i].setValue(standard_points[i])

                for c_idx, channel_points in enumerate(d.channel_pointses):
                    start = w_idx * 12 + 24 * c_idx + (12 if c_idx > 0 else 0)
                    for i in range(6):
                        self.out_flash_param_cc_sps[start + i + 6].setValue(channel_points[i])

    def onOutFlashParamCC_Debug(self, event):
        for idx, sp in enumerate(self.out_flash_param_cc_sps):
            if idx % 12 < 6:
                p_sp = self.out_flash_param_cc_sps[idx + 6]
                sp.setValue(p_sp.value())

    def onOutFlashParamCC_Check(self, event):
        rank_list = []
        temp = []
        for idx, sp in enumerate(self.out_flash_param_cc_sps):
            if idx % 12 == 0:
                if temp:
                    rank_list.append(temp[:])
                temp.clear()
            if idx % 12 >= 6:
                temp.append(sp.value())
        rank_list.append(temp[:])
        logger.debug(f"get rank list. \n{rank_list}")
        error_flag = False
        for idx, rank in enumerate(rank_list):
            if idx == 2:
                continue
            diff = list(map(lambda a, b: abs(a - b), rank[1:], rank[:-1]))
            if any((d < 1600 for d in diff)):
                error_flag = True
                msg = ModernMessageBox(self)
                msg.setIcon(QMessageBox.Warning)
                msg.setWindowTitle(f"定标数据异常 | {idx + 1}")
                msg.setText(f"{rank}\n{diff}")
                msg.show()
        if error_flag is False:
            msg = ModernMessageBox(self)
            msg.setIcon(QMessageBox.Information)
            msg.setWindowTitle("定标数据正常")
            msg.setText(">1600")
            msg.show()

    def updateFlashCC_Plot(self, refresh=True):
        logger.debug(f"refresh | {refresh} | self.flash_json_data is None {self.flash_json_data is None}")
        self.out_flash_data_parse_dg.setWindowTitle(f"校正曲线-----{self.flash_plot_wave}")
        if refresh or self.flash_json_data is None:
            fd = QFileDialog()
            file_path, _ = fd.getOpenFileName(filter="Excel 工作簿 (*.xlsx)")
            if not file_path:
                return
            data = load_CC(file_path)
            if not data:
                return
            self.flash_json_data = data
        else:
            data = self.flash_json_data
        xss = []
        yss = []
        for d in data:
            if isinstance(d, ILLU_CC_DataInfo):
                if d.wave != int(self.flash_plot_wave):
                    continue
                xss = d.standard_pointses
                yss = d.channel_pointses
                break
        self.flash_plot_graph.clear_plot()
        self.flash_plot_graph.plot_data_new(name="标准值", color="DAEDF0")
        for i in range(6):
            self.flash_plot_graph.plot_data_new(name=f"SCH-{i + 1}")
            self.flash_plot_graph.plot_data_new(name=f"RCH-{i + 1}")
        for i, xs in enumerate(xss):
            logger.debug(f"load xs | {i} | {xs}")
            for x in xs:
                self.flash_plot_graph.plot_data_update(i * 2 + 0, x)
        for i, ys in enumerate(yss):
            logger.debug(f"load ys | {i} | {ys}")
            for y in ys:
                self.flash_plot_graph.plot_data_update(i * 2 + 1, y)
        self.flash_json_data = data

    def updateFlashCC_PlotSelectWave(self):
        # logger.debug(f"self.out_flash_plot_ccw.currentData() | {self.out_flash_plot_ccw.currentText()}")
        self.flash_plot_wave = self.out_flash_plot_ccw.currentText()
        logger.debug(f"invoke | {self.updateFlashCC_Plot}")
        self.updateFlashCC_Plot(refresh=False)

    def onOutFlashParamCC_parse(self):
        self.out_flash_data_parse_dg = QDialog(self)
        self.flash_plot_channel = 1
        self.flash_plot_wave = 610
        parse_ly = QVBoxLayout(self.out_flash_data_parse_dg)
        self.flash_plot_graph = CC_Graph(parent=self.out_flash_data_parse_dg)
        parse_ly.addWidget(self.flash_plot_graph.win)
        bt_ly = QHBoxLayout()
        parse_ly.addLayout(bt_ly)
        bt_ly.setAlignment(Qt.AlignRight)
        bt_ly.addWidget(QLabel("波长"))
        self.out_flash_plot_ccw = QComboBox(maximumWidth=90, currentIndexChanged=self.updateFlashCC_PlotSelectWave)
        self.out_flash_plot_ccw.blockSignals(True)
        self.out_flash_plot_ccw.addItems(("610", "550"))
        self.out_flash_plot_ccw.blockSignals(False)
        bt_ly.addWidget(self.out_flash_plot_ccw)
        logger.debug(f"invoke | {self.updateFlashCC_Plot}")
        self.updateFlashCC_Plot()
        self.out_flash_data_parse_dg = ModernDialog(self.out_flash_data_parse_dg, self)
        self.out_flash_data_parse_dg.resize(900, 400)
        self.out_flash_data_parse_dg.show()

    def onOutFlashParamRead(self, event=None):
        data = (*(struct.pack("H", 0)), *(struct.pack("H", 159)))
        self._serialSendPack(0xDD, data)
        QTimer.singleShot(1000, self.updateOutFlashParamSpinBG)

    def onOutFlashParamWrite(self, event=None):
        data = []
        for idx, sp in enumerate(self.out_flash_param_cc_sps):
            for d in struct.pack("I", int(sp.value())):
                data.append(d)
        for i in range(0, len(data), 224):
            sl = data[i : i + 224]
            start = [*struct.pack("H", (i // 4) + 3)]
            num = [*struct.pack("H", len(sl) // 4)]
            self._serialSendPack(0xDD, start + num + sl)
        data = (0xFF, 0xFF)  # 保存参数 写入Flash
        self._serialSendPack(0xDD, data)
        for sp in self.out_flash_param_cc_sps:
            self._setColor(sp, nfg="red")
        QTimer.singleShot(500, partial(self.onOutFlashParamRead, event=False))

    def updateOutFlashParamSpinBG(self):
        for idx, sp in enumerate(self.out_flash_param_cc_sps):
            if idx % 12 >= 6:  # 测试点
                cid = 15 - (idx % 12 - 6) * 3
                row = idx // 12
                if row in (0, 3, 5, 7, 9, 11):
                    nbg = ColorReds[cid].hex
                elif row in (1, 4, 6, 8, 10, 12):
                    nbg = ColorGreens[cid].hex
                else:
                    nbg = ColorPurples[cid].hex
                nfg = "white"
                self._setColor(sp, nbg, nfg)
            else:  # 标准点
                nbg = "blue"
                nfg = "yellow"
                self._setColor(sp, nbg, nfg)

    def updateOutFlashParam(self, info):
        raw_pack = info.content
        start = struct.unpack("H", raw_pack[6:8])[0]
        num = struct.unpack("H", raw_pack[8:10])[0]
        logger.debug(f"param pack | start {start} | num {num} | raw_pack_len {len(raw_pack)}")
        if start < 0x5000:
            for i in range(num):
                idx = start + i
                if idx < len(self.out_flash_param_temp_sps):
                    value = struct.unpack("f", raw_pack[10 + i * 4 : 14 + i * 4])[0] * -1
                    sp = self.out_flash_param_temp_sps[idx]
                else:
                    value = struct.unpack("I", raw_pack[10 + i * 4 : 14 + i * 4])[0]
                    sp = self.out_flash_param_cc_sps[idx - len(self.out_flash_param_temp_sps)]
                sp.setValue(value)
                self._setColor(sp)
        elif start == 0x5000:
            sample_led_pd_buffer = struct.unpack("I" * 18, raw_pack[10:82])
            sample_led_dac_buffer = struct.unpack("H" * 3, raw_pack[82:88])
            logger.info(f"采集板 LED 白板 PD 值 | {sample_led_pd_buffer}")
            logger.info(f"采集板 LED 电压值 值 | {sample_led_dac_buffer}")

    def genBinaryData(self, data, unit=32, offset=0):
        result = []
        for i in range(0, len(data), unit):
            b = data[i : i + unit]
            result.append(f"0x{i + offset:04X} ~ 0x{i + offset + unit - 1:04X} | {bytesPuttyPrint(b)}")
        return result

    def onID_CardDialogShow(self, event):
        self.id_card_data_dg.show()

    def onID_CardRead(self, event):
        self.id_card_data_dg.setWindowTitle("ID Card")
        self.id_card_data_te.clear()
        self._serialSendPack(0x06)

    def onID_CardWrite(self, event):
        fd = QFileDialog()
        file_path, _ = fd.getOpenFileName()
        if not file_path:
            return
        file_size = os.path.getsize(file_path)
        if file_size > 4096:
            msg = ModernMessageBox(self, timeout=5)
            msg.setIcon(QMessageBox.Warning)
            msg.setWindowTitle(f"文件大小异常 | {file_size}")
            msg.setText("只取头部4096字节数据")
            msg.show()
        with open(file_path, "rb") as f:
            data = f.read(4096)
        for a, i in enumerate(range(len(data) // 224, -1, -1)):
            addr = a * 224
            if i != 0:
                num = 224
            else:
                num = len(data) % 224
            addr_list = ((addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF)
            num_list = ((num >> 16) & 0xFF, (num >> 8) & 0xFF, num & 0xFF)
            data_list = (i for i in data[addr : addr + num])
            self._serialSendPack(0xD9, (*addr_list, *num_list, *data_list))

    def updateIDCardData(self, info):
        offset = 0
        raw_bytes = info.content
        start = int.from_bytes(raw_bytes[8:10], byteorder="little")
        length = raw_bytes[10]
        if start == 0:
            self.storge_time_start = time.time()
            total = int.from_bytes(raw_bytes[6:8], byteorder="little")
            offset = total - len(self.id_card_data)
        elif start + length > len(self.id_card_data):
            offset = start + length - len(self.id_card_data)
        if offset > 0:
            self.id_card_data.extend((0xFF for _ in range(offset)))
        self.id_card_data[start : start + length] = raw_bytes[11 : 11 + length]
        if start + length == 4096:
            self.id_card_data_dg.setWindowTitle(f"ID Card | sha256 {sha256(self.id_card_data).hexdigest()} | {time.time() - self.storge_time_start:.4f} S")
        result = self.genBinaryData(self.id_card_data[: start + length])
        raw_text = "\n".join(result)
        # logger.debug(f"ID Card Raw Data \n{raw_text}")
        self.id_card_data_te.setPlainText(raw_text)

    def onOutFlashDialogShow(self, event):
        self.out_flash_data_dg.show()

    def onOutFlashRead(self, event):
        addr = self.out_flash_data_addr.value()
        addr_list = ((addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF)
        num = self.out_flash_data_num.value()
        num_list = ((num >> 16) & 0xFF, (num >> 8) & 0xFF, num & 0xFF)
        self.out_flash_start = addr
        self.out_flash_length = num
        self.out_flash_data.clear()
        self.out_flash_data.extend((0xFF for _ in range(num)))
        self.out_flash_data_te.clear()
        self._serialSendPack(0xD6, (*addr_list, *num_list))

    def updateOutFlashData(self, info):
        raw_bytes = info.content
        start = int.from_bytes(raw_bytes[9:12], byteorder="little") - self.out_flash_start
        length = raw_bytes[12]
        self.out_flash_data[start : start + length] = raw_bytes[13 : 13 + length]
        self.out_flash_data_dg.setWindowTitle(f"外部Flash | sha256 {sha256(self.out_flash_data).hexdigest()}")
        result = self.genBinaryData(self.out_flash_data[: start + length], offset=self.out_flash_start)
        raw_text = "\n".join(result)
        logger.debug(f"Out Flash Raw Data | {start} | {length}\n{raw_text}")
        if self.out_flash_start == 0x1000 and self.out_flash_length == 636:
            info = DC201_ParamInfo(self.out_flash_data[: start + length])
            plain_text = f"{info}\n{'=' * 100}\nraw bytes:\n{raw_text}"
            self.out_flash_data_te.setPlainText(plain_text)
        elif self.out_flash_start == 0x2000 and start + length == 1440 * 6 + 120 * 6:
            info, correct_list = parse_1440(self.out_flash_data[: start + length])
            plain_text = f"{info}\n{'=' * 100}\nraw bytes:\n{raw_text}"
            self.out_flash_data_te.setPlainText(plain_text)
            for cu in correct_list:
                logger.debug(cu)
            dump_correct_record(correct_list, f"data/correct_{self.device_id}.xlsx")
        else:
            self.out_flash_data_te.setPlainText(raw_text)

    def onDebugFlagChanged(self, event=None, idx=0):
        logger.debug(f"invoke onDebugFlagChanged | event {event} | idx {idx}")
        if event:
            self._serialSendPack(0xD4, ((1 << idx), 1))
        else:
            self._serialSendPack(0xD4, ((1 << idx), 0))

    def updateDebugFlag(self, info):
        if len(info.content) == 8:
            data = info.content[6]
            for i, cb in enumerate(self.debug_flag_cbs):
                if data & (1 << i):
                    cb.setChecked(True)
                else:
                    cb.setChecked(False)
        else:
            data = [struct.unpack("H", info.content[8 + 2 * i : 10 + 2 * i])[0] for i in range(info.content[6])]
            logger.debug(f"DebugTest Data channel {self.debugtest_cnt} | {data}")
            self.debugtest_bt.setToolTip(f"{data}")

    def onLampSL(self):
        self._serialSendPack(0xDC, (2,))

    def onLampBP(self):
        self._serialSendPack(0xDC, (1,))
        self.sample_record_lable_name = f"Lamp BP {datetime.now().strftime('%Y%m%d%H%M%S')}"
        logger.debug(f"set lamp bp label name | {self.sample_record_lable_name}")
        conf = []
        self.sample_confs = []
        self.sample_datas = []
        for i in range(6):
            conf.append(1)
            conf.append(1)
            conf.append(12)
            self.sample_confs.append(SampleConf(conf[-3], conf[-2], conf[-1], "Lamp BP"))
        logger.debug(f"get matplot cnf | {conf}")
        self.sample_label = self.sample_db.build_label(
            name=self.sample_record_lable_name, version=f"{self.version}.{datetime.strftime(self.device_datetime, '%Y%m%d.%H%M%S')}", device_id=self.device_id
        )
        self.plot_graph.clear_plot()
        self.matplot_data.clear()

    def createSysConf(self):
        self.sys_conf_gb = QGroupBox("系统")
        sys_conf_ly = QVBoxLayout(self.sys_conf_gb)
        sys_conf_ly.setContentsMargins(3, 3, 3, 3)
        sys_conf_ly.setSpacing(0)

        storge_ly = QHBoxLayout()
        storge_ly.setContentsMargins(3, 3, 3, 3)
        storge_ly.setSpacing(3)
        self.storge_gb = QWidget()
        self.storge_gb.setLayout(QGridLayout())
        self.storge_gb.layout().setContentsMargins(3, 3, 3, 3)
        self.storge_gb.layout().setSpacing(3)
        self.storge_id_card_dialog_bt = QPushButton("ID Code 卡")
        self.storge_id_card_dialog_bt.setMaximumWidth(80)
        self.storge_flash_read_bt = QPushButton("外部 Flash")
        self.storge_flash_read_bt.setMaximumWidth(100)
        self.storge_gb.layout().addWidget(self.storge_id_card_dialog_bt, 0, 0)
        self.storge_gb.layout().addWidget(self.storge_flash_read_bt, 0, 1)

        self.debug_flag_gb = QWidget()
        debug_flag_ly = QGridLayout(self.debug_flag_gb)
        debug_flag_ly.setSpacing(0)
        debug_flag_ly.setContentsMargins(3, 3, 3, 3)
        self.debug_flag_cbs = (QCheckBox("温度"), QCheckBox("告警"), QCheckBox("扫码"), QCheckBox("托盘"), QCheckBox("原值"), QCheckBox("老化"))
        for i, cb in enumerate(self.debug_flag_cbs):
            cb.setMaximumWidth(45)
            debug_flag_ly.addWidget(cb, i // 6, i % 6)
        storge_ly.addWidget(self.storge_gb)
        storge_ly.addWidget(self.debug_flag_gb)

        self.storge_id_card_dialog_bt.clicked.connect(self.onID_CardDialogShow)
        self.storge_flash_read_bt.clicked.connect(self.onOutFlashDialogShow)
        for i, cb in enumerate(self.debug_flag_cbs):
            cb.clicked.connect(partial(self.onDebugFlagChanged, idx=i))
        sys_conf_ly.addLayout(storge_ly)

        boot_ly = QHBoxLayout()
        boot_ly.setContentsMargins(3, 3, 3, 3)
        boot_ly.setSpacing(0)
        self.upgrade_bt = QPushButton("固件", maximumWidth=50, clicked=self.onUpgrade)
        self.bootload_bt = QPushButton("BL", maximumWidth=40, clicked=self.onBootload)
        self.sample_fr_bt = QPushButton("采样板", maximumWidth=60, clicked=self.onSampleFr)
        self.reboot_bt = QPushButton("重启", maximumWidth=50, clicked=self.onReboot)
        self.selftest_bt = QPushButton("自检", maximumWidth=50)
        self.selftest_bt.mousePressEvent = self.onSelfCheck  # 区分鼠标按键
        self.debugtest_bt = QPushButton("定标", maximumWidth=50, clicked=self.onDebugTest)
        self.debug_aging_sleep_sp = QSpinBox(minimum=0, maximum=255, value=10, maximumWidth=50, suffix="S", valueChanged=self.on_debug_aging_sleep_sp)
        self.debugtest_cnt = 0
        boot_ly.addWidget(self.upgrade_bt)
        boot_ly.addWidget(self.bootload_bt)
        boot_ly.addWidget(self.sample_fr_bt)
        boot_ly.addWidget(self.reboot_bt)
        boot_ly.addWidget(self.selftest_bt)
        boot_ly.addWidget(self.debugtest_bt)
        boot_ly.addWidget(QLabel("间隔", maximumWidth=50))
        boot_ly.addWidget(self.debug_aging_sleep_sp)
        sys_conf_ly.addLayout(boot_ly)

    def on_debug_aging_sleep_sp(self, event):
        self._serialSendPack(0xD4, (event,))

    def onDebugTest(self, event):
        self.onCorrectMatplotStart()
        self._serialSendPack(0xD2, (0xFF,))

    def getErrorContent(self, error_code):
        for i in DC201ErrorCode:
            if error_code == i.value[0]:
                logger.error(f"hit error | {i.value[0]:05d} | {i.value[1]}")
                return i.value[1]
        for i in (DC201ErrorCode.eError_Comm_Main_UART, DC201ErrorCode.eError_Comm_Data_UART, DC201ErrorCode.eError_Comm_Out_UART):
            if (error_code >> 10) > 0 and error_code & 0xFF == i.value[0]:
                logger.error(f"hit serial dma error | {i.value[0]:05d} | {i.value[1]}")
                return f"{i.value[1]} | 硬件故障码 0x{(error_code >> 10):02X}"

        logger.error(f"unknow error code | {error_code}")
        return "Unknow Error Code"

    def on_warn_msgbox_close(self):
        self.warn_msgbox_cnt -= 1
        logger.debug(f"close msg box | nogoru wa {self.warn_msgbox_cnt}")

    def showWarnInfo(self, info):
        error_code = struct.unpack("H", info.content[6:8])[0]
        error_content = self.getErrorContent(error_code)
        level = QMessageBox.Warning
        if error_code in (122, 123):
            timeout = 5
        elif self.warn_msgbox_cnt > 5:
            timeout = 5
        else:
            timeout = nan
        msg = ModernMessageBox(self, timeout=timeout)
        self.warn_msgbox_cnt += 1
        msg.close_callback.connect(self.on_warn_msgbox_close)
        msg.setIcon(level)
        msg.setWindowTitle(f"故障信息 | {datetime.now()}")
        msg.setText(f"故障码 {error_code}\n{error_content}")
        msg.show()


if __name__ == "__main__":

    def trap_exc_during_debug(exc_type, exc_value, exc_traceback):
        t = "\n".join(traceback.format_exception(exc_type, exc_value, exc_traceback))
        logger.error(f"sys execpt hook\n{t}")

    sys.excepthook = trap_exc_during_debug
    try:
        app = QApplication(sys.argv)
        # app.setStyle('Windows')

        # app.setStyle("Fusion")
        # dark_palette = QPalette()
        # dark_palette.setColor(QPalette.Window, QColor(53, 53, 53))
        # dark_palette.setColor(QPalette.WindowText, Qt.white)
        # dark_palette.setColor(QPalette.Base, QColor(25, 25, 25))
        # dark_palette.setColor(QPalette.AlternateBase, QColor(53, 53, 53))
        # dark_palette.setColor(QPalette.ToolTipBase, Qt.white)
        # dark_palette.setColor(QPalette.ToolTipText, Qt.white)
        # dark_palette.setColor(QPalette.Text, Qt.white)
        # dark_palette.setColor(QPalette.Button, QColor(53, 53, 53))
        # dark_palette.setColor(QPalette.ButtonText, Qt.white)
        # dark_palette.setColor(QPalette.BrightText, Qt.red)
        # dark_palette.setColor(QPalette.Link, QColor(42, 130, 218))
        # dark_palette.setColor(QPalette.Highlight, QColor(42, 130, 218))
        # dark_palette.setColor(QPalette.HighlightedText, Qt.black)
        # app.setPalette(dark_palette)
        # app.setStyleSheet("QToolTip { color: #ffffff; background-color: #2a82da; border: 1px solid white; }")

        window = MainWindow()

        qtmodern.styles.dark(app)
        window = qtmodern.windows.ModernWindow(window)

        window.show()
        app.setWindowIcon(QIcon(ICON_PATH))
        app.exec_()
    except Exception:
        logger.error(f"exception in main loop \n{stackprinter.format()}")
