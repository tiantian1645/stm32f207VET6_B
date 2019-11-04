# http://doc.qt.io/qt-5/qt.html

import functools
import os
import queue
import sys
import time

import loguru
import serial
import serial.tools.list_ports
import stackprinter
from PyQt5.QtCore import QObject, QRunnable, Qt, QThreadPool, QTimer, pyqtSignal, pyqtSlot
from PyQt5.QtWidgets import (
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
    QProgressBar,
    QPushButton,
    QSpinBox,
    QStatusBar,
    QVBoxLayout,
    QWidget,
)

import pyperclip
from bytes_helper import bytesPuttyPrint
from dc201_pack import DC201_PACK, write_firmware_pack_FC
from pyqtgraph import GraphicsLayoutWidget, LabelItem, SignalProxy, mkPen

BARCODE_NAMES = ("B1", "B2", "B3", "B4", "B5", "B6", "QR")
TEMPERAUTRE_NAMES = ("下加热体:", "上加热体:")
LINE_COLORS = ("b", "g", "r", "c", "m", "y", "k", "w")
LINE_SYMBOLS = ("o", "s", "t", "d", "+")

logger = loguru.logger


class SeialWorkerSignals(QObject):
    owari = pyqtSignal()
    finished = pyqtSignal()
    error = pyqtSignal(tuple)
    result = pyqtSignal(object)
    serial_statistic = pyqtSignal(object)


class SerialWorker(QRunnable):
    def __init__(self, serial, task_queue, firm_queue, logger=loguru.logger):
        super(SerialWorker, self).__init__()
        self.serial = serial
        self.logger = logger
        self.task_queue = task_queue
        self.firm_queue = firm_queue
        self.dd = DC201_PACK()
        self.signals = SeialWorkerSignals()
        self.signals.owari.connect(self.stoptask)
        self.stop = False

    def stoptask(self):
        self.stop = True

    def getWriteData(self, timeout=0.01):
        try:
            data = self.task_queue.get(timeout=timeout)
        except queue.Empty:
            return None
        else:
            return data

    @pyqtSlot()
    def run(self):
        try:
            start_time = time.time()
            if not self.serial.isOpen():
                self.serial.open()
            recv_buffer = b""
            while True:
                # check stop
                if self.stop:
                    break
                # check write task
                write_data = self.getWriteData(0.001)
                if isinstance(write_data, bytes):
                    logger.debug("serial write data | {}".format(bytesPuttyPrint(write_data)))
                    self.serial.write(write_data)
                    self.signals.serial_statistic.emit(("w", write_data))
                elif write_data is not None:
                    self.logger.error("write data type error | {}".format(write_data))

                # check recv task
                recv_data = self.serial.read(2048)
                if len(recv_data) <= 0:
                    continue

                self.logger.debug("get raw bytes | {}".format(bytesPuttyPrint(recv_data)))
                self.signals.serial_statistic.emit(("r", recv_data))
                recv_buffer += recv_data
                if len(recv_buffer) < 7:
                    continue
                info = None
                for info in self.dd.iterIntactPack(recv_buffer):
                    recv_pack = info.content
                    if info.is_head and info.is_crc and len(recv_pack) >= 7:
                        ack = recv_pack[3]
                        fun_code = recv_pack[5]
                        if fun_code != 0xAA:
                            write_data = self.dd.buildPack(0x13, 0xFF - ack, 0xAA, (ack,))
                            self.serial.write(write_data)
                            self.signals.serial_statistic.emit(("w", write_data))
                            self.logger.warning("send pack | {}".format(bytesPuttyPrint(write_data)))
                            self.signals.result.emit(info)
                            if fun_code in (0xDC, 0xDD, 0xFB):
                                self.firm_queue.put(recv_pack[6])
                if info is not None:
                    if info.is_head and info.is_crc:
                        recv_buffer = b""
                    else:
                        recv_buffer = info.content
        except Exception:
            exctype, value = sys.exc_info()[:2]
            trace_back_text = stackprinter.format()
            logger.error("serial worker run excpetion \n{}".format(trace_back_text))
            self.signals.error.emit((exctype, value, trace_back_text))
        finally:
            logger.info("total run | {:.2f} S".format(time.time() - start_time))
            self.signals.finished.emit()


