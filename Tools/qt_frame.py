# http://doc.qt.io/qt-5/qt.html

import json
import os
import queue
import random
import struct
import sys
import time
import traceback
from collections import namedtuple
from datetime import datetime
from functools import partial
from hashlib import sha256

import loguru
import numpy as np
import pyperclip
import serial
import serial.tools.list_ports
import stackprinter
import statistics
from PyQt5.QtCore import Qt, QThreadPool, QTimer
from PyQt5.QtGui import QFont, QIcon, QPalette
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
    QSizePolicy,
    QSlider,
    QSpacerItem,
    QSpinBox,
    QStatusBar,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)
from pyqtgraph import GraphicsLayoutWidget, LabelItem, SignalProxy, mkPen

from bytes_helper import best_fit_slope_and_intercept, bytes2Float, bytesPuttyPrint
from dc201_pack import DC201_PACK, DC201_ParamInfo, DC201ErrorCode, write_firmware_pack_BL, write_firmware_pack_FC
import qtmodern.styles
import qtmodern.windows
from qt_modern_dialog import ModernDialog, ModernMessageBox
from qt_serial import SerialRecvWorker, SerialSendWorker
from sample_data import MethodEnum, SampleDB, WaveEnum

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
SampleConf = namedtuple("SampleConf", "method wave point_num")
HEATER_PID_PS = [1, 100, 0.01, 1, 1, 1]

logger = loguru.logger


CONFIG_PATH = "./conf/config.json"
CONFIG = dict()
try:
    with open(CONFIG_PATH, "r") as f:
        CONFIG = json.load(f)
except Exception:
    logger.error(f"load conf failed \n{stackprinter.format()}")
    CONFIG = dict()
    CONFIG["log"] = dict(rotation="1 MB", retention=50)
    try:
        with open(CONFIG_PATH, "w") as f:
            json.dump(CONFIG, f)
    except Exception:
        logger.error(f"dump conf failed \n{stackprinter.format()}")

