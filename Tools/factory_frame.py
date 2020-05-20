import queue
import time
from datetime import datetime
import struct
import serial
import serial.tools.list_ports
import simplejson
import stackprinter
from loguru import logger
from PyQt5.QtCore import QMutex, Qt, QThreadPool, QTimer
from PyQt5.QtGui import QIcon, QPalette
from PyQt5.QtWidgets import (
    QApplication,
    QComboBox,
    QDesktopWidget,
    QFrame,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QStatusBar,
    QVBoxLayout,
    QWidget,
)

from bytes_helper import bytesPuttyPrint, bytes2Float
from dc201_pack import DC201_PACK, DC201ErrorCode
from qt_serial import SerialRecvWorker, SerialSendWorker
from sample_graph import SampleGraph


ICON_PATH = "./icos/tt.ico"
CONFIG_PATH = "./conf/config.json"
CONFIG = dict()
try:
    with open(CONFIG_PATH, "r", encoding="utf-8") as f:
        CONFIG = simplejson.load(f)
    if "pd_criterion" not in CONFIG.keys() or "sh_criterion" not in CONFIG.keys():
        raise ValueError("lost key")
except Exception:
    logger.error(f"load conf failed \n{stackprinter.format()}")
    CONFIG = dict()
    CONFIG["log"] = dict(rotation="4 MB", retention=16)
    CONFIG["pd_criterion"] = {"610": [6000000, 14000000], "550": [6000000, 14000000], "405": [6000000, 14000000]}
    CONFIG["sh_criterion"] = {"610": (1434, 3080, 4882, 6894, 8818, 10578), "550": (2181, 3943, 5836, 7989, 10088, 12032), "405": (0, 0, 0, 0, 0, 0)}
    try:
        with open(CONFIG_PATH, "w", encoding="utf-8") as f:
            simplejson.dump(CONFIG, f)
    except Exception:
        logger.error(f"dump conf failed \n{stackprinter.format()}")

