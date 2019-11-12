# http://doc.qt.io/qt-5/qt.html

import json
import os
import queue
import sys
import time
from datetime import datetime
from functools import partial
from hashlib import sha256
import struct

import loguru
import pyperclip
import serial
import serial.tools.list_ports
import stackprinter
from PySide2.QtCore import Qt, QThreadPool, QTimer
from PySide2.QtGui import QIcon
from PySide2.QtWidgets import (
    QApplication,
    QCheckBox,
    QComboBox,
    QDialog,
    QFileDialog,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QProgressBar,
    QPushButton,
    QSpinBox,
    QDoubleSpinBox,
    QStatusBar,
    QTextEdit,
    QVBoxLayout,
    QWidget,
)
from pyqtgraph import GraphicsLayoutWidget, LabelItem, SignalProxy, mkPen
from bytes_helper import bytesPuttyPrint, bytes2Float
from dc201_pack import DC201_PACK, write_firmware_pack_FC, DC201_ParamInfo
from qt_serial import SerialRecvWorker, SerialSendWorker

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
logger = loguru.logger


CONFIG_PATH = "./conf/config.json"
CONFIG = dict()
try:
    with open(CONFIG_PATH, "r") as f:
        CONFIG = json.load(f)
except Exception:
    logger.error("load conf failed \n{}".format(stackprinter.format()))
    CONFIG = dict()
    CONFIG["log"] = dict(rotation="1 MB", retention=50)
    try:
        with open(CONFIG_PATH, "w") as f:
            json.dump(CONFIG, f)
    except Exception:
        logger.error("dump conf failed \n{}".format(stackprinter.format()))

rotation = CONFIG.get("log", {}).get("rotation", "4 MB")
retention = CONFIG.get("log", {}).get("retention", 25)
logger.add("./log/dc201.log", rotation=rotation, retention=retention, enqueue=True)


