# http://doc.qt.io/qt-5/qt.html

import functools
import queue
import sys
import time

import loguru
import numpy as np
import serial
import serial.tools.list_ports
import stackprinter
from PyQt5.QtCore import QObject, QRunnable, Qt, QThreadPool, pyqtSignal, pyqtSlot
from PyQt5.QtWidgets import QApplication, QComboBox, QGridLayout, QGroupBox, QHBoxLayout, QLabel, QMainWindow, QPushButton, QWidget

import dc201_pack
from bytes_helper import bytesPuttyPrint
from matplotlib.backends.backend_qt5agg import FigureCanvas
from matplotlib.backends.backend_qt5agg import NavigationToolbar2QT as NavigationToolbar
from matplotlib.figure import Figure

BARCODE_NAMES = ("B1", "B2", "B3", "B4", "B5", "B6", "QR")
TEMPERAUTRE_NAMES = ("下:", "上:")

logger = loguru.logger


class SeialWorkerSignals(QObject):
    owari = pyqtSignal()
    finished = pyqtSignal()
    error = pyqtSignal(tuple)
    result = pyqtSignal(object)


class SerialWorker(QRunnable):
    def __init__(self, serial, task_queue, logger=loguru.logger):
        super(SerialWorker, self).__init__()
        self.serial = serial
        self.logger = logger
        self.task_queue = task_queue
        self.dd = dc201_pack.DC201_PACK()
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
                elif write_data is not None:
                    self.logger.error("write data type error | {}".format(write_data))

                # check recv task
                recv_data = self.serial.read(2048)
                if len(recv_data) <= 0:
                    continue

                self.logger.debug("get raw bytes | {}".format(bytesPuttyPrint(recv_data)))
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
                            send_data = self.dd.buildPack(0x13, 0xFF - ack, 0xAA, (ack,))
                            self.serial.write(send_data)
                            self.logger.warning("send pack | {}".format(bytesPuttyPrint(send_data)))
                            self.signals.result.emit(info)
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
        self.setWindowTitle("My Awesome App")
        self.serial = serial.Serial(port=None, baudrate=115200, timeout=0.01)
        self.task_queue = queue.Queue()
        self.threadpool = QThreadPool()
        self.dd = dc201_pack.DC201_PACK()
        self.pack_index = 1
        self.device_id = 0x13
        self.initUI()

    def __serialSendPack(self, *args, **kwargs):
        self.task_queue.put(self.dd.buildPack(self.device_id, self.getPackIndex(), *args, **kwargs))

    def initUI(self):
        self.createBarcode()
        self.createMotor()
        self.createTemperature()
        self.createSerial()
        self.createMathplot()
        widget = QWidget()
        layout = QGridLayout(widget)
        layout.addWidget(self.barcode_gb, 0, 0, 6, 1)
        layout.addWidget(self.motor_bg, 6, 0, 8, 1)
        layout.addWidget(self.temperautre_gb, 14, 0, 2, 1)
        layout.addWidget(self.serial_gb, 16, 0, 3, 1)
        layout.addWidget(self.static_canvas, 0, 1, 19, 1)
        self.setCentralWidget(widget)

    def createBarcode(self):
        self.barcode_gb = QGroupBox("扫码")
        barcode_ly = QGridLayout(self.barcode_gb)
        self.barcode_lbs = [QLabel("*" * 10) for i in range(7)]
        self.motor_scan_bts = [QPushButton(BARCODE_NAMES[i]) for i in range(7)]
        for i in range(7):
            if i == 6:
                barcode_ly.addWidget(QLabel("QR"), i, 0)
            else:
                barcode_ly.addWidget(QLabel("B{:1d}".format(i + 1)), i, 0)
            barcode_ly.addWidget(self.barcode_lbs[i], i, 1)
            barcode_ly.addWidget(self.motor_scan_bts[i], i, 2)
        self.barcode_scan_bt = QPushButton("开始")
        barcode_ly.addWidget(self.barcode_scan_bt, 7, 0, 1, 2)
        self.barcode_scan_bt.clicked.connect(self.onBarcodeScan)
        for i in range(7):
            self.motor_scan_bts[i].clicked.connect(functools.partial(self.onMotorScan, idx=i))

    def onBarcodeScan(self, event):
        for lb in self.barcode_lbs:
            lb.setText(("*" * 10))
        self.__serialSendPack(0x01)

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
        motor_tray_ly.addWidget(self.motor_tray_in_bt, 0, 0)
        motor_tray_ly.addWidget(self.motor_tray_scan_bt, 1, 0)
        motor_tray_ly.addWidget(self.motor_tray_out_bt, 0, 1)

        motor_ly.addWidget(self.motor_heater_bg, 0, 0, 1, 1)
        motor_ly.addWidget(self.motor_white_bg, 1, 0, 1, 1)
        motor_ly.addWidget(self.motor_tray_bg, 2, 0, 1, 1)

        self.motor_heater_up_bt.clicked.connect(self.onMotorHeaterUp)
        self.motor_heater_down_bt.clicked.connect(self.onMotorHeaterDown)
        self.motor_white_pd_bt.clicked.connect(self.onMotorWhitePD)
        self.motor_white_od_bt.clicked.connect(self.onMotorWhiteOD)
        self.motor_tray_in_bt.clicked.connect(self.onMotorTrayIn)
        self.motor_tray_scan_bt.clicked.connect(self.onMotorTrayScan)
        self.motor_tray_out_bt.clicked.connect(self.onMotorTrayOut)

    def onMotorHeaterUp(self, event):
        self.__serialSendPack(0xD3, (0,))

    def onMotorHeaterDown(self, event):
        self.__serialSendPack(0xD3, (1,))

    def onMotorWhitePD(self, event):
        self.__serialSendPack(0xD4, (0,))

    def onMotorWhiteOD(self, event):
        self.__serialSendPack(0xD4, (1,))

    def onMotorTrayIn(self, event):
        self.__serialSendPack(0xD1, (0,))

    def onMotorTrayScan(self, event):
        self.__serialSendPack(0xD1, (1,))

    def onMotorTrayOut(self, event):
        self.__serialSendPack(0xD1, (2,))

    def onMotorScan(self, event, idx):
        logger.debug("click motor scan idx | {}".format(idx))
        self.barcode_lbs[idx].setText("*" * 10)
        self.__serialSendPack(0xD0, (idx,))

    def createTemperature(self):
        self.temperautre_gb = QGroupBox("加热体温度")
        temperautre_ly = QHBoxLayout(self.temperautre_gb)
        self.temperautre_lbs = [QLabel("-" * 8) for _ in TEMPERAUTRE_NAMES]
        for i, name in enumerate(TEMPERAUTRE_NAMES):
            temperautre_ly.addWidget(QLabel(name))
            temperautre_ly.addWidget(self.temperautre_lbs[i])
            temperautre_ly.addSpacing(1)

    def updateTemperautre(self, info):
        temp_btm = int.from_bytes(info.content[6:8], byteorder="little") / 100
        temp_top = int.from_bytes(info.content[8:10], byteorder="little") / 100
        self.temperautre_lbs[0].setText("{:03.2f}℃".format(temp_btm))
        self.temperautre_lbs[1].setText("{:03.2f}℃".format(temp_top))

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
            self.worker = SerialWorker(self.serial, self.task_queue, logger)
            self.worker.signals.finished.connect(self.onSerialWorkerFinish)
            self.worker.signals.error.connect(self.onSerialWorkerError)
            self.worker.signals.result.connect(self.onSerialWorkerResult)
            self.threadpool.start(self.worker)
            self.task_queue.put(self.dd.buildPack(self.device_id, self.getPackIndex(), 0x07))
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
        elif cmd_type == 0xB2:
            self.updateBarcode(info)

    def createMathplot(self):
        self.static_canvas = FigureCanvas(Figure(figsize=(5, 3)))
        self.addToolBar(NavigationToolbar(self.static_canvas, self.static_canvas))
        self._static_ax = self.static_canvas.figure.subplots()
        t = np.linspace(0, 10, 501)
        self._static_ax.plot(t, np.tan(t), ".")


if __name__ == "__main__":
    app = QApplication(sys.argv)
    window = MainWindow()
    window.show()
    app.exec_()