rotation = CONFIG.get("log", {}).get("rotation", "4 MB")
retention = CONFIG.get("log", {}).get("retention", 16)
logger.add("./log/dc201_fa.log", rotation=rotation, retention=retention, enqueue=True, encoding="utf8")


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
        self.setWindowTitle("DC201 FA")
        self.dd = DC201_PACK()
        self.pack_index = -1
        self.threadpool = QThreadPool()
        self.task_queue = queue.Queue()
        self.henji_queue = queue.Queue()
        self.serial = serial.Serial(port=None, baudrate=115200, timeout=0.01)
        self.init_UI()

    def create_plot(self):
        self.pd_plot_wg = QWidget(self)
        pd_plot_ly = QVBoxLayout(self.pd_plot_wg)
        pd_plot_ly.setSpacing(1)
        pd_plot_ly.setContentsMargins(2, 2, 2, 2)
        self.pd_plot_graph = SampleGraph(parent=self)
        pd_plot_ly.addWidget(self.pd_plot_graph.win)
        for i in range(6):
            self.pd_plot_graph.plot_data_new(name=f"CH-{i + 1}")

    def create_status_bar(self):
        self.status_bar = QStatusBar(self)
        self.status_bar.layout().setContentsMargins(5, 0, 5, 0)
        self.status_bar.layout().setSpacing(0)
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
        self.status_bar.addWidget(kirakira_wg, 0)

        self.version = ""
        self.version_lb = QLabel("版本: *.*")
        self.device_id_lb = QLabel("ID: *.*")

        self.status_bar.addWidget(self.version_lb, 0)
        self.status_bar.addWidget(self.device_id_lb, 0)

        self.setStatusBar(self.status_bar)

    def create_ctl_gb(self):
        self.ctl_gb = QGroupBox("控制")
        layout = QVBoxLayout(self.ctl_gb)
        self.selftest_ctl_pd_bt = QPushButton("&PD启动", checkable=True, toggled=self.on_selftest_pd_debug)
        layout.addWidget(self.selftest_ctl_pd_bt)
        self.selftest_ctl_clr_bt = QPushButton("&清除数据", checkable=True, toggled=self.on_selftest_clr_plot)
        layout.addWidget(self.selftest_ctl_clr_bt)
        self.selftest_all_test_bt = QPushButton("&全检", checkable=True, toggled=self.on_selftest_all_test)
        layout.addWidget(self.selftest_all_test_bt)
        self.selftest_reboot_bt = QPushButton("&重启", checkable=True, toggled=self._reboot_board)
        layout.addWidget(self.selftest_reboot_bt)

    def create_serial_gb(self):
        self.serial_gb = QGroupBox("串口")
        serial_ly = QGridLayout(self.serial_gb)
        serial_ly.setContentsMargins(3, 3, 3, 3)
        serial_ly.setSpacing(0)
        self.serial_switch_bt = QPushButton("打开串口")
        self.serial_refresh_bt = QPushButton("刷新(F5)", shortcut="F5")
        self.serial_post_co = QComboBox()
        self.serialRefreshPort()
        serial_ly.addWidget(self.serial_post_co, 0, 0, 1, 1)
        serial_ly.addWidget(self.serial_refresh_bt, 1, 0, 1, 1)
        serial_ly.addWidget(self.serial_switch_bt, 2, 0, 1, 1)
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

    def _getHeater(self):
        self._serialSendPack(0xD3)
        self._serialSendPack(0xD3, (0, 0, 4))
        self._serialSendPack(0xD3, (1, 0, 4))

    def _getDebuFlag(self):
        self._serialSendPack(0xD4)

    def _getDeviceID(self):
        self._serialSendPack(0xDF)

    def _reboot_board(self):
        self._serialSendPack(0xDC)

    def _fa_pd_ctl(self, stop=True):
        if stop:
            self._serialSendPack(0x34, (0,))
        else:
            self._serialSendPack(0x34, (1,))

    def _getStatus(self):
        self._serialSendPack(0x07)
        self._getHeater()
        self._getDebuFlag()
        self._getDeviceID()
        self._fa_pd_ctl()

    def _clearTaskQueue(self):
        while True:
            try:
                self.task_queue.get_nowait()
            except queue.Empty:
                break
            except Exception:
                logger.error(f"clear task queue exception \n{stackprinter.format()}")
                break

    def getPackIndex(self):
        self.pack_index += 1
        if self.pack_index <= 0 or self.pack_index > 255:
            self.pack_index = 1
        return self.pack_index

    def _serialSendPack(self, *args, **kwargs):
        try:
            pack = self.dd.buildPack(0x13, self.getPackIndex(), *args, **kwargs)
        except Exception:
            logger.error(f"build pack exception \n{stackprinter.format()}")
            raise
        self.task_queue.put(pack)
        if not self.serial.isOpen():
            logger.debug(f"try send | {bytesPuttyPrint(pack)}")

    def onSerialWorkerFinish(self):
        logger.info("emit from serial worker finish signal")

    def onSerialWorkerError(self, s):
        logger.error(f"emit from serial worker error signal | {s}")
        msg = QMessageBox(self)
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

    def onSerialSendWorkerResult(self, write_result):
        result, write_data, info = write_result
        logger.debug(f"write info {info} | data {write_data} | result | {result}")

    def onSerialRecvWorkerResult(self, info):
        cmd_type = info.content[5]
        if cmd_type == 0xDA:
            self.updateSelfCheckDialog(info)
        elif cmd_type == 0xB7:
            self.updateVersionLabel(info)
        elif cmd_type == 0xDF:
            self.udpateDeviceIDLabel(info)
        elif cmd_type == 0xB5:
            self.showWarnInfo(info)
        elif cmd_type == 0x34:
            self.parse_PD_Data(info)
        else:
            logger.debug(f"recv pack info | {info.text}")

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
            for i, lb in enumerate(self.selftest_motor_bts[4:6]):
                data = raw_bytes[8 + i]
                if data == 0:
                    self._set_btn_color(lb, bg="green")
                else:
                    self._set_btn_color(lb, bg="red")
        elif item == 7:
            if result > 0:
                self.selftest_motor_heater_gb.setStyleSheet("QGroupBox:title {color: red};")
            else:
                self.selftest_motor_heater_gb.setStyleSheet("QGroupBox:title {color: green};")
            for i, lb in enumerate(self.selftest_motor_bts[0:2]):
                data = raw_bytes[8 + i]
                if data == 0:
                    self._set_btn_color(lb, bg="green")
                else:
                    self._set_btn_color(lb, bg="red")
        elif item == 8:
            if result > 0:
                self.selftest_motor_tray_gb.setStyleSheet("QGroupBox:title {color: red};")
            else:
                self.selftest_motor_tray_gb.setStyleSheet("QGroupBox:title {color: green};")
            for i, lb in enumerate(self.selftest_motor_bts[2:4]):
                data = raw_bytes[8 + i]
                if data == 0:
                    self._set_btn_color(lb, bg="green")
                else:
                    self._set_btn_color(lb, bg="red")
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
            if len(raw_bytes) != 61:
                logger.error(f"self check pd data length error | {bytesPuttyPrint(raw_bytes)}")
            mask = raw_bytes[7]
            values = []
            for i in range(13):
                value = struct.unpack("I", raw_bytes[8 + 4 * i : 8 + 4 * (i + 1)])[0]
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

    def updateVersionLabel(self, info):
        self.version = f"{bytes2Float(info.content[6:10]):.3f}"

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
        self.version_lb.setText(f"V{self.version}.{datetime.strftime(self.device_datetime, '%Y%m%d.%H%M%S')}")

    def getErrorContent(self, error_code):
        for i in DC201ErrorCode:
            if error_code == i.value[0]:
                logger.error(f"hit error | {i.value[0]:05d} | {i.value[1]}")
                return i.value[1]
        for i in (DC201ErrorCode.eError_Comm_Main_UART, DC201ErrorCode.eError_Comm_Data_UART, DC201ErrorCode.eError_Comm_Out_UART):
            if (error_code >> 10) > 0 and error_code & 0xFF == i.value[0]:
                logger.error(f"hit serial dma error | {i.value[0]:05d} | {i.value[1]}")
                return f"{i.value[1]} | 硬件故障码 0x{(error_code >> 10):02X}"

    def showWarnInfo(self, info):
        error_code = struct.unpack("H", info.content[6:8])[0]
        error_content = self.getErrorContent(error_code)
        level = QMessageBox.Warning
        msg = QMessageBox(self)
        msg.setIcon(level)
        msg.setWindowTitle(f"故障信息 | {datetime.now()}")
        msg.setText(f"故障码 {error_code}\n{error_content}")
        msg.show()

    def parse_PD_Data(self, info):
        payload_byte = info.content[6:-1]
        logger.debug(f"recv pd debug payload_byte | {len(payload_byte)}")
        num = len(payload_byte) // 6 // 4
        for i in range(6):
            raw_data = [struct.unpack("f", payload_byte[24 * j + 4 * i : 24 * j + 4 * i + 4])[0] for j in range(num)]
            logger.debug(f"channel {i + 1} | raw data {raw_data}")
            self.pd_plot_graph.plot_data_bantch_update(i, raw_data)

    def onSerialStatistic(self, info):
        if info[0] == "w" and time.time() - self.kirakira_recv_time > 0.1:
            self.kirakira_recv_time = time.time()
            self.kirakira_send_lb.setStyleSheet("background-color : green; color : #3d3d3d;")
            QTimer.singleShot(100, lambda: self.kirakira_send_lb.setStyleSheet("background-color : white; color : #3d3d3d;"))
        elif info[0] == "r" and time.time() - self.kirakira_send_time > 0.1:
            self.kirakira_send_time = time.time()
            self.kirakira_recv_lb.setStyleSheet("background-color : red; color : #3d3d3d;")
            QTimer.singleShot(100, lambda: self.kirakira_recv_lb.setStyleSheet("background-color : white; color : #3d3d3d;"))

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

    def _set_btn_color(self, btn, bg=None):
        if not bg:
            btn.setStyleSheet("background-color: light gray")
        else:
            btn.setStyleSheet(f"background-color: {bg};")

    def _clear_widget_style_sheet(self, widget):
        widget.setStyleSheet("")
        layout = widget.layout()
        for i in range(layout.count()):
            layout.itemAt(i).widget().setStyleSheet("")

    def center(self):
        # logger.debug(f"invoke center in ModernWidget")
        qr = self.frameGeometry()
        cp = QDesktopWidget().availableGeometry().center()
        qr.moveCenter(cp)
        self.move(qr.topLeft())

    def init_UI(self):
        widget = QWidget()
        layout = QHBoxLayout(widget)
        left_ly = QGridLayout()
        self.create_selftest_wg()
        self.create_serial_gb()
        self.create_ctl_gb()
        self.create_status_bar()
        self.create_plot()
        left_ly.addWidget(self.selftest_wg, 0, 0, 8, 2)
        left_ly.addWidget(self.serial_gb, 8, 0, 3, 1)
        left_ly.addWidget(self.ctl_gb, 8, 1, 3, 1)
        layout.addLayout(left_ly)
        layout.addWidget(self.pd_plot_wg)
        self.setCentralWidget(widget)
        self.resize(900, 600)

    def resizeEvent(self, event):
        logger.debug(f"windows size | {self.size()}")

    def create_selftest_wg(self):
        self.selftest_wg = QWidget(self)
        self.selftest_temp_lbs = [QLabel("**.**") for _ in range(9)]
        selftest_wg_ly = QVBoxLayout(self.selftest_wg)
        selftest_temp_ly = QGridLayout()
        self.selftest_temp_top_gb = QGroupBox("上温度")
        self.selftest_temp_top_gb.setLayout(QVBoxLayout())
        self.selftest_temp_top_gb.layout().setContentsMargins(3, 3, 3, 3)
        for lb in self.selftest_temp_lbs[0:6]:
            lb.setAlignment(Qt.AlignCenter)
            lb.mouseReleaseEvent = self.onSelftest_temp_top_gb_Clicked
            self.selftest_temp_top_gb.layout().addWidget(lb)
        self.selftest_temp_btm_gb = QGroupBox("下温度")
        self.selftest_temp_btm_gb.setLayout(QVBoxLayout())
        self.selftest_temp_btm_gb.layout().setContentsMargins(3, 3, 3, 3)
        for lb in self.selftest_temp_lbs[6:8]:
            lb.setAlignment(Qt.AlignCenter)
            lb.mouseReleaseEvent = self.onSelftest_temp_btm_gb_Clicked
            self.selftest_temp_btm_gb.layout().addWidget(lb)
        self.selftest_temp_env_gb = QGroupBox("环境")
        self.selftest_temp_env_gb.setLayout(QVBoxLayout())
        self.selftest_temp_env_gb.layout().setContentsMargins(3, 3, 3, 3)
        for lb in self.selftest_temp_lbs[8:9]:
            lb.setAlignment(Qt.AlignCenter)
            lb.mouseReleaseEvent = self.onSelftest_temp_env_gb_Clicked
            self.selftest_temp_env_gb.layout().addWidget(lb)
        selftest_temp_ly.addWidget(self.selftest_temp_top_gb, 0, 0, 6, 1)
        selftest_temp_ly.addWidget(self.selftest_temp_btm_gb, 0, 1, 4, 1)
        selftest_temp_ly.addWidget(self.selftest_temp_env_gb, 4, 1, 2, 1)

        selftest_motor_ly = QGridLayout()
        self.selftest_motor_bts = [QPushButton(t, maximumWidth=30) for t in ("上", "下", "进", "出", "PD", "白")]
        self.selftest_motor_heater_gb = QGroupBox("上加热体")
        self.selftest_motor_heater_gb.setLayout(QHBoxLayout())
        self.selftest_motor_heater_gb.layout().setContentsMargins(3, 3, 3, 3)
        for bt in self.selftest_motor_bts[0:2]:
            bt.clicked.connect(self.onSelftest_motor_heater_gb_Clicked)
            bt.setContextMenuPolicy(Qt.CustomContextMenu)
            bt.customContextMenuRequested.connect(self.on_selftest_motor_heater_clicked)
            self.selftest_motor_heater_gb.layout().addWidget(bt)
        self.selftest_motor_tray_gb = QGroupBox("托盘")
        self.selftest_motor_tray_gb.setLayout(QHBoxLayout())
        self.selftest_motor_tray_gb.layout().setContentsMargins(3, 3, 3, 3)
        for bt in self.selftest_motor_bts[2:4]:
            bt.clicked.connect(self.onSelftest_motor_tray_gb_Clicked)
            bt.setContextMenuPolicy(Qt.CustomContextMenu)
            bt.customContextMenuRequested.connect(self.on_selftest_motor_tray_clicked)
            self.selftest_motor_tray_gb.layout().addWidget(bt)
        self.selftest_motor_white_gb = QGroupBox("白板")
        self.selftest_motor_white_gb.setLayout(QHBoxLayout())
        self.selftest_motor_white_gb.layout().setContentsMargins(3, 3, 3, 3)
        for bt in self.selftest_motor_bts[4:6]:
            bt.clicked.connect(self.onSelftest_motor_white_gb_Clicked)
            bt.setContextMenuPolicy(Qt.CustomContextMenu)
            bt.customContextMenuRequested.connect(self.on_selftest_motor_white_clicked)
            self.selftest_motor_white_gb.layout().addWidget(bt)
        self.selftest_motor_scan_gb = QGroupBox("扫码")
        self.selftest_motor_scan_gb.setLayout(QVBoxLayout())
        self.selftest_motor_scan_gb.layout().setContentsMargins(3, 3, 3, 3)
        self.selftest_motor_scan_m = QLabel("电机")
        self.selftest_motor_scan_m.mouseReleaseEvent = self.onSelftest_motor_scan_m_Clicked
        self.selftest_motor_scan_l = QLabel("*" * 10)
        self.selftest_motor_scan_l.mouseReleaseEvent = self.onSelftest_motor_scan_l_Clicked
        self.selftest_motor_scan_m.setAlignment(Qt.AlignCenter)
        self.selftest_motor_scan_l.setAlignment(Qt.AlignCenter)
        self.selftest_motor_scan_gb.layout().addWidget(self.selftest_motor_scan_m, stretch=1)
        self.selftest_motor_scan_gb.layout().addWidget(QHLine())
        self.selftest_motor_scan_gb.layout().addWidget(self.selftest_motor_scan_l, stretch=2)
        selftest_motor_ly.addWidget(self.selftest_motor_heater_gb, 0, 0, 1, 1)
        selftest_motor_ly.addWidget(self.selftest_motor_tray_gb, 1, 0, 1, 1)
        selftest_motor_ly.addWidget(self.selftest_motor_white_gb, 2, 0, 1, 1)
        selftest_motor_ly.addWidget(self.selftest_motor_scan_gb, 0, 1, 3, 1)

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
        self.selftest_lamp_610_gb.setLayout(QVBoxLayout())
        self.selftest_lamp_610_gb.layout().setContentsMargins(0, 0, 0, 0)
        self.selftest_lamp_610_gb.layout().setSpacing(0)
        self.selftest_lamp_550_gb = QGroupBox("550")
        self.selftest_lamp_550_gb.setLayout(QVBoxLayout())
        self.selftest_lamp_550_gb.layout().setContentsMargins(0, 0, 0, 0)
        self.selftest_lamp_550_gb.layout().setSpacing(0)
        self.selftest_lamp_405_gb = QGroupBox("405")
        self.selftest_lamp_405_gb.setLayout(QVBoxLayout())
        self.selftest_lamp_405_gb.layout().setContentsMargins(0, 0, 0, 0)
        self.selftest_lamp_405_gb.layout().setSpacing(0)

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
        selftest_lamp_ly.addWidget(self.selftest_lamp_610_gb, 0, 0, 6, 1)
        selftest_lamp_ly.addWidget(self.selftest_lamp_550_gb, 0, 1, 6, 1)
        selftest_lamp_ly.addWidget(self.selftest_lamp_405_gb, 6, 0, 1, 1)
        self.selftest_lamp_610_gb.mouseReleaseEvent = self.onSelftest_lamp_610_gb_Clicked
        self.selftest_lamp_550_gb.mouseReleaseEvent = self.onSelftest_lamp_550_gb_Clicked
        self.selftest_lamp_405_gb.mouseReleaseEvent = self.onSelftest_lamp_405_gb_Clicked
        self.selftest_lamp_all_bt = QPushButton("全波长", checkable=True, toggled=self.on_selftest_lamp_all)
        selftest_lamp_ly.addWidget(self.selftest_lamp_all_bt, 6, 1, 1, 1)

        selftest_wg_ly.addLayout(selftest_temp_ly)
        selftest_wg_ly.addLayout(selftest_motor_ly)
        selftest_wg_ly.addLayout(selftest_storge_ly)
        selftest_wg_ly.addLayout(selftest_lamp_ly)

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

    def on_selftest_pd_debug(self, event):
        logger.debug(f"clicked pd toggle | {event}")
        if event:
            self.selftest_ctl_pd_bt.setText("PD停止")
            self._serialSendPack(0x34, (1,))
        else:
            self.selftest_ctl_pd_bt.setText("PD启动")
            self._serialSendPack(0x34, (0,))

    def on_selftest_clr_plot(self, event):
        self.pd_plot_graph.clear_plot()
        for i in range(6):
            self.pd_plot_graph.plot_data_new(name=f"CH-{i + 1}")

    def on_selftest_all_test(self, event):
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

    def on_selftest_lamp_all(self, event):
        for idx, lb in enumerate(self.selftest_lamp_610_lbs):
            self._setColor(lb)
            lb.setText(f"{idx + 1}")
        for idx, lb in enumerate(self.selftest_lamp_550_lbs):
            self._setColor(lb)
            lb.setText(f"{idx + 1}")
        for idx, lb in enumerate(self.selftest_lamp_405_lbs):
            self._setColor(lb)
            lb.setText(f"{idx + 1}")
        self._serialSendPack(0xDA, (0x0B, 7))

    def on_selftest_motor_heater_clicked(self, event):
        sender = self.sender()
        idx = self.selftest_motor_bts[0:2].index(sender)
        self._serialSendPack(0xD0, (2, idx))

    def on_selftest_motor_tray_clicked(self, event):
        sender = self.sender()
        idx = self.selftest_motor_bts[2:4].index(sender)
        if idx == 0:
            self._serialSendPack(0x05)
        else:
            self._serialSendPack(0x04)

    def on_selftest_motor_white_clicked(self, event):
        sender = self.sender()
        idx = self.selftest_motor_bts[4:6].index(sender)
        self._serialSendPack(0xD0, (3, idx))


if __name__ == "__main__":
    import sys
    import traceback

    def trap_exc_during_debug(exc_type, exc_value, exc_traceback):
        t = "\n".join(traceback.format_exception(exc_type, exc_value, exc_traceback))
        logger.error(f"sys execpt hook\n{t}")

    sys.excepthook = trap_exc_during_debug
    try:
        app = QApplication(sys.argv)
        window = MainWindow()
        window.show()
        app.setWindowIcon(QIcon(ICON_PATH))
        app.exec_()
    except Exception:
        logger.error(f"exception in main loop \n{stackprinter.format()}")