rotation = CONFIG.get("log", {}).get("rotation", "4 MB")
retention = CONFIG.get("log", {}).get("retention", 25)
logger.add("./log/dc201.log", rotation=rotation, retention=retention, enqueue=True)


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
        self.setWindowTitle("DC201 工装测试")
        self.serial = serial.Serial(port=None, baudrate=115200, timeout=0.01)
        self.task_queue = queue.Queue()
        self.henji_queue = queue.Queue()
        self.last_firm_path = None
        self.last_bl_path = None
        self.threadpool = QThreadPool()
        self.id_card_data = bytearray(4096)
        self.storge_time_start = 0
        self.out_flash_start = 0
        self.out_flash_length = 0
        self.out_flash_data = bytearray()
        self.temp_btm_record = []
        self.temp_top_record = []
        self.temp_start_time = None
        self.temp_time_record = []
        self.temp_raw_records = [[] for _ in range(9)]
        self.temp_raw_start_time = None
        self.temp_raw_time_record = []
        self.temperature_raw_plots = []
        self.dd = DC201_PACK()
        self.pack_index = 1
        self.device_id = "no name"
        self.version = "no version"
        self.device_datetime = datetime.now()
        self.serial_recv_worker = None
        self.serial_send_worker = None
        self.sample_db = SampleDB("sqlite:///data/db.sqlite3")
        self.sample_record_current_label = None
        self.initUI()
        self.center()

    def _serialSendPack(self, *args, **kwargs):
        try:
            pack = self.dd.buildPack(0x13, self.getPackIndex(), *args, **kwargs)
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
        self._serialSendPack(0x07)
        self._getHeater()
        self._getDebuFlag()
        self._getDeviceID()

    def _setColor(sself, wg, nbg=None, nfg=None):
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
        right_ly.addWidget(self.serial_gb)

        layout.addLayout(right_ly, stretch=0)
        layout.addWidget(self.matplot_wg, stretch=1)
        image_path = "./icos/tt.ico"
        self.setWindowIcon(QIcon(image_path))
        self.setCentralWidget(widget)
        self.resize(850, 492)

    def center(self):
        logger.debug(f"invoke center in ModernWidget")
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
        self.threadpool.waitForDone(500)
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
        self.temperature_plot_win = GraphicsLayoutWidget()
        temperature_plot_ly.addWidget(self.temperature_plot_win)
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
        temp_ly.addWidget(self.temperature_plot_clear_bt)
        temp_ly.addStretch(1)
        temp_ly.addLayout(temperature_heater_ctl_ly)
        temperature_plot_ly.addLayout(temp_ly)
        self.temperature_heater_btm_cb.clicked.connect(self.onTemperatureHeaterChanged)
        self.temperature_heater_top_cb.clicked.connect(self.onTemperatureHeaterChanged)

        self.temperature_plot_lb = LabelItem(justify="right")
        self.temperature_plot_win.addItem(self.temperature_plot_lb, 0, 0)
        self.temperature_plot_wg = self.temperature_plot_win.addPlot(row=0, col=0)
        self.temperature_plot_wg.addLegend()
        self.temperature_plot_wg.showGrid(x=True, y=True, alpha=1.0)
        self.temperature_plot_proxy = SignalProxy(self.temperature_plot_wg.scene().sigMouseMoved, rateLimit=60, slot=self.onTemperaturePlotMouseMove)
        self.temperature_btm_plot = self.temperature_plot_wg.plot(self.temp_time_record, self.temp_btm_record, name="下加热体", pen=mkPen(color="r"))
        self.temperature_top_plot = self.temperature_plot_wg.plot(self.temp_time_record, self.temp_top_record, name="上加热体", pen=mkPen(color="b"))
        self.temperature_plot_clear_bt.clicked.connect(self.onTemperautreDataClear)

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
        if len(raw_bytes) == 26:
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

    def onTemperautreDataClear(self, event):
        self.temp_btm_record.clear()
        self.temp_top_record.clear()
        self.temp_start_time = None
        self.temp_time_record.clear()
        self.temperature_plot_wg.clear()
        self.temperature_btm_plot = self.temperature_plot_wg.plot(self.temp_time_record, self.temp_btm_record, name="下加热体", pen=mkPen(color="r"))
        self.temperature_top_plot = self.temperature_plot_wg.plot(self.temp_time_record, self.temp_top_record, name="上加热体", pen=mkPen(color="b"))

    def onTemperaturePlotMouseMove(self, event):
        mouse_point = self.temperature_plot_wg.vb.mapSceneToView(event[0])
        self.temperature_plot_lb.setText(
            "<span style='font-size: 14pt; color: white'> x = %0.2f S, <span style='color: white'> y = %0.2f ℃</span>" % (mouse_point.x(), mouse_point.y())
        )

    def onTemperautreRawLabelClick(self, event):
        self.temperature_raw_plot_dg.show()

    def onTemperautreRawDataClear(self, event):
        for i in self.temp_raw_records:
            i.clear()
        self.temp_raw_start_time = None
        self.temp_raw_time_record.clear()
        self.temperature_raw_plot_wg.clear()
        self.temperature_raw_plots.clear()
        for i in range(9):
            symbol = symbol, symbolSize, color = TEMP_RAW_SYMBOL_CONFIG[i]
            plot = self.temperature_raw_plot_wg.plot(
                self.temp_raw_time_record,
                self.temp_raw_records[i],
                name=f"#{i + 1}",
                pen=mkPen(color=TEMP_RAW_COLORS[i]),
                symbol=symbol,
                symbolSize=symbolSize,
                symbolBrush=(color),
            )
            self.temperature_raw_plots.append(plot)

    def onTemperatureRawPlotMouseMove(self, event):
        mouse_point = self.temperature_raw_plot_wg.vb.mapSceneToView(event[0])
        self.temperature_raw_plot_lb.setText(
            "<span style='font-size: 14pt; color: white'> x = %0.2f S, <span style='color: white'> y = %0.3f ℃</span>" % (mouse_point.x(), mouse_point.y())
        )

    def updateTemperautre(self, info):
        if self.temp_start_time is None:
            self.temp_start_time = time.time()
            self.temp_time_record = [0]
        else:
            self.temp_time_record.append(time.time() - self.temp_start_time)
        temp_btm = int.from_bytes(info.content[6:8], byteorder="little") / 100
        temp_top = int.from_bytes(info.content[8:10], byteorder="little") / 100
        self.temp_btm_record.append(temp_btm)
        self.temp_top_record.append(temp_top)
        self.temperature_btm_plot.setData(self.temp_time_record, self.temp_btm_record)
        self.temperature_top_plot.setData(self.temp_time_record, self.temp_top_record)
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
        self.temperature_plot_dg.setWindowTitle(f"温度记录 | 下 {self.temperautre_lbs[0].text()} | 上 {self.temperautre_lbs[1].text()}")

    def updateTemperautreRaw(self, info):
        if len(info.content) != 43:
            logger.error(f"error temp raw info | {info}")
            return
        if self.temp_raw_start_time is None:
            self.temp_raw_start_time = time.time()
            self.temp_raw_time_record = [0]
        else:
            self.temp_raw_time_record.append(time.time() - self.temp_raw_start_time)
        for idx in range(9):
            temp_value = struct.unpack("f", info.content[6 + 4 * idx : 10 + 4 * idx])[0]
            self.temperautre_raw_lbs[idx].setText(f"#{idx + 1} {temp_value:.3f}℃")
            self.temp_raw_records[idx].append(temp_value)
            self.temperature_raw_plots[idx].setData(self.temp_raw_time_record, self.temp_raw_records[idx])

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
        self.device_id_lb.setText(f"ID: {self.device_id}")
        date_str = f'{raw_bytes[26: 37].decode("ascii", errors="ignore")} - {raw_bytes[18: 26].decode("ascii", errors="ignore")}'
        self.device_datetime = datetime.strptime(date_str, "%b %d %Y - %H:%M:%S")
        logger.debug(f"get datetime obj | {self.device_datetime}")
        self.version_lb.setToolTip(f"V{self.version}.{datetime.strftime(self.device_datetime, '%Y%m%d.%H%M%S')}")

    def createBarcode(self):
        self.barcode_gb = QGroupBox("测试通道")
        barcode_ly = QGridLayout(self.barcode_gb)
        barcode_ly.setContentsMargins(3, 3, 3, 3)
        self.barcode_lbs = [QLabel("*" * 10) for i in range(7)]
        self.motor_scan_bts = [QPushButton(BARCODE_NAMES[i]) for i in range(7)]
        self.matplot_conf_houhou_cs = [QComboBox() for i in range(6)]
        self.matplot_conf_wavelength_cs = [QComboBox() for i in range(6)]
        self.matplot_conf_point_sps = [QSpinBox() for i in range(6)]
        self.barcode_scan_bt = QPushButton("扫码")
        self.barcode_scan_bt.setMaximumWidth(60)
        self.matplot_start_bt = QPushButton("测试")
        self.matplot_start_bt.setMaximumWidth(60)
        self.matplot_cancel_bt = QPushButton("取消")
        self.matplot_cancel_bt.setMaximumWidth(50)
        self.matplot_period_tv_lb = QLabel("NL")
        self.matplot_period_tv_cb = QCheckBox()
        self.matplot_period_tv_cb.setTristate(True)
        self.matplot_period_tv_cb.stateChanged.connect(lambda x: self.matplot_period_tv_lb.setText(("NL", "OD", "PD")[x]))
        for i in range(7):
            self.motor_scan_bts[i].setMaximumWidth(45)
            barcode_ly.addWidget(self.motor_scan_bts[i], i, 0)
            barcode_ly.addWidget(self.barcode_lbs[i], i, 1)
            if i < 6:
                self.matplot_conf_houhou_cs[i].addItems(METHOD_NAMES)
                self.matplot_conf_houhou_cs[i].setMaximumWidth(75)
                self.matplot_conf_houhou_cs[i].setCurrentIndex(1)
                self.matplot_conf_wavelength_cs[i].addItems(WAVE_NAMES)
                self.matplot_conf_wavelength_cs[i].setMaximumWidth(60)
                self.matplot_conf_wavelength_cs[i].setCurrentIndex(0)
                self.matplot_conf_point_sps[i].setRange(0, 120)
                self.matplot_conf_point_sps[i].setMaximumWidth(60)
                self.matplot_conf_point_sps[i].setValue(6)
                barcode_ly.addWidget(self.matplot_conf_houhou_cs[i], i, 2)
                barcode_ly.addWidget(self.matplot_conf_wavelength_cs[i], i, 3)
                barcode_ly.addWidget(self.matplot_conf_point_sps[i], i, 4)
            else:
                temp_ly = QHBoxLayout()
                temp_ly.setSpacing(0)
                temp_ly.setContentsMargins(3, 3, 3, 3)
                temp_ly.addWidget(self.barcode_scan_bt)
                temp_ly.addWidget(self.matplot_start_bt)
                temp_ly.addWidget(self.matplot_cancel_bt)
                temp_ly.addWidget(self.matplot_period_tv_lb)
                temp_ly.addWidget(self.matplot_period_tv_cb)
                barcode_ly.addLayout(temp_ly, i, 2, 1, 3)
        self.sample_record_idx_sp = QSpinBox()
        self.sample_record_idx_sp.setRange(0, 99999999)
        self.sample_record_idx_sp.setMaximumWidth(75)
        self.sample_record_label = QLabel("***********")
        matplot_record_ly = QHBoxLayout()
        matplot_record_ly.setContentsMargins(3, 1, 3, 1)
        matplot_record_ly.setSpacing(1)
        label = QLabel("序号")
        label.setMaximumWidth(30)
        matplot_record_ly.addWidget(label, 0)
        matplot_record_ly.addWidget(self.sample_record_idx_sp, 0)
        matplot_record_ly.addWidget(QVLine(), 0)
        matplot_record_ly.addWidget(self.sample_record_label)
        barcode_ly.addLayout(matplot_record_ly, 7, 0, 1, 5)
        self.barcode_scan_bt.clicked.connect(self.onBarcodeScan)
        self.matplot_start_bt.clicked.connect(self.onMatplotStart)
        self.matplot_cancel_bt.clicked.connect(self.onMatplotCancel)
        for i in range(7):
            self.motor_scan_bts[i].clicked.connect(partial(self.onMotorScan, idx=i))
        self.sample_record_idx_sp.valueChanged.connect(self.onSampleRecordReadBySp)
        self.sample_record_label.mousePressEvent = self.onSampleLabelClick

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

        self.motor_heater_up_bt = QPushButton("上加热体抬升")
        self.motor_heater_up_bt.setMaximumWidth(120)
        self.motor_heater_down_bt = QPushButton("上加热体下降")
        self.motor_heater_down_bt.setMaximumWidth(120)

        self.motor_white_pd_bt = QPushButton("白板PD位置")
        self.motor_white_pd_bt.setMaximumWidth(120)
        self.motor_white_od_bt = QPushButton("白物质位置")
        self.motor_white_od_bt.setMaximumWidth(120)

        self.motor_tray_in_bt = QPushButton("托盘进仓")
        self.motor_tray_in_bt.setMaximumWidth(120)
        self.motor_tray_scan_bt = QPushButton("托盘扫码")
        self.motor_tray_scan_bt.setMaximumWidth(120)
        self.motor_tray_out_bt = QPushButton("托盘出仓")
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
        self._serialSendPack(0xD0, (1, 1))

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

    def sample_record_parse_raw_data(self, raw_data):
        data = []
        data_len = len(raw_data)
        if data_len % 2 != 0:
            logger.error(f"sample data length error | {data_len}")
            return data
        for i in range(0, data_len, 2):
            data.append(struct.unpack("H", raw_data[i : i + 2])[0])
        return data

    def sample_record_plot_by_index(self, idx):
        label = self.sample_db.get_label_by_index(idx)
        self.sample_record_current_label = label
        if label is None:
            logger.error("hit the label limit")
            cnt = self.sample_db.get_label_cnt()
            self.sample_record_idx_sp.setRange(0, cnt - 1)
            return
        # logger.debug(f"get Label {label}")
        # logger.debug(f"get sample datas | {label.sample_datas}")
        self.sample_record_label.setText(f"{label.name}")
        self.sample_record_label.setToolTip(f"{label.datetime}")
        for plot in self.matplot_plots:
            plot.clear()
        for i in range(6):
            self.matplot_conf_houhou_cs[i].setCurrentIndex(0)
            self.matplot_conf_wavelength_cs[i].setCurrentIndex(0)
            self.matplot_conf_point_sps[i].setValue(0)
        for sd in label.sample_datas:
            channel_idx = sd.channel - 1
            data = self.sample_record_parse_raw_data(sd.raw_data)
            self.temp_saple_data[channel_idx] = data
            self.matplot_plots[channel_idx].setData(data)
            self.matplot_conf_houhou_cs[channel_idx].setCurrentIndex(sd.method.value)
            self.matplot_conf_wavelength_cs[channel_idx].setCurrentIndex(sd.wave.value - 1)
            self.matplot_conf_point_sps[channel_idx].setValue(sd.total)

    def onSampleLabelClick(self, event):
        def data_format_list(data):
            result = ", ".join(f"{i:#5d}" for i in data)
            if len(result) > 300:
                result = result[:300] + "..."
            return result

        label = self.sample_record_current_label
        if label is None:
            return
        sds = sorted(label.sample_datas, key=lambda x: x.channel)
        msg = ModernMessageBox(self)
        msg.setTextInteractionFlags(Qt.TextSelectableByMouse)
        msg.setIcon(QMessageBox.Information)
        msg.setWindowTitle(f"采样数据 | {label.datetime} | {label.name}")
        data_infos = []
        for sd in sds:
            real_data = self.sample_record_parse_raw_data(sd.raw_data)
            if len(real_data) == 0:
                continue
            xs = np.array([10 * i for i in range(len(real_data))], dtype=np.int32)
            ys = np.array(real_data, dtype=np.int32)
            yv = np.std(ys)
            ym = np.mean(ys)
            m, b = best_fit_slope_and_intercept(xs, ys)
            data_infos.append(f"通道 {sd.channel} | {data_format_list(real_data)} | (m = {m:.4f}, b = {b:.4f}) | (yv = {yv:.4f}, ym = {ym:.4f})")
        # https://stackoverflow.com/a/10977872
        font = QFont("Consolas")
        font.setFixedPitch(True)
        msg.setFont(font)
        msg.setText("\n".join(data_infos))
        detail_head_text = f"时间 | {label.datetime}\n标签 | {label.name}\n版本 | {label.version}\n容量 | {len(sds)}"
        detail_text = f"\n{'=' * 24}\n".join(
            (
                f"通道 {sd.channel} | {sd.datetime} | {METHOD_NAMES[sd.method.value]} | {WAVE_NAMES[sd.wave.value - 1]} | 点数 {sd.total}\n"
                f"{self.sample_record_parse_raw_data(sd.raw_data)}\n{bytesPuttyPrint(sd.raw_data)}"
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

    def createSerial(self):
        self.serial_gb = QGroupBox("串口")
        serial_ly = QHBoxLayout(self.serial_gb)
        serial_ly.setContentsMargins(3, 3, 3, 3)
        serial_ly.setSpacing(0)
        self.serial_switch_bt = QPushButton("打开串口")
        self.serial_switch_bt.setMaximumWidth(75)
        self.serial_refresh_bt = QPushButton("刷新")
        self.serial_refresh_bt.setMaximumWidth(75)
        self.serial_post_co = QComboBox()
        self.serial_post_co.setMaximumWidth(75)
        self.serialRefreshPort()
        serial_ly.addWidget(self.serial_post_co)
        serial_ly.addWidget(self.serial_refresh_bt)
        serial_ly.addWidget(self.serial_switch_bt)
        self.serial_refresh_bt.clicked.connect(self.onSerialRefresh)
        self.serial_switch_bt.setCheckable(True)
        self.serial_switch_bt.clicked.connect(self.onSerialSwitch)

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
            self.serial_recv_worker = SerialRecvWorker(self.serial, self.henji_queue, logger)
            self.serial_recv_worker.signals.finished.connect(self.onSerialWorkerFinish)
            self.serial_recv_worker.signals.error.connect(self.onSerialWorkerError)
            self.serial_recv_worker.signals.serial_statistic.connect(self.onSerialStatistic)
            self.serial_recv_worker.signals.result.connect(self.onSerialRecvWorkerResult)
            self.threadpool.start(self.serial_recv_worker)

            self.serial_send_worker = SerialSendWorker(self.serial, self.task_queue, self.henji_queue, self.serial_recv_worker, logger)
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
        logger.info(f"emit from serial worker result signal | {info.text}")
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
                self.firm_size = file_size + (256 - file_size % 256)
                for pack in write_firmware_pack_FC(self.dd, file_path, chunk_size=1024):
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

        self.plot_win = GraphicsLayoutWidget()
        self.plot_win.setBackground((0, 0, 0, 255))
        self.plot_lb = LabelItem(justify="right")
        self.plot_win.addItem(self.plot_lb, 0, 0)
        self.plot_wg = self.plot_win.addPlot(row=0, col=0)
        self.plot_wg.addLegend()
        self.plot_wg.showGrid(x=True, y=True, alpha=1.0)
        self.plot_proxy = SignalProxy(self.plot_wg.scene().sigMouseMoved, rateLimit=60, slot=self.onPlotMouseMove)
        matplot_ly.addWidget(self.plot_win)
        self.temp_saple_data = [None] * 6
        self.matplot_plots = []
        for k in range(6):
            color = LINE_COLORS[k % len(LINE_COLORS)]
            symbol = LINE_SYMBOLS[k % len(LINE_SYMBOLS)]
            self.matplot_plots.append(self.plot_wg.plot([], name=f"B{k+1}", pen=mkPen(color=color), symbol=symbol, symbolSize=5, symbolBrush=(color)))

    def onPlotMouseMove(self, event):
        mouse_point = self.plot_wg.vb.mapSceneToView(event[0])
        self.plot_lb.setText(
            "<span style='font-size: 14pt; color: white'> x = %0.2f, <span style='color: white'> y = %0.2f</span>" % (mouse_point.x(), mouse_point.y())
        )

    def onMatplotStart(self, event):
        self.initBarcodeScan()
        name_text, press_result = QInputDialog.getText(self, "测试标签", "输入标签名称", QLineEdit.Normal, datetime.now().strftime("%Y%m%d%H%M%S"))
        if press_result and len(name_text) > 0:
            self.sample_record_lable_name = name_text
        else:
            self.sample_record_lable_name = datetime.now().strftime("%Y%m%d%H%M%S")
        logger.debug(f"set label name | {name_text} | {press_result} | {self.sample_record_lable_name}")
        conf = []
        self.sample_confs = []
        self.sample_datas = []
        for i in range(6):
            conf.append(self.matplot_conf_houhou_cs[i].currentIndex())
            conf.append(self.matplot_conf_wavelength_cs[i].currentIndex() + 1)
            conf.append(self.matplot_conf_point_sps[i].value())
            self.sample_confs.append(SampleConf(conf[-3], conf[-2], conf[-1]))
        logger.debug(f"get matplot cnf | {conf}")
        self.sample_label = self.sample_db.build_label(
            name=self.sample_record_lable_name, version=f"{self.version}.{datetime.strftime(self.device_datetime, '%Y%m%d.%H%M%S')}", device_id=self.device_id
        )
        self._serialSendPack(0x01)
        if self.matplot_period_tv_cb.isChecked():
            conf.append(self.matplot_period_tv_cb.checkState())
            self._serialSendPack(0x08, conf)
        else:
            self._serialSendPack(0x03, conf)
        for plot in self.matplot_plots:
            plot.clear()

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
            self.sample_confs.append(SampleConf(conf[-3], conf[-2], conf[-1]))
        logger.debug(f"get matplot cnf | {conf}")
        self.sample_label = self.sample_db.build_label(
            name=self.sample_record_lable_name, version=f"{self.version}.{datetime.strftime(self.device_datetime, '%Y%m%d.%H%M%S')}", device_id=self.device_id
        )
        for plot in self.matplot_plots:
            plot.clear()

    def onMatplotCancel(self, event):
        self._serialSendPack(0x02)

    def onBootload(self, event):
        # self._serialSendPack(0x0F)
        # self._serialSendPack(0xDC)
        self.upgbl_dg = QDialog(self)
        self.upgbl_dg.setWindowTitle("Bootloader升级")
        self.upgbl_dg_ly = QVBoxLayout(self.upgbl_dg)

        upgbl_temp_ly = QHBoxLayout()
        upgbl_temp_ly.addWidget(QLabel("Bootloader路径"))
        self.upgbl_dg_lb = QLineEdit()
        if self.last_bl_path is not None and os.path.isfile(self.last_bl_path):
            self.upgbl_dg_lb.setText(self.last_bl_path)
            self.upgbl_dg.setWindowTitle(f"Bootloader升级 | {self._getFileHash_SHA256(self.last_bl_path)}")
        self.upgbl_dg_fb_bt = QPushButton("...")
        upgbl_temp_ly.addWidget(self.upgbl_dg_lb)
        upgbl_temp_ly.addWidget(self.upgbl_dg_fb_bt)
        self.upgbl_dg_ly.addLayout(upgbl_temp_ly)
        self.upgbl_dg_fb_bt.clicked.connect(self.onUpgBLDialogFileSelect)

        upgbl_temp_ly = QHBoxLayout()
        self.upgbl_pr = QProgressBar(self)
        upgbl_temp_ly.addWidget(QLabel("进度"))
        upgbl_temp_ly.addWidget(self.upgbl_pr)
        self.upgbl_dg_bt = QPushButton("开始")
        upgbl_temp_ly.addWidget(self.upgbl_dg_bt)
        self.upgbl_dg_ly.addLayout(upgbl_temp_ly)

        self.upgbl_pr.setMaximum(100)
        self.upgbl_dg_bt.setCheckable(True)
        self.upgbl_dg_bt.clicked.connect(self.onUpgBLDialog)

        self.upgbl_dg.resize(600, 75)
        self.upgbl_dg = ModernDialog(self.upgbl_dg, self)
        self.upgbl_dg.exec_()

    def onUpgBLDialog(self, event):
        if event is True:
            file_path = self.upgbl_dg_lb.text()
            if os.path.isfile(file_path):
                self.upgbl_dg_bt.setEnabled(False)
                self.bl_wrote_size = 0
                self.bl_start_time = time.time()
                for pack in write_firmware_pack_BL(self.dd, file_path, chunk_size=224):
                    self.task_queue.put(pack)
            else:
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

    def onReboot(self, event):
        self._serialSendPack(0xDC)
        QTimer.singleShot(3000, self._getStatus)

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
            self.selftest_temp_top_gb.layout().addWidget(lb)
        self.selftest_temp_btm_gb = QGroupBox("下加热体温度")
        self.selftest_temp_btm_gb.setLayout(QHBoxLayout())
        self.selftest_temp_btm_gb.layout().setContentsMargins(3, 3, 3, 3)
        for lb in self.selftest_temp_lbs[6:8]:
            lb.setAlignment(Qt.AlignCenter)
            self.selftest_temp_btm_gb.layout().addWidget(lb)
        self.selftest_temp_env_gb = QGroupBox("环境")
        self.selftest_temp_env_gb.setLayout(QHBoxLayout())
        self.selftest_temp_env_gb.layout().setContentsMargins(3, 3, 3, 3)
        for lb in self.selftest_temp_lbs[8:9]:
            lb.setAlignment(Qt.AlignCenter)
            self.selftest_temp_env_gb.layout().addWidget(lb)
        selftest_temp_ly.addWidget(self.selftest_temp_top_gb, stretch=6)
        selftest_temp_ly.addWidget(self.selftest_temp_btm_gb, stretch=2)
        selftest_temp_ly.addWidget(self.selftest_temp_env_gb, stretch=1)
        self.selftest_temp_top_gb.mousePressEvent = lambda x: [self._clear_widget_style_sheet(self.selftest_temp_top_gb), self._serialSendPack(0xDA, (0x01,))]
        self.selftest_temp_btm_gb.mousePressEvent = lambda x: [self._clear_widget_style_sheet(self.selftest_temp_btm_gb), self._serialSendPack(0xDA, (0x02,))]
        self.selftest_temp_env_gb.mousePressEvent = lambda x: [self._clear_widget_style_sheet(self.selftest_temp_env_gb), self._serialSendPack(0xDA, (0x03,))]

        selftest_motor_ly = QHBoxLayout()
        self.selftest_motor_lbs = [QLabel(t) for t in ("上", "下", "进", "出", "PD", "白")]
        self.selftest_motor_heater_gb = QGroupBox("上加热体")
        self.selftest_motor_heater_gb.setLayout(QHBoxLayout())
        self.selftest_motor_heater_gb.layout().setContentsMargins(3, 3, 3, 3)
        for lb in self.selftest_motor_lbs[0:2]:
            lb.setAlignment(Qt.AlignCenter)
            self.selftest_motor_heater_gb.layout().addWidget(lb)
        self.selftest_motor_tray_gb = QGroupBox("托盘")
        self.selftest_motor_tray_gb.setLayout(QHBoxLayout())
        self.selftest_motor_tray_gb.layout().setContentsMargins(3, 3, 3, 3)
        for lb in self.selftest_motor_lbs[2:4]:
            lb.setAlignment(Qt.AlignCenter)
            self.selftest_motor_tray_gb.layout().addWidget(lb)
        self.selftest_motor_white_gb = QGroupBox("白板")
        self.selftest_motor_white_gb.setLayout(QHBoxLayout())
        self.selftest_motor_white_gb.layout().setContentsMargins(3, 3, 3, 3)
        for lb in self.selftest_motor_lbs[4:6]:
            lb.setAlignment(Qt.AlignCenter)
            self.selftest_motor_white_gb.layout().addWidget(lb)
        self.selftest_motor_scan_gb = QGroupBox("扫码")
        self.selftest_motor_scan_gb.setLayout(QHBoxLayout())
        self.selftest_motor_scan_gb.layout().setContentsMargins(3, 3, 3, 3)
        self.selftest_motor_scan_m = QLabel("电机")
        self.selftest_motor_scan_l = QLabel("*" * 10)
        self.selftest_motor_scan_m.setAlignment(Qt.AlignCenter)
        self.selftest_motor_scan_l.setAlignment(Qt.AlignCenter)
        self.selftest_motor_scan_gb.layout().addWidget(self.selftest_motor_scan_m, stretch=1)
        self.selftest_motor_scan_gb.layout().addWidget(QVLine())
        self.selftest_motor_scan_gb.layout().addWidget(self.selftest_motor_scan_l, stretch=2)
        selftest_motor_ly.addWidget(self.selftest_motor_heater_gb, stretch=1)
        selftest_motor_ly.addWidget(self.selftest_motor_tray_gb, stretch=1)
        selftest_motor_ly.addWidget(self.selftest_motor_white_gb, stretch=1)
        selftest_motor_ly.addWidget(self.selftest_motor_scan_gb, stretch=3)
        self.selftest_motor_heater_gb.mousePressEvent = lambda x: [
            self._clear_widget_style_sheet(self.selftest_motor_heater_gb),
            self._serialSendPack(0xDA, (0x07,)),
        ]
        self.selftest_motor_tray_gb.mousePressEvent = lambda x: [
            self._clear_widget_style_sheet(self.selftest_motor_tray_gb),
            self._serialSendPack(0xDA, (0x08,)),
        ]
        self.selftest_motor_white_gb.mousePressEvent = lambda x: [
            self._clear_widget_style_sheet(self.selftest_motor_white_gb),
            self._serialSendPack(0xDA, (0x06,)),
        ]
        self.selftest_motor_scan_m.mousePressEvent = lambda x: [self._setColor(self.selftest_motor_scan_m), self._serialSendPack(0xDA, (0x09,))]
        self.selftest_motor_scan_l.mousePressEvent = lambda x: [
            self._setColor(self.selftest_motor_scan_l),
            self.selftest_motor_scan_l.setText("*" * 10),
            self._serialSendPack(0xDA, (0x0A,)),
        ]

        selftest_storge_ly = QHBoxLayout()
        self.selftest_storge_lb_f = QLabel("片外 Flash")
        self.selftest_storge_lb_c = QLabel("ID Code 卡")
        selftest_storge_gb = QGroupBox("存储")
        selftest_storge_gb.setLayout(QHBoxLayout())
        selftest_storge_gb.layout().setContentsMargins(3, 3, 3, 3)
        self.selftest_storge_lb_f.setAlignment(Qt.AlignCenter)
        self.selftest_storge_lb_c.setAlignment(Qt.AlignCenter)
        selftest_storge_gb.layout().addWidget(self.selftest_storge_lb_f)
        selftest_storge_gb.layout().addWidget(self.selftest_storge_lb_c)
        self.selftest_storge_lb_f.mousePressEvent = lambda x: [self._setColor(self.selftest_storge_lb_f), self._serialSendPack(0xDA, (0x04,))]
        self.selftest_storge_lb_c.mousePressEvent = lambda x: [self._setColor(self.selftest_storge_lb_c), self._serialSendPack(0xDA, (0x05,))]
        selftest_storge_ly.addWidget(selftest_storge_gb)

        selftest_dg_ly.addLayout(selftest_temp_ly)
        selftest_dg_ly.addLayout(selftest_motor_ly)
        selftest_dg_ly.addLayout(selftest_storge_ly)
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
            else:
                self._setColor(self.selftest_motor_scan_m, nbg="green")
        elif item == 10:
            if result > 0:
                self._setColor(self.selftest_motor_scan_l, nbg="red")
            else:
                self._setColor(self.selftest_motor_scan_l, nbg="green")
                text = raw_bytes[9 : 9 + raw_bytes[8]].decode(errors="replace")
                self.selftest_motor_scan_l.setText(text)

    def onSelfCheck(self, event):
        button = event.button()
        logger.debug(f"invoke onSelfCheck | event {event.type()} | {button}")
        if button == Qt.LeftButton:
            for lb in self.selftest_temp_lbs:
                lb.setText("**.**")
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
        self.upgrade_dg_fb_bt = QPushButton("...")
        upgrade_temp_ly.addWidget(self.upgrade_dg_lb)
        upgrade_temp_ly.addWidget(self.upgrade_dg_fb_bt)
        self.upgrade_dg_ly.addLayout(upgrade_temp_ly)
        self.upgrade_dg_fb_bt.clicked.connect(self.onUpgradeDialogFileSelect)

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
        if event is True:
            file_path = self.upgrade_dg_lb.text()
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

    def onSampleOver(self):
        records = []
        for k in sorted(self.matplot_data.keys()):
            v = self.matplot_data.get(k, [0])
            records.append(f"{k} | {v}")
        pyperclip.copy("\n".join(records))
        cnt = self.sample_db.get_label_cnt()
        if cnt > 1:
            self.sample_record_idx_sp.setRange(0, cnt - 1)
            self.sample_record_idx_sp.setValue(cnt - 1)
        else:
            self.sample_record_plot_by_index(0)

    def updateMatplotData(self, info):
        length = info.content[6]
        channel = info.content[7]
        if len(info.content) != 2 * length + 9:
            logger.error(f"error data length | {len(info.content)} --> {length} | {info.text}")
            return
        data = tuple((struct.unpack("H", info.content[8 + i * 2 : 10 + i * 2])[0] for i in range(length)))
        self.matplot_plots[channel - 1].setData(data)
        self.matplot_data[channel] = data
        method = self.sample_confs[channel - 1].method
        wave = self.sample_confs[channel - 1].wave
        raw_data = info.content[8:-1]
        sample_data = self.sample_db.build_sample_data(
            datetime.now(), channel=channel, method=MethodEnum(method), wave=WaveEnum(wave), total=length, raw_data=raw_data
        )
        self.sample_datas.append(sample_data)
        logger.debug(f"get data in channel | {channel} | {data}")
        self.sample_db.bind_label_sample_data(self.sample_label, sample_data)
        if len(self.sample_datas) == 6:
            cnt = self.sample_db.get_label_cnt()
            if cnt > 1:
                self.sample_record_idx_sp.setRange(0, cnt - 1)
                self.sample_record_idx_sp.setValue(cnt - 1)
            else:
                self.sample_record_plot_by_index(0)
            logger.debug(f"put to new label | update label cnt {cnt}")
            idx = len(self.sample_datas) // 6 + 1
            conf = []
            self.sample_confs = []
            self.sample_datas = []
            for i in range(6):
                conf.append(1)
                conf.append(idx)
                conf.append(12)
                self.sample_confs.append(SampleConf(conf[-3], conf[-2], conf[-1]))
            self.sample_label = self.sample_db.build_label(
                name=f"{self.sample_record_lable_name}-{idx}",
                version=f"{self.version}.{datetime.strftime(self.device_datetime, '%Y%m%d.%H%M%S')}",
                device_id=self.device_id,
            )

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

        temperautre_raw_wg = QWidget()
        temperautre_raw_ly = QHBoxLayout(temperautre_raw_wg)
        temperautre_raw_ly.setContentsMargins(5, 0, 5, 0)
        temperautre_raw_ly.setSpacing(5)
        self.temperautre_raw_lbs = [QLabel("-" * 4) for _ in range(9)]
        for i in range(9):
            temperautre_raw_ly.addSpacing(1)
            temperautre_raw_ly.addWidget(self.temperautre_raw_lbs[i])
            temperautre_raw_ly.addSpacing(1)
        temperautre_raw_wg.mousePressEvent = self.onTemperautreRawLabelClick

        self.temperature_raw_plot_dg = QDialog(self)
        self.temperature_raw_plot_dg.setWindowTitle("温度ADC 采样原始数据")
        temperature_raw_plot_ly = QVBoxLayout(self.temperature_raw_plot_dg)
        self.temperature_raw_plot_win = GraphicsLayoutWidget()
        self.temperature_raw_plot_clear_bt = QPushButton("清零")
        temperature_raw_plot_ly.addWidget(self.temperature_raw_plot_win)
        temperature_raw_plot_ly.addWidget(self.temperature_raw_plot_clear_bt)
        self.temperature_raw_plot_lb = LabelItem(justify="right")
        self.temperature_raw_plot_win.addItem(self.temperature_raw_plot_lb, 0, 0)
        self.temperature_raw_plot_wg = self.temperature_raw_plot_win.addPlot(row=0, col=0)
        self.temperature_raw_plot_wg.addLegend()
        self.temperature_raw_plot_wg.showGrid(x=True, y=True, alpha=1.0)
        self.temperature_raw_plot_proxy = SignalProxy(self.temperature_raw_plot_wg.scene().sigMouseMoved, rateLimit=60, slot=self.onTemperatureRawPlotMouseMove)
        self.temperature_raw_plots.clear()
        for i in range(9):
            symbol = symbol, symbolSize, color = TEMP_RAW_SYMBOL_CONFIG[i]
            plot = self.temperature_raw_plot_wg.plot(
                self.temp_raw_time_record,
                self.temp_raw_records[i],
                name=f"#{i + 1}",
                pen=mkPen(color=TEMP_RAW_COLORS[i]),
                symbol=symbol,
                symbolSize=symbolSize,
                symbolBrush=(color),
            )
            self.temperature_raw_plots.append(plot)
        self.temperature_raw_plot_clear_bt.clicked.connect(self.onTemperautreRawDataClear)
        self.temperature_raw_plot_dg.resize(600, 600)
        self.temperature_raw_plot_dg = ModernDialog(self.temperature_raw_plot_dg, self)

        self.out_flash_data_dg = QDialog(self)
        self.out_flash_data_dg.setWindowTitle("外部Flash")
        self.out_flash_data_dg.resize(730, 370)
        out_flash_data_ly = QVBoxLayout(self.out_flash_data_dg)
        self.out_flash_data_te = QTextEdit()
        out_flash_temp_ly = QHBoxLayout()
        self.out_flash_data_addr = QSpinBox()
        self.out_flash_data_addr.setRange(0, 8 * 2 ** 20)
        self.out_flash_data_addr.setMaximumWidth(90)
        self.out_flash_data_addr.setValue(4096)
        self.out_flash_data_num = QSpinBox()
        self.out_flash_data_num.setRange(0, 8 * 2 ** 20)
        self.out_flash_data_num.setMaximumWidth(90)
        self.out_flash_data_num.setValue(668)
        self.out_flash_data_read_bt = QPushButton("读取")
        out_flash_temp_ly.addWidget(QLabel("地址"))
        out_flash_temp_ly.addWidget(self.out_flash_data_addr)
        out_flash_temp_ly.addWidget(QLabel("数量"))
        out_flash_temp_ly.addWidget(self.out_flash_data_num)
        out_flash_temp_ly.addWidget(self.out_flash_data_read_bt)

        out_flash_param_gb = QGroupBox("系统参数")
        out_flash_param_ly = QVBoxLayout(out_flash_param_gb)
        out_flash_param_ly.setSpacing(5)
        self.out_flash_param_temp_sps = [QDoubleSpinBox(self) for _ in range(11)]
        self.out_flash_param_read_bt = QPushButton("读取")
        self.out_flash_param_write_bt = QPushButton("写入")
        self.out_flash_param_clear_o_bt = QPushButton("清除测试点")
        self.out_flash_param_clear_s_bt = QPushButton("清除标准点")
        self.out_flash_param_clear_s_d_bt = QPushButton("预设标准点")

        out_flash_param_temp_cc_wg = QGroupBox("温度校正参数")
        out_flash_param_temp_cc_ly = QGridLayout(out_flash_param_temp_cc_wg)

        for i, sp in enumerate(self.out_flash_param_temp_sps):
            sp.setMaximumWidth(90)
            sp.setRange(-5, 5)
            sp.setDecimals(3)
            sp.setSingleStep(0.035)
            sp.setSuffix("℃")
            temp_ly = QHBoxLayout()
            temp_ly.setContentsMargins(5, 0, 5, 0)
            temp_ly.setSpacing(5)
            if i < 6:
                temp_ly.addWidget(QLabel(f"#{i + 1} 上加热体-{i + 1}"))
            elif i < 8:
                temp_ly.addWidget(QLabel(f"#{i + 1} 下加热体-{i - 5}"))
            elif i == 8:
                temp_ly.addWidget(QLabel(f"#{i + 1} 环境-{i - 7}"))
            else:
                temp_ly.addWidget(QLabel(f"目标点偏差-{('下', '上')[i-9]}"))
            temp_ly.addWidget(sp)
            out_flash_param_temp_cc_ly.addLayout(temp_ly, i // 6, i % 6)
        out_flash_param_ly.addWidget(out_flash_param_temp_cc_wg)

        out_flash_param_od_cc_wg = QGroupBox("OD校正参数")
        out_flash_param_od_cc_ly = QVBoxLayout(out_flash_param_od_cc_wg)
        self.out_flash_param_cc_sps = [QDoubleSpinBox(self) for _ in range(156)]
        for idx, sp in enumerate(self.out_flash_param_cc_sps):
            sp.setRange(0, 99999999)
            sp.setDecimals(0)
            sp.setValue(idx)
        for i in range(6):
            if i == 0:
                pp = 3
            else:
                pp = 2
            out_flash_param_od_cc_ly.addWidget(QHLine())
            for j in range(pp):
                wave = ("610", "550", "405")[j]
                temp_ly = QHBoxLayout()
                if j == 0:
                    channel = f"通道{i + 1}"
                else:
                    channel = f"     "
                temp_ly.addWidget(QLabel(f"{channel} {wave}"))
                for k in range(6):
                    temp_ly.addWidget(QVLine())
                    head = k * 1 + j * 12 + i * 24 + 36 - 12 * pp
                    temp_ly.addWidget(self.out_flash_param_cc_sps[head])
                    temp_ly.addWidget(self.out_flash_param_cc_sps[head + 6])
                out_flash_param_od_cc_ly.addLayout(temp_ly)
        out_flash_param_ly.addWidget(out_flash_param_od_cc_wg)

        out_flash_data_ly.addWidget(self.out_flash_data_te)
        out_flash_data_ly.addLayout(out_flash_temp_ly)
        out_flash_data_ly.addWidget(temperautre_raw_wg)
        out_flash_data_ly.addWidget(out_flash_param_gb)

        temp_ly = QHBoxLayout()
        temp_ly.setContentsMargins(5, 0, 5, 0)
        temp_ly.setSpacing(5)
        temp_ly.addWidget(self.out_flash_param_read_bt)
        temp_ly.addWidget(self.out_flash_param_write_bt)
        temp_ly.addWidget(self.out_flash_param_clear_o_bt)
        temp_ly.addWidget(self.out_flash_param_clear_s_bt)
        temp_ly.addWidget(self.out_flash_param_clear_s_d_bt)
        out_flash_data_ly.addLayout(temp_ly)

        self.out_flash_data_read_bt.clicked.connect(self.onOutFlashRead)
        self.out_flash_param_read_bt.clicked.connect(self.onOutFlashParamRead)
        self.out_flash_param_write_bt.clicked.connect(self.onOutFlashParamWrite)
        self.out_flash_data_dg = ModernDialog(self.out_flash_data_dg, self)
        self.out_flash_data_dg.mouseDoubleClickEvent = self.onOutFlashDoubleClicked
        self.out_flash_param_clear_o_bt.clicked.connect(self.onOutFlashParamCC_O)
        self.out_flash_param_clear_s_bt.clicked.connect(self.onOutFlashParamCC_S)
        self.out_flash_param_clear_s_d_bt.clicked.connect(self.onOutFlashParamCC_S_D)

    def onOutFlashParamCC_O(self, event):
        for idx, sp in enumerate(self.out_flash_param_cc_sps):
            if idx % 12 >= 6:
                sp.setValue(0)

    def onOutFlashParamCC_S(self, event):
        for idx, sp in enumerate(self.out_flash_param_cc_sps):
            if idx % 12 < 6:
                sp.setValue(0)

    def onOutFlashParamCC_S_D(self, event):
        for idx, sp in enumerate(self.out_flash_param_cc_sps):
            if idx % 12 < 6:
                if idx // 12 == 2:
                    sp.setValue(0)
                else:
                    sp.setValue(50 + (idx % 12) * 2000)

    def onOutFlashDoubleClicked(self, event):
        for sp in self.out_flash_param_cc_sps:
            sp.setValue(0)

    def onOutFlashParamRead(self, event):
        data = (*(struct.pack("H", 0)), *(struct.pack("H", 167)))
        self._serialSendPack(0xDD, data)

    def onOutFlashParamWrite(self, event):
        data = []
        for idx, sp in enumerate(self.out_flash_param_temp_sps):
            for d in struct.pack("f", sp.value()):
                data.append(d)
        for idx, sp in enumerate(self.out_flash_param_cc_sps):
            for d in struct.pack("I", int(sp.value())):
                data.append(d)
        for i in range(0, len(data), 224):
            sl = data[i : i + 224]
            start = [*struct.pack("H", i // 4)]
            num = [*struct.pack("H", len(sl) // 4)]
            self._serialSendPack(0xDD, start + num + sl)
        data = (0xFF, 0xFF)
        self._serialSendPack(0xDD, data)
        for sp in self.out_flash_param_cc_sps + self.out_flash_param_temp_sps:
            self._setColor(sp, nfg="red")
        QTimer.singleShot(500, partial(self.onOutFlashParamRead, event=False))

    def updateOutFlashParam(self, info):
        raw_pack = info.content
        start = struct.unpack("H", raw_pack[6:8])[0]
        num = struct.unpack("H", raw_pack[8:10])[0]
        for i in range(num):
            idx = start + i
            if idx < len(self.out_flash_param_temp_sps):
                value = struct.unpack("f", raw_pack[10 + i * 4 : 14 + i * 4])[0]
                sp = self.out_flash_param_temp_sps[idx]
            else:
                value = struct.unpack("I", raw_pack[10 + i * 4 : 14 + i * 4])[0]
                sp = self.out_flash_param_cc_sps[idx - len(self.out_flash_param_temp_sps)]
            sp.setValue(value)
            self._setColor(sp)

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
            msg = ModernMessageBox(self)
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
        if self.out_flash_start == 0x1000 and self.out_flash_length == 668:
            info = DC201_ParamInfo(self.out_flash_data[: start + length])
            plain_text = f"{info}\n{'=' * 100}\nraw bytes:\n{raw_text}"
            self.out_flash_data_te.setPlainText(plain_text)
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

    def createSysConf(self):
        self.sys_conf_gb = QGroupBox("系统")
        sys_conf_ly = QVBoxLayout(self.sys_conf_gb)
        sys_conf_ly.setContentsMargins(3, 3, 3, 3)
        sys_conf_ly.setSpacing(0)

        storge_ly = QHBoxLayout()
        storge_ly.setContentsMargins(3, 3, 3, 3)
        storge_ly.setSpacing(3)
        self.storge_gb = QGroupBox("存储")
        self.storge_gb.setLayout(QGridLayout())
        self.storge_gb.layout().setContentsMargins(3, 3, 3, 3)
        self.storge_gb.layout().setSpacing(3)
        self.storge_id_card_dialog_bt = QPushButton("ID Code 卡")
        self.storge_id_card_dialog_bt.setMaximumWidth(80)
        self.storge_flash_read_bt = QPushButton("外部 Flash")
        self.storge_flash_read_bt.setMaximumWidth(100)
        self.storge_gb.layout().addWidget(self.storge_id_card_dialog_bt, 0, 0)
        self.storge_gb.layout().addWidget(self.storge_flash_read_bt, 0, 1)
        self.debug_flag_gb = QGroupBox("调试")
        debug_flag_ly = QGridLayout(self.debug_flag_gb)
        debug_flag_ly.setSpacing(3)
        debug_flag_ly.setContentsMargins(3, 3, 3, 3)
        self.debug_flag_cbs = (QCheckBox("温度"), QCheckBox("告警"), QCheckBox("扫码"), QCheckBox("托盘"), QCheckBox("校正"))
        for i, cb in enumerate(self.debug_flag_cbs):
            debug_flag_ly.addWidget(cb, i // 3, i % 3)
        storge_ly.addStretch(1)
        storge_ly.addWidget(self.storge_gb)
        storge_ly.addStretch(1)
        storge_ly.addWidget(self.debug_flag_gb)
        storge_ly.addStretch(1)

        self.storge_id_card_dialog_bt.clicked.connect(self.onID_CardDialogShow)
        self.storge_flash_read_bt.clicked.connect(self.onOutFlashDialogShow)
        for i, cb in enumerate(self.debug_flag_cbs):
            cb.clicked.connect(partial(self.onDebugFlagChanged, idx=i))
        sys_conf_ly.addLayout(storge_ly)

        boot_ly = QHBoxLayout()
        boot_ly.setContentsMargins(3, 3, 3, 3)
        boot_ly.setSpacing(0)
        self.upgrade_bt = QPushButton("固件")
        self.upgrade_bt.setMaximumWidth(75)
        self.bootload_bt = QPushButton("Bootloader")
        self.bootload_bt.setMaximumWidth(75)
        self.reboot_bt = QPushButton("重启")
        self.reboot_bt.setMaximumWidth(75)
        self.selftest_bt = QPushButton("自检")
        self.selftest_bt.setMaximumWidth(75)
        self.debugtest_bt = QPushButton("Test")
        self.debugtest_bt.setMaximumWidth(35)
        self.debugtest_sp = QSpinBox()
        self.debugtest_sp.setMaximumWidth(35)
        self.debugtest_sp.setRange(0, 30)
        self.debugtest_cnt = 0
        boot_ly.addWidget(self.upgrade_bt)
        boot_ly.addWidget(self.bootload_bt)
        boot_ly.addWidget(self.reboot_bt)
        boot_ly.addWidget(self.selftest_bt)
        boot_ly.addWidget(self.debugtest_bt)
        boot_ly.addWidget(self.debugtest_sp)
        sys_conf_ly.addLayout(boot_ly)

        self.upgrade_bt.clicked.connect(self.onUpgrade)
        self.bootload_bt.clicked.connect(self.onBootload)
        self.reboot_bt.clicked.connect(self.onReboot)
        self.debugtest_bt.clicked.connect(self.onDebugTest)
        self.selftest_bt.mousePressEvent = self.onSelfCheck

    def onDebugTest(self, event):
        value = self.debugtest_sp.value()
        if value == 0:
            self.debugtest_cnt += 1
            if self.debugtest_cnt > 6:
                self.debugtest_cnt = 1
            self._serialSendPack(0xD4, (self.debugtest_cnt, 1, 6))
        elif value == 7:
            points = [random.randint(0, 65535) for _ in range(13)]
            logger.debug(f"points is [{', '.join(f'{i:04x}' for i in points)}]")
            points_str = "".join(f"{i:04x}" for i in points)
            data = f'6632{datetime.now().strftime("%y%m%d")}{value:02d}{points_str}{random.randint(0, 255):02X}'
            self._serialSendPack(0xD2, (*data.encode("ascii"),))
        elif 1 <= value <= 6:
            self.onCorrectMatplotStart()
            self._serialSendPack(0xD2, (0,))
        elif 21 <= value <= 26:
            data = [value]
            for i in range(6):
                data.append(self.matplot_conf_wavelength_cs[i].currentIndex() + 1)
                logger.debug(f"get temp sample index | {i} | data {self.temp_saple_data[i]}")
                if len(self.temp_saple_data[i]) >= 3:
                    p = int(statistics.mean(self.temp_saple_data[i][-3:]))
                else:
                    p = 0
                data.append(struct.pack("H", p)[0])
                data.append(struct.pack("H", p)[1])
            logger.debug(f"test debug send data | {data}")
            self._serialSendPack(0xD2, (*data,))
        elif 11 <= value <= 16:
            data = [12, value]
            tt = []
            for i in range(12):
                m = 50 + (value - 1) * 2000
                p = random.randint(m - 50, m + 100)
                data.append(struct.pack("H", p)[0])
                data.append(struct.pack("H", p)[1])
                tt.append(p)
            tt = sorted(tt[-7:])
            logger.debug(f"test sample data | {tt} | avg {statistics.mean(tt[1:-1]):.2f}")
            self._serialSendPack(0xD2, (*data,))

    def getErrorContent(self, error_code):
        for i in DC201ErrorCode:
            if error_code == i.value[0]:
                logger.debug(f"hit error | {i.value[0]:05d} | {i.value[1]}")
                return i.value[1]
        logger.error(f"unknow error code | {error_code}")
        return "Unknow Error Code"

    def showWarnInfo(self, info):
        error_code = struct.unpack("H", info.content[6:8])[0]
        error_content = self.getErrorContent(error_code)
        level = QMessageBox.Warning
        msg = ModernMessageBox(self)
        msg.setIcon(level)
        msg.setWindowTitle(f"故障信息 | {datetime.now()}")
        msg.setText(f"故障码 {error_code}\n{error_content}")
        msg.exec_()


if __name__ == "__main__":

    def trap_exc_during_debug(exc_type, exc_value, exc_traceback):
        logger.error(f"sys execpthook\n{''.join(traceback.format_exception(exc_type, exc_value, exc_traceback))}")

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
        app.exec_()
    except Exception:
        logger.error(f"exception in main loop \n{stackprinter.format()}")