class MainWindow(QMainWindow):
    def __init__(self, *args, **kwargs):
        super(MainWindow, self).__init__(*args, **kwargs)
        self.setWindowTitle("DC201 工装测试")
        self.serial = serial.Serial(port=None, baudrate=115200, timeout=0.01)
        self.task_queue = queue.Queue()
        self.henji_queue = queue.Queue()
        self.last_firm_path = None
        self.threadpool = QThreadPool()
        self.id_card_data = bytearray(4096)
        self.storge_time_start = 0
        self.out_flash_start = 0
        self.out_flash_data = bytearray()
        self.temp_btm_record = []
        self.temp_top_record = []
        self.temp_start_time = None
        self.temp_time_record = []
        self.temp_raw_records = [[] for _ in range(9)]
        self.temp_raw_start_time = None
        self.temp_raw_time_record = []
        self.dd = DC201_PACK()
        self.pack_index = 1
        self.device_id = 0x13
        self.serial_recv_worker = None
        self.serial_send_worker = None
        self.initUI()

    def _serialSendPack(self, *args, **kwargs):
        self.task_queue.put(self.dd.buildPack(self.device_id, self.getPackIndex(), *args, **kwargs))

    def _clearTaskQueue(self):
        while True:
            try:
                self.task_queue.get_nowait()
            except queue.Empty:
                break
            except Exception:
                logger.error("clear task queue exception \n{}".format(stackprinter.format()))
                break

    def _delay(self, timeout):
        start = time.time()
        while time.time() - start < timeout:
            time.sleep(0.2)
            QApplication.processEvents()

    def initUI(self):
        self.createBarcode()
        self.createMotor()
        self.createSerial()
        self.createMatplot()
        self.createStatusBar()
        self.createStorge()
        self.createBoot()
        widget = QWidget()
        layout = QGridLayout(widget)
        layout.addWidget(self.barcode_gb, 0, 0, 6, 2)
        layout.addWidget(self.motor_gb, 6, 0, 2, 2)
        layout.addWidget(self.storge_gb, 8, 0, 1, 2)
        layout.addWidget(self.boot_gb, 9, 0, 1, 2)
        layout.addWidget(self.serial_gb, 10, 0, 1, 2)
        layout.addWidget(self.matplot_wg, 0, 2, 11, 2)
        layout.setContentsMargins(0, 5, 0, 0)
        layout.setSpacing(0)
        image_path = "./icos/tt.ico"
        self.setWindowIcon(QIcon(image_path))
        self.setCentralWidget(widget)
        self.resize(850, 553)

    def resizeEvent(self, event):
        logger.debug("windows size | {}".format(self.size()))
        pass

    def closeEvent(self, event):
        logger.debug("invoke close event")
        if self.serial_recv_worker is not None:
            self.serial_recv_worker.signals.owari.emit()
        if self.serial_send_worker is not None:
            self.serial_send_worker.signals.owari.emit()
        time.sleep(0.2)

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
        self.temperature_plot_clear_bt = QPushButton("清零")
        temperature_plot_ly.addWidget(self.temperature_plot_win)
        temperature_plot_ly.addWidget(self.temperature_plot_clear_bt)
        self.temperature_plot_lb = LabelItem(justify="right")
        self.temperature_plot_win.addItem(self.temperature_plot_lb, 0, 0)
        self.temperature_plot_wg = self.temperature_plot_win.addPlot(row=0, col=0)
        self.temperature_plot_wg.addLegend()
        self.temperature_plot_wg.showGrid(x=True, y=True)
        self.temperature_plot_proxy = SignalProxy(self.temperature_plot_wg.scene().sigMouseMoved, rateLimit=60, slot=self.onTemperaturePlotMouseMove)
        self.temperature_btm_plot = self.temperature_plot_wg.plot(self.temp_time_record, self.temp_btm_record, name="\u00A0下加热体", pen=mkPen(color="r"))
        self.temperature_top_plot = self.temperature_plot_wg.plot(self.temp_time_record, self.temp_top_record, name="\u00A0上加热体", pen=mkPen(color="b"))
        self.temperature_plot_clear_bt.clicked.connect(self.onTemperautreDataClear)

        motor_tray_position_wg = QWidget()
        motor_tray_position_ly = QHBoxLayout(motor_tray_position_wg)
        motor_tray_position_ly.setContentsMargins(5, 0, 5, 0)
        motor_tray_position_ly.setSpacing(5)
        self.motor_tray_position = QLabel("******")
        motor_tray_position_ly.addWidget(QLabel("托盘电机位置"))
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

        self.status_bar.addWidget(temperautre_wg, 0)
        self.status_bar.addWidget(motor_tray_position_wg, 0)
        self.status_bar.addWidget(kirakira_wg, 0)
        self.status_bar.addWidget(self.version_lb, 0)
        self.setStatusBar(self.status_bar)

    def onTemperautreLabelClick(self, event):
        self.temperature_plot_dg.show()

    def onTemperautreDataClear(self, event):
        self.temp_btm_record.clear()
        self.temp_top_record.clear()
        self.temp_start_time = None
        self.temp_time_record.clear()
        self.temperature_plot_wg.clear()
        self.temperature_btm_plot = self.temperature_plot_wg.plot(self.temp_time_record, self.temp_btm_record, name="\u00A0下加热体", pen=mkPen(color="r"))
        self.temperature_top_plot = self.temperature_plot_wg.plot(self.temp_time_record, self.temp_top_record, name="\u00A0上加热体", pen=mkPen(color="b"))

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
        self.temperature_raw_plots = [
            self.temperature_raw_plot_wg.plot(self.temp_raw_time_record, self.temp_raw_records[i], name=f"\u00A0#{i + 1}", pen=mkPen(color=TEMP_RAW_COLORS[i]))
            for i in range(9)
        ]

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
            self.temperautre_lbs[0].setText("{:03.2f}℃".format(temp_btm))
            self.temperautre_lbs[0].setStyleSheet("")
        else:
            self.temperautre_lbs[0].setText("数据异常")
            self.temperautre_lbs[0].setStyleSheet("background-color: red;")
        if temp_top != 128:
            self.temperautre_lbs[1].setText("{:03.2f}℃".format(temp_top))
            self.temperautre_lbs[1].setStyleSheet("")
        else:
            self.temperautre_lbs[1].setText("数据异常")
            self.temperautre_lbs[1].setStyleSheet("background-color: red;")

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
        self.version_lb.setText(f"版本: {bytes2Float(info.content[6:10]):.1f}")

    def createBarcode(self):
        self.barcode_gb = QGroupBox("测试通道")
        barcode_ly = QGridLayout(self.barcode_gb)
        self.barcode_lbs = [QLabel("*" * 10) for i in range(7)]
        self.motor_scan_bts = [QPushButton(BARCODE_NAMES[i]) for i in range(7)]
        self.matplot_conf_houhou_cs = [QComboBox() for i in range(6)]
        self.matplot_conf_wavelength_cs = [QComboBox() for i in range(6)]
        self.matplot_conf_point_sps = [QSpinBox() for i in range(6)]
        self.barcode_scan_bt = QPushButton("统一扫码")
        self.barcode_scan_bt.setMaximumWidth(120)
        self.matplot_start_bt = QPushButton("测试")
        self.matplot_start_bt.setMaximumWidth(60)
        self.matplot_cancel_bt = QPushButton("取消")
        self.matplot_cancel_bt.setMaximumWidth(60)
        for i in range(7):
            self.motor_scan_bts[i].setMaximumWidth(45)
            barcode_ly.addWidget(self.motor_scan_bts[i], i, 0)
            barcode_ly.addWidget(self.barcode_lbs[i], i, 1)
            if i < 6:
                self.matplot_conf_houhou_cs[i].addItems(("无项目", "速率法", "终点法", "两点终点法"))
                self.matplot_conf_houhou_cs[i].setMaximumWidth(75)
                self.matplot_conf_houhou_cs[i].setCurrentIndex(1)
                self.matplot_conf_wavelength_cs[i].addItems(("610", "550", "405"))
                self.matplot_conf_wavelength_cs[i].setMaximumWidth(90)
                self.matplot_conf_wavelength_cs[i].setCurrentIndex(0)
                self.matplot_conf_point_sps[i].setRange(0, 120)
                self.matplot_conf_point_sps[i].setMaximumWidth(60)
                self.matplot_conf_point_sps[i].setValue(1)
                barcode_ly.addWidget(self.matplot_conf_houhou_cs[i], i, 2)
                barcode_ly.addWidget(self.matplot_conf_wavelength_cs[i], i, 3)
                barcode_ly.addWidget(self.matplot_conf_point_sps[i], i, 4)
            else:
                barcode_ly.addWidget(self.barcode_scan_bt, i, 2)
                barcode_ly.addWidget(self.matplot_start_bt, i, 3)
                barcode_ly.addWidget(self.matplot_cancel_bt, i, 4)
        self.barcode_scan_bt.clicked.connect(self.onBarcodeScan)
        self.matplot_start_bt.clicked.connect(self.onMatplotStart)
        self.matplot_cancel_bt.clicked.connect(self.onMatplotCancel)
        for i in range(7):
            self.motor_scan_bts[i].clicked.connect(partial(self.onMotorScan, idx=i))

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
            self.barcode_lbs[channel - 1].setText("{}...".format(text[:10]))
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
        self.motor_tray_debug_cb = QCheckBox("托盘调试")
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
        self.motor_tray_scan_bt.setEnabled(False)
        self.motor_tray_out_bt.clicked.connect(self.onMotorTrayOut)
        self.motor_tray_debug_cb.setChecked(False)
        self.motor_tray_debug_cb.stateChanged.connect(self.onMotorTrayDebug)

    def onMotorHeaterUp(self, event):
        self._serialSendPack(0xD3, (0,))

    def onMotorHeaterDown(self, event):
        self._serialSendPack(0xD3, (1,))

    def onMotorWhitePD(self, event):
        self._serialSendPack(0xD4, (0,))

    def onMotorWhiteOD(self, event):
        self._serialSendPack(0xD4, (1,))

    def onMotorTrayIn(self, event):
        if self.motor_tray_debug_cb.isChecked():
            self._serialSendPack(0xD1, (0,))
        else:
            self._serialSendPack(0x05)

    def onMotorTrayScan(self, event):
        self._serialSendPack(0xD1, (1,))

    def onMotorTrayOut(self, event):
        if self.motor_tray_debug_cb.isChecked():
            self._serialSendPack(0xD1, (2,))
        else:
            self._serialSendPack(0x04)

    def onMotorTrayDebug(self, event):
        logger.debug("motor tray debug checkbox event | {}".format(event))
        if event == 0:
            self.motor_tray_scan_bt.setEnabled(False)
        elif event == 2:
            self._serialSendPack(0xD3, (0,))
            self.motor_tray_scan_bt.setEnabled(True)

    def onMotorScan(self, event=None, idx=0):
        logger.debug("click motor scan idx | {}".format(idx))
        self.barcode_lbs[idx].setText("*" * 10)
        self._serialSendPack(0xD0, (idx,))

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
            self.serial_post_co.addItem("{}".format(serial_port.device))
            self.serial_post_co.setItemData(i, serial_port.description, Qt.ToolTipRole)
        if self.serial_post_co.count() == 0:
            self.serial_post_co.addItem("{:^20s}".format("None"))

    def onSerialRefresh(self, event):
        self.serialRefreshPort()

    def onSerialSwitch(self, event):
        logger.debug("serial switch button event | {}".format(event))
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

            self.serial_send_worker = SerialSendWorker(self.serial, self.task_queue, self.henji_queue, logger)
            self.serial_send_worker.signals.finished.connect(self.onSerialWorkerFinish)
            self.serial_send_worker.signals.error.connect(self.onSerialWorkerError)
            self.serial_send_worker.signals.serial_statistic.connect(self.onSerialStatistic)
            self.serial_send_worker.signals.result.connect(self.onSerialSendWorkerResult)
            self.serial_send_worker.signals.henji.connect(self.serial_recv_worker.setNeedHenji)
            self.threadpool.start(self.serial_send_worker)
            self._serialSendPack(0x07)
            logger.info("port update {} --> {}".format(old_port, self.serial.port))
        else:
            self.serial_recv_worker.signals.owari.emit()
            self.serial_send_worker.signals.owari.emit()
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
        logger.error("emit from serial worker error signal | {}".format(s))
        msg = QMessageBox(self)
        msg.setIcon(QMessageBox.Critical)
        msg.setWindowTitle("串口通讯故障")
        msg.setText(repr(s[1]))
        msg.exec_()
        self.serial.close()
        self.serial_post_co.setEnabled(True)
        self.serial_refresh_bt.setEnabled(True)
        self.serial_switch_bt.setText("打开串口")
        self.serial_switch_bt.setChecked(False)

    def onSerialRecvWorkerResult(self, info):
        logger.info("emit from serial worker result signal | {}".format(info.text))
        cmd_type = info.content[5]
        if cmd_type == 0xA0:
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
            self.updateMatplotPlot()
        elif cmd_type == 0xD1:
            self.updateOutFlashData(info)
        elif cmd_type == 0xB7:
            self.updateVersionLabel(info)
        elif cmd_type == 0xDD:
            self.updateOutFalshParam(info)
        elif cmd_type == 0xDE:
            self.updateTemperautreRaw(info)

    def onSerialSendWorkerResult(self, write_result):
        result, write_data, info = write_result
        if write_data[5] == 0x0F:
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
            return
        elif write_data[5] == 0xFC:
            if result:
                self.firm_wrote_size += len(write_data) - 8
                self.upgrade_pr.setValue(int(self.firm_wrote_size * 100 / self.firm_size))
                time_usage = time.time() - self.firm_start_time
                logger.info(
                    "finish loop file | complete {:.2%} | {} / {} Byte | in {:.2f}S".format(
                        self.firm_wrote_size / self.firm_size, self.firm_wrote_size, self.firm_size, time_usage
                    )
                )
                if write_data[6] == 0:
                    title = "固件升级 结束 | {} / {} Byte | {:.2f} S".format(self.firm_wrote_size, self.firm_size, time_usage)
                    QTimer.singleShot(7000, lambda: self._serialSendPack(0x07))
                    self.upgrade_dg_bt.setText("もう一回")
                    self.upgrade_dg_bt.setEnabled(True)
                    self.upgrade_dg_bt.setChecked(False)
                else:
                    title = "固件升级中... | {} Byte | {:.2f} S".format(self.firm_wrote_size, time_usage)
                    self.upgrade_dg_bt.setText("升级中")
                self.upgrade_dg.setWindowTitle(title)
            else:
                self.upgrade_dg_bt.setText("重试")
                self.upgrade_dg_bt.setEnabled(True)
                self.upgrade_dg_bt.setChecked(False)
            return
        elif write_data[5] == 0xD9:
            logger.info("ID卡信息包 | {}".format(write_result))
        logger.info("other | {}".format(write_result))

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
        self.plot_wg.showGrid(x=True, y=True)
        self.plot_proxy = SignalProxy(self.plot_wg.scene().sigMouseMoved, rateLimit=60, slot=self.onPlotMouseMove)
        matplot_ly.addWidget(self.plot_win)
        self.updateMatplotPlot()

    def onPlotMouseMove(self, event):
        mouse_point = self.plot_wg.vb.mapSceneToView(event[0])
        self.plot_lb.setText(
            "<span style='font-size: 14pt; color: white'> x = %0.2f, <span style='color: white'> y = %0.2f</span>" % (mouse_point.x(), mouse_point.y())
        )

    def onMatplotStart(self, event):
        self.initBarcodeScan()
        conf = []
        for i in range(6):
            conf.append(self.matplot_conf_houhou_cs[i].currentIndex())
            conf.append(self.matplot_conf_wavelength_cs[i].currentIndex() + 1)
            conf.append(self.matplot_conf_point_sps[i].value())
        logger.debug("get matplot cnf | {}".format(conf))
        self._serialSendPack(0x03, conf)
        self._serialSendPack(0x01)
        self.plot_wg.clear()

    def onMatplotCancel(self, event):
        self._serialSendPack(0x02)

    def onBootload(self, event):
        self._serialSendPack(0x0F)

    def onReboot(self, event):
        self._serialSendPack(0xDC)

    def onUpgrade(self, event):
        self.upgrade_dg = QDialog(self)
        self.upgrade_dg.setWindowTitle("固件升级")
        self.upgrade_dg_ly = QVBoxLayout(self.upgrade_dg)

        upgrade_temp_ly = QHBoxLayout()
        upgrade_temp_ly.addWidget(QLabel("固件路径"))
        self.upgrade_dg_lb = QLineEdit()
        if self.last_firm_path is not None and os.path.isfile(self.last_firm_path):
            self.upgrade_dg_lb.setText(self.last_firm_path)
            self.upgrade_dg.setWindowTitle("固件升级 | {}".format(self._getFileHash_SHA256(self.last_firm_path)))
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
            QTimer.singleShot(3000, lambda: self._serialSendPack(0x07))
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
            self.upgrade_dg.setWindowTitle("固件升级 | {}".format(self._getFileHash_SHA256(file_path)))

    def updateMatplotPlot(self):
        self.plot_wg.clear()
        if len(self.matplot_data.items()) == 0:
            return
        records = []
        for k in sorted(self.matplot_data.keys()):
            v = self.matplot_data.get(k, [0])
            color = LINE_COLORS[abs(k - 1) % len(LINE_COLORS)]
            symbol = LINE_SYMBOLS[abs(k - 1) % len(LINE_SYMBOLS)]
            self.plot_wg.plot(v, name="{}B{}".format("\u00A0", k), pen=mkPen(color=color), symbol=symbol, symbolSize=10, symbolBrush=(color))
            records.append("{} | {}".format(k, v))
        pyperclip.copy("\n".join(records))

    def updateMatplotData(self, info):
        length = info.content[6]
        channel = info.content[7]
        if len(info.content) != 2 * length + 9:
            logger.error("error data length | {} --> {} | {}".format(len(info.content), length, info.text))
            self.matplot_data[channel] = None
            return
        data = tuple((int.from_bytes(info.content[8 + i * 2 : 9 + i * 2], byteorder="little") for i in range(length)))
        self.matplot_data[channel] = data
        logger.debug("get data in channel | {} | {}".format(channel, data))

    def createStorge(self):
        self.storge_gb = QGroupBox("存储信息")
        storge_ly = QHBoxLayout(self.storge_gb)
        storge_ly.setContentsMargins(3, 3, 3, 3)
        storge_ly.setSpacing(0)
        self.storge_id_card_dialog_bt = QPushButton("ID卡信息")
        self.storge_id_card_dialog_bt.setMaximumWidth(120)
        self.storge_flash_read_bt = QPushButton("外部Flash信息")
        self.storge_flash_read_bt.setMaximumWidth(120)
        storge_ly.addWidget(self.storge_id_card_dialog_bt)
        storge_ly.addWidget(self.storge_flash_read_bt)

        self.storge_id_card_dialog_bt.clicked.connect(self.onID_CardDialogShow)
        self.storge_flash_read_bt.clicked.connect(self.onOutFlashDialogShow)

        self.id_card_data_dg = QDialog(self)
        self.id_card_data_dg.setWindowTitle("ID Card")
        self.id_card_data_dg.resize(730, 370)
        self.id_card_data_dg.resizeEvent = lambda x: logger.debug("windows size | {}".format(self.id_card_data_dg.size()))
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
        self.temperature_raw_plot_wg.showGrid(x=True, y=True)
        self.temperature_raw_plot_proxy = SignalProxy(self.temperature_raw_plot_wg.scene().sigMouseMoved, rateLimit=60, slot=self.onTemperatureRawPlotMouseMove)
        self.temperature_raw_plots = [
            self.temperature_raw_plot_wg.plot(self.temp_raw_time_record, self.temp_raw_records[i], name=f"\u00A0#{i + 1}", pen=mkPen(color=TEMP_RAW_COLORS[i]))
            for i in range(9)
        ]
        self.temperature_raw_plot_clear_bt.clicked.connect(self.onTemperautreRawDataClear)

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
        self.out_flash_data_num.setValue(36)
        self.out_flash_data_read_bt = QPushButton("读取")
        out_flash_temp_ly.addWidget(QLabel("地址"))
        out_flash_temp_ly.addWidget(self.out_flash_data_addr)
        out_flash_temp_ly.addWidget(QLabel("数量"))
        out_flash_temp_ly.addWidget(self.out_flash_data_num)
        out_flash_temp_ly.addWidget(self.out_flash_data_read_bt)

        out_flash_param_gb = QGroupBox("系统参数")
        out_flash_param_ly = QVBoxLayout(out_flash_param_gb)
        out_flash_param_ly.setSpacing(5)
        self.out_falsh_param_sps = [QDoubleSpinBox(self) for _ in range(9)]
        self.out_falsh_param_read_bt = QPushButton("读取")
        self.out_falsh_param_write_bt = QPushButton("写入")

        out_flash_param_temp_cc_wg = QGroupBox("温度校正参数")
        out_flash_param_temp_cc_ly = QGridLayout(out_flash_param_temp_cc_wg)

        for i, sp in enumerate(self.out_falsh_param_sps):
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
            else:
                temp_ly.addWidget(QLabel(f"#{i + 1} 环境-{i - 7}"))
            temp_ly.addWidget(sp)
            out_flash_param_temp_cc_ly.addLayout(temp_ly, i // 3, i % 3)
        out_flash_param_ly.addWidget(out_flash_param_temp_cc_wg)

        out_flash_data_ly.addWidget(self.out_flash_data_te)
        out_flash_data_ly.addLayout(out_flash_temp_ly)
        out_flash_data_ly.addWidget(temperautre_raw_wg)
        out_flash_data_ly.addWidget(out_flash_param_gb)
        temp_ly = QHBoxLayout()
        temp_ly.setContentsMargins(5, 0, 5, 0)
        temp_ly.setSpacing(5)
        temp_ly.addWidget(self.out_falsh_param_read_bt)
        temp_ly.addWidget(self.out_falsh_param_write_bt)
        out_flash_data_ly.addLayout(temp_ly)

        self.out_flash_data_read_bt.clicked.connect(self.onOutFlashRead)
        self.out_falsh_param_read_bt.clicked.connect(self.onOutFlashParamRead)
        self.out_falsh_param_write_bt.clicked.connect(self.onOutFlashParamWrite)

    def onOutFlashParamRead(self, event):
        for idx in range(9):
            data = (idx % 256, idx // 256)
            self._serialSendPack(0xDD, data)

    def onOutFlashParamWrite(self, event):
        for idx, sp in enumerate(self.out_falsh_param_sps):
            pv = bytearray(struct.pack("f", sp.value()))
            data = (idx % 256, idx // 256, *(i for i in pv))
            self._serialSendPack(0xDD, data)
        data = (0xFF, 0xFF)
        self._serialSendPack(0xDD, data)

    def updateOutFalshParam(self, info):
        raw_pack = info.content
        idx = struct.unpack("H", raw_pack[6:8])[0]
        value = struct.unpack("f", raw_pack[8:12])[0]
        self.out_falsh_param_sps[idx].setValue(value)

    def genBinaryData(self, data, unit=32, offset=0):
        result = []
        for i in range(0, len(data), unit):
            b = data[i : i + unit]
            result.append("0x{:04X} ~ 0x{:04X} | {}".format(i + offset, i + offset + unit - 1, bytesPuttyPrint(b)))
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
            msg = QMessageBox(self)
            msg.setIcon(QMessageBox.Warning)
            msg.setWindowTitle("文件大小异常 | {}".format(file_size))
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
            self.id_card_data_dg.setWindowTitle(
                "ID Card | sha256 {} | {:.4f} S".format(sha256(self.id_card_data).hexdigest(), time.time() - self.storge_time_start)
            )
        result = self.genBinaryData(self.id_card_data[: start + length])
        raw_text = "\n".join(result)
        # logger.debug("ID Card Raw Data \n{}".format(raw_text))
        self.id_card_data_te.setPlainText(raw_text)

    def onOutFlashDialogShow(self, event):
        self.out_flash_data_dg.show()

    def onOutFlashRead(self, event):
        addr = self.out_flash_data_addr.value()
        addr_list = ((addr >> 16) & 0xFF, (addr >> 8) & 0xFF, addr & 0xFF)
        num = self.out_flash_data_num.value()
        num_list = ((num >> 16) & 0xFF, (num >> 8) & 0xFF, num & 0xFF)
        self.out_flash_start = addr
        self.out_flash_data.clear()
        self.out_flash_data.extend((0xFF for _ in range(num)))
        self.out_flash_data_te.clear()
        self._serialSendPack(0xD6, (*addr_list, *num_list))

    def updateOutFlashData(self, info):
        raw_bytes = info.content
        start = int.from_bytes(raw_bytes[9:12], byteorder="little") - self.out_flash_start
        length = raw_bytes[12]
        self.out_flash_data[start : start + length] = raw_bytes[13 : 13 + length]
        self.out_flash_data_dg.setWindowTitle("外部Flash | sha256 {}".format(sha256(self.out_flash_data).hexdigest()))
        result = self.genBinaryData(self.out_flash_data[: start + length], offset=self.out_flash_start)
        raw_text = "\n".join(result)
        logger.debug("Out Falsh Raw Data \n{}".format(raw_text))
        if self.out_flash_start == 0x1000 and length >= 36:
            info = DC201_ParamInfo(self.out_flash_data[: start + length])
            plain_text = f"{info}\n{'=' * 100}\nraw bytes:\n{raw_text}"
            self.out_flash_data_te.setPlainText(plain_text)
        else:
            self.out_flash_data_te.setPlainText(raw_text)

    def createBoot(self):
        self.boot_gb = QGroupBox("系统")
        boot_ly = QHBoxLayout(self.boot_gb)
        boot_ly.setContentsMargins(3, 3, 3, 3)
        boot_ly.setSpacing(0)
        self.upgrade_bt = QPushButton("升级")
        self.upgrade_bt.setMaximumWidth(75)
        self.bootload_bt = QPushButton("Bootloader")
        self.bootload_bt.setMaximumWidth(75)
        self.reboot_bt = QPushButton("重启")
        self.reboot_bt.setMaximumWidth(75)
        self.selftest_bt = QPushButton("自检")
        self.selftest_bt.setMaximumWidth(75)
        boot_ly.addWidget(self.upgrade_bt)
        boot_ly.addWidget(self.bootload_bt)
        boot_ly.addWidget(self.reboot_bt)
        boot_ly.addWidget(self.selftest_bt)
        self.upgrade_bt.clicked.connect(self.onUpgrade)
        self.bootload_bt.clicked.connect(self.onBootload)
        self.reboot_bt.clicked.connect(self.onReboot)

    def getPeripheralType(self, p_type):
        if p_type == 0x00:
            return "上加热体电机"
        elif p_type == 0x01:
            return "白板电机"
        elif p_type == 0x02:
            return "托盘电机"
        elif p_type == 0x03:
            return "扫码电机"
        elif p_type == 0x04:
            return "上加热体温度"
        elif p_type == 0x05:
            return "下加热体温度"
        elif p_type == 0x06:
            return "环境温度"
        elif p_type == 0x07:
            return "上加热体温控"
        elif p_type == 0x08:
            return "下加热体温控"
        elif p_type == 0x09:
            return "Flash存储芯片"
        elif p_type == 0x0A:
            return "ID Code 卡"
        elif p_type == 0x0B:
            return "对外串口通信"
        elif p_type == 0x0C:
            return "主板通信"
        elif p_type == 0x0D:
            return "采集板通信"
        elif p_type == 0x0E:
            return "扫码枪"
        elif p_type == 0x0F:
            return "风扇"
        else:
            return "0x{:02X}".format(p_type)

    def getFaultType(self, p_type, e_type):
        if p_type in (0x00, 0x01, 0x02, 0x03):
            error_list = ("资源不可用", "电机运动超时", "电机驱动异常")
        elif p_type in (0x04, 0x05, 0x06, 0x07, 0x08):
            error_list = ("温度值无效", "温度持续过低", "温度持续过高")
        elif p_type in (0x09, 0x0A):
            error_list = ("硬件故障", "读取失败", "写入失败", "参数越限")
        elif p_type in (0x0B, 0x0C, 0x0D):
            error_list = ("资源不可用", "发送失败", "回应帧号不正确", "无回应", "未知功能码", "参数不正常")
        elif p_type in (0x0E,):
            error_list = ("配置失败",)
        elif p_type in (0x0F,):
            error_list = ("转速为零", "失速")
        result = []
        for i, e in enumerate(error_list):
            if e_type & (1 << i):
                result.append(e)
        if e_type > (1 << len(error_list)) - 1:
            result.append("0x{:02X}".format(e_type))
        return " | ".join(result)

    def getFaultLevel(self, p_type, e_type):
        level = QMessageBox.Warning
        # ("资源不可用", "电机运动超时", "电机驱动异常")
        if p_type in (0x00, 0x01, 0x02, 0x03):
            if e_type & 0x04:
                level = QMessageBox.Critical
        # ("温度值无效", "温度持续过低", "温度持续过高")
        elif p_type in (0x04, 0x05, 0x06, 0x07, 0x08):
            level = QMessageBox.Critical
        # ("硬件故障", "读取失败", "写入失败")
        elif p_type in (0x09, 0x0A):
            level = QMessageBox.Critical
        # ("资源不可用", "发送失败", "回应帧号不正确", "无回应")
        elif p_type in (0x0B, 0x0C, 0x0D):
            level = QMessageBox.Warning
        # ("配置失败", )
        elif p_type in (0x0E,):
            level = QMessageBox.Warning
        # ("转速为零", "失速")
        elif p_type in (0x0F,):
            level = QMessageBox.Critical
        return level

    def showWarnInfo(self, info):
        p_type, e_type = info.content[6:8]
        e = self.getFaultType(p_type, e_type)
        p = self.getPeripheralType(p_type)
        level = self.getFaultLevel(p_type, e_type)
        msg = QMessageBox(self)
        msg.setIcon(level)
        msg.setWindowTitle("故障信息 | {}".format(datetime.now()))
        msg.setText("设备类型: {}\n故障类型: {}".format(p, e))
        msg.show()


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    app.exec_()