class MainWindow(QMainWindow):
    def __init__(self, *args, **kwargs):
        super(MainWindow, self).__init__(*args, **kwargs)
        self.setWindowTitle("DC201 工装测试")
        self.serial = serial.Serial(port=None, baudrate=115200, timeout=0.01)
        self.task_queue = queue.Queue()
        self.firm_queue = queue.Queue()
        self.threadpool = QThreadPool()
        self.dd = DC201_PACK()
        self.pack_index = 1
        self.device_id = 0x13
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
        widget = QWidget()
        layout = QGridLayout(widget)
        layout.addWidget(self.barcode_gb, 0, 0, 6, 1)
        layout.addWidget(self.motor_bg, 6, 0, 8, 1)
        layout.addWidget(self.serial_gb, 14, 0, 3, 1)
        layout.addWidget(self.matplot_wg, 0, 1, 17, 1)
        layout.setContentsMargins(0, 5, 0, 0)
        layout.setSpacing(0)
        self.setCentralWidget(widget)
        self.resize(870, 610)

    def resizeEvent(self, event):
        # logger.debug("windows size | {}".format(self.size()))
        pass

    def createStatusBar(self):
        self.status_bar = QStatusBar(self)
        self.status_bar.layout().setContentsMargins(0, 5, 0, 0)
        self.status_bar.layout().setSpacing(0)

        temperautre_wg = QWidget()
        temperautre_ly = QHBoxLayout(temperautre_wg)
        temperautre_ly.setContentsMargins(2, 2, 5, 5)
        temperautre_ly.setSpacing(5)
        self.temperautre_lbs = [QLabel("-" * 8) for _ in TEMPERAUTRE_NAMES]
        for i, name in enumerate(TEMPERAUTRE_NAMES):
            temperautre_ly.addWidget(QLabel(name))
            temperautre_ly.addWidget(self.temperautre_lbs[i])

        motor_tray_position_wg = QWidget()
        motor_tray_position_ly = QHBoxLayout(motor_tray_position_wg)
        motor_tray_position_ly.setContentsMargins(0, 0, 0, 0)
        motor_tray_position_ly.setSpacing(5)
        self.motor_tray_position = QLabel("******")
        motor_tray_position_ly.addWidget(QLabel("托盘电机位置"))
        motor_tray_position_ly.addWidget(self.motor_tray_position)

        kirakira_wg = QWidget()
        kirakira_ly = QHBoxLayout(kirakira_wg)
        kirakira_ly.setContentsMargins(2, 2, 5, 5)
        kirakira_ly.setSpacing(5)
        self.kirakira_recv_lb = QLabel("    ")
        self.kirakira_send_lb = QLabel("    ")
        kirakira_ly.addWidget(QLabel("发送"))
        kirakira_ly.addWidget(self.kirakira_send_lb)
        kirakira_ly.addWidget(QLabel("接收"))
        kirakira_ly.addWidget(self.kirakira_recv_lb)

        self.status_bar.addWidget(temperautre_wg, 0)
        self.status_bar.addWidget(motor_tray_position_wg, 0)
        self.status_bar.addWidget(kirakira_wg, 0)
        self.setStatusBar(self.status_bar)

    def updateTemperautre(self, info):
        temp_btm = int.from_bytes(info.content[6:8], byteorder="little") / 100
        temp_top = int.from_bytes(info.content[8:10], byteorder="little") / 100
        self.temperautre_lbs[0].setText("{:03.2f}℃".format(temp_btm))
        self.temperautre_lbs[1].setText("{:03.2f}℃".format(temp_top))

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

    def createBarcode(self):
        self.barcode_gb = QGroupBox("扫码")
        barcode_ly = QGridLayout(self.barcode_gb)
        self.barcode_lbs = [QLabel("*" * 10) for i in range(7)]
        self.motor_scan_bts = [QPushButton(BARCODE_NAMES[i]) for i in range(7)]
        for i in range(7):
            barcode_ly.addWidget(self.motor_scan_bts[i], i, 0)
            barcode_ly.addWidget(self.barcode_lbs[i], i, 1)
        self.barcode_scan_bt = QPushButton("开始")
        barcode_ly.addWidget(self.barcode_scan_bt, 7, 0, 1, 2)
        self.barcode_scan_bt.clicked.connect(self.onBarcodeScan)
        for i in range(7):
            self.motor_scan_bts[i].clicked.connect(functools.partial(self.onMotorScan, idx=i))

    def onBarcodeScan(self, event):
        for lb in self.barcode_lbs:
            lb.setText(("*" * 10))
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
        self.motor_bg = QGroupBox("电机控制")
        motor_ly = QGridLayout(self.motor_bg)
        self.motor_heater_bg = QGroupBox("上加热体")
        motor_heater_ly = QHBoxLayout(self.motor_heater_bg)
        self.motor_white_bg = QGroupBox("白板")
        motor_white_ly = QHBoxLayout(self.motor_white_bg)
        self.motor_tray_bg = QGroupBox("托盘")
        motor_tray_ly = QGridLayout(self.motor_tray_bg)

        self.motor_heater_up_bt = QPushButton("上")
        self.motor_heater_down_bt = QPushButton("下")
        motor_heater_ly.addWidget(self.motor_heater_up_bt)
        motor_heater_ly.addWidget(self.motor_heater_down_bt)

        self.motor_white_pd_bt = QPushButton("PD")
        self.motor_white_od_bt = QPushButton("白物质")
        motor_white_ly.addWidget(self.motor_white_pd_bt)
        motor_white_ly.addWidget(self.motor_white_od_bt)

        self.motor_tray_in_bt = QPushButton("进仓")
        self.motor_tray_scan_bt = QPushButton("扫码")
        self.motor_tray_out_bt = QPushButton("出仓")
        self.motor_tray_debug_cb = QCheckBox("调试")
        motor_tray_ly.addWidget(self.motor_tray_in_bt, 0, 0)
        motor_tray_ly.addWidget(self.motor_tray_scan_bt, 1, 0)
        motor_tray_ly.addWidget(self.motor_tray_out_bt, 0, 1)
        motor_tray_ly.addWidget(self.motor_tray_debug_cb, 1, 1)

        motor_ly.addWidget(self.motor_heater_bg, 0, 0, 1, 1)
        motor_ly.addWidget(self.motor_white_bg, 1, 0, 1, 1)
        motor_ly.addWidget(self.motor_tray_bg, 2, 0, 1, 1)

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

    def onMotorScan(self, event, idx):
        logger.debug("click motor scan idx | {}".format(idx))
        self.barcode_lbs[idx].setText("*" * 10)
        self._serialSendPack(0xD0, (idx,))

    def createSerial(self):
        self.serial_gb = QGroupBox("串口")
        serial_ly = QGridLayout(self.serial_gb)
        self.serial_switch_bt = QPushButton("打开串口")
        self.serial_refresh_bt = QPushButton("刷新")
        self.serial_post_co = QComboBox()
        self.serialRefreshPort()
        serial_ly.addWidget(self.serial_post_co, 0, 0, 1, 2)
        serial_ly.addWidget(self.serial_refresh_bt, 1, 0)
        serial_ly.addWidget(self.serial_switch_bt, 1, 1)
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
            self.serial_post_co.setEnabled(False)
            self.serial_refresh_bt.setEnabled(False)
            self.serial_switch_bt.setText("关闭串口")
            self._clearTaskQueue()
            self.worker = SerialWorker(self.serial, self.task_queue, self.firm_queue, logger)
            self.worker.signals.finished.connect(self.onSerialWorkerFinish)
            self.worker.signals.error.connect(self.onSerialWorkerError)
            self.worker.signals.result.connect(self.onSerialWorkerResult)
            self.worker.signals.serial_statistic.connect(self.onSerialStatistic)
            self.threadpool.start(self.worker)
            self._serialSendPack(0x07)
            logger.info("port update {} --> {}".format(old_port, self.serial.port))
        else:
            self.worker.signals.owari.emit()
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

    def onSerialWorkerResult(self, info):
        logger.info("emit from serial worker result signal | {}".format(info.text))
        cmd_type = info.content[5]
        if cmd_type == 0xA0:
            self.updateTemperautre(info)
        if cmd_type == 0xB0:
            self.updateMotorTrayPosition(info)
        elif cmd_type == 0xB2:
            self.updateBarcode(info)
        elif cmd_type == 0xB3:
            self.updateMatplotData(info)
        elif cmd_type == 0xB6:
            self.updateMatplotPlot()

    def onSerialStatistic(self, info):
        if info[0] == "w":
            self.kirakira_send_lb.setStyleSheet("background-color : green; color : #3d3d3d;")
            QTimer.singleShot(100, lambda: self.kirakira_send_lb.setStyleSheet("background-color : white; color : #3d3d3d;"))
        elif info[0] == "r":
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

        matplot_bt_ly = QHBoxLayout()
        self.matplot_start_bt = QPushButton("测试")
        self.matplot_cancel_bt = QPushButton("取消")
        self.upgrade_bt = QPushButton("升级")
        matplot_bt_ly.addWidget(self.matplot_start_bt)
        matplot_bt_ly.addWidget(self.matplot_cancel_bt)
        matplot_bt_ly.addWidget(self.upgrade_bt)
        matplot_ly.addLayout(matplot_bt_ly)

        matplot_conf_wg = QWidget()
        matplot_conf_ly = QHBoxLayout(matplot_conf_wg)
        matplot_conf_ly.setContentsMargins(2, 2, 2, 2)
        self.matplot_conf_houhou_cs = [QComboBox() for i in range(6)]
        self.matplot_conf_wavelength_cs = [QComboBox() for i in range(6)]
        self.matplot_conf_point_sps = [QSpinBox() for i in range(6)]
        for i in range(6):
            self.matplot_conf_houhou_cs[i].addItems(("无项目", "速率法", "终点法", "两点终点法"))
            self.matplot_conf_houhou_cs[i].setMaximumWidth(90)
            self.matplot_conf_wavelength_cs[i].addItems(("610", "550", "405"))
            self.matplot_conf_wavelength_cs[i].setMaximumWidth(90)
            self.matplot_conf_point_sps[i].setRange(0, 120)
            self.matplot_conf_point_sps[i].setMaximumWidth(90)
            matplot_conf_sub_gb = QGroupBox(BARCODE_NAMES[i])
            matplot_conf_sub_ly = QVBoxLayout(matplot_conf_sub_gb)
            matplot_conf_sub_ly.addWidget(self.matplot_conf_houhou_cs[i])
            matplot_conf_sub_ly.addWidget(self.matplot_conf_wavelength_cs[i])
            matplot_conf_sub_ly.addWidget(self.matplot_conf_point_sps[i])
            matplot_conf_ly.addWidget(matplot_conf_sub_gb)
        matplot_ly.addWidget(matplot_conf_wg)

        self.matplot_start_bt.clicked.connect(self.onMatplotStart)
        self.matplot_cancel_bt.clicked.connect(self.onMatplotCancel)
        self.upgrade_bt.clicked.connect(self.onUpgrade)
        self.updateMatplotPlot()

    def onPlotMouseMove(self, event):
        mouse_point = self.plot_wg.vb.mapSceneToView(event[0])
        self.plot_lb.setText(
            "<span style='font-size: 14pt; color: white'> x = %0.2f, <span style='color: white'> y = %0.2f</span>" % (mouse_point.x(), mouse_point.y())
        )

    def onMatplotStart(self, event):
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

    def onUpgrade(self, event):
        self.upgrade_dg = QDialog(self)
        self.upgrade_dg.setWindowTitle("固件升级")
        self.upgrade_dg_ly = QVBoxLayout(self.upgrade_dg)

        upgrade_temp_ly = QHBoxLayout()
        upgrade_temp_ly.addWidget(QLabel("固件路径"))
        self.upgrade_dg_lb = QLineEdit()
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

        self.upgrade_dg.resize(350, 75)
        self.upgrade_dg.exec_()

    def onUpgradeDialog(self, event):
        if event is True:
            file_path = self.upgrade_dg_lb.text()
            if os.path.isfile(file_path):
                self.upgrade_dg_bt.setEnabled(False)
                self._serialSendPack(0x0F)
                self.upgrade_dg_bt.setText("重启中")
                self._delay(2)
                while True:
                    try:
                        self.firm_queue.get_nowait()
                    except queue.Empty:
                        break
                self._serialSendPack(0xDD)
                self.upgrade_dg_bt.setText("擦除中")
                result = self.firm_queue.get(timeout=5)
                if result == 1:
                    self.upgrade_dg_bt.setText("擦除失败")
                    self._serialSendPack(0x0F)
                    self.upgrade_dg.setWindowTitle("固件升级失败 且原应用区可能已破坏")
                    return
                self.upgrade_dg_bt.setText("升级中")
                start = time.time()
                real_size = 0
                file_size = os.path.getsize(file_path)
                for pack in write_firmware_pack_FC(self.dd, file_path, chunk_size=1024):
                    self.task_queue.put(pack)
                    if len(pack) > 8:
                        delta = len(pack) - 8
                        logger.info("write pack addr 0x{:08X} ~ 0x{:08X}".format(real_size, real_size + delta))
                        real_size += delta
                        self.upgrade_pr.setValue(int(real_size * 100 / file_size))
                    try:
                        result = self.firm_queue.get(timeout=5)
                        if result != 0:
                            if result == 2:
                                logger.success("get all recv")
                                QTimer.singleShot(7000, lambda: self._serialSendPack(0x07))
                                self.upgrade_dg_bt.setText("完成")
                                self.upgrade_dg_bt.setEnabled(True)
                            else:
                                logger.error("get error recv")
                            break
                    except queue.Empty:
                        logger.error("get recv timeout")
                        break
                    except Exception:
                        logger.error("other error\n{}".format(stackprinter.format()))
                        break
                    QApplication.processEvents()
                logger.info(
                    "finish loop file | complete {:.2%} | {} / {} Byte | in {:.2f}S".format(real_size / file_size, real_size, file_size, time.time() - start)
                )
            else:
                self.upgrade_dg_bt.setChecked(False)
                self.upgrade_dg_lb.setText("请输入有效路径")
        elif event is False:
            QTimer.singleShot(3000, lambda: self._serialSendPack(0x07))
            self.upgrade_dg.close()

    def onUpgradeDialogFileSelect(self, event):
        fd = QFileDialog()
        filename, _ = fd.getOpenFileName(filter="BIN 文件 (*.bin)")
        if filename:
            self.upgrade_dg_lb.setText(filename)

    def updateMatplotPlot(self):
        self.plot_wg.clear()
        if len(self.matplot_data.items()) == 0:
            return
        i = 0
        records = []
        for k, v in self.matplot_data.items():
            color = LINE_COLORS[i]
            symbol = LINE_SYMBOLS[i % len(LINE_SYMBOLS)]
            self.plot_wg.plot(v, name="{}B{}".format("\u00A0", k), pen=mkPen(color=color), symbol=symbol, symbolSize=10, symbolBrush=(color))
            i += 1
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


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    app.exec_()
