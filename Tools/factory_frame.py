import queue
import struct
import time
from collections import namedtuple
from datetime import datetime
from functools import partial

import serial
import serial.tools.list_ports
import simplejson
import stackprinter
from loguru import logger
from PyQt5.QtCore import QMutex, Qt, QThreadPool, QTimer
from PyQt5.QtGui import QIcon, QPalette
from PyQt5.QtWidgets import (
    QApplication,
    QCheckBox,
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
    QSpinBox,
    QStatusBar,
    QVBoxLayout,
    QWidget,
)

from bytes_helper import bytes2Float, bytesPuttyPrint
from dc201_pack import DC201_PACK, DC201ErrorCode
from qt_serial import SerialRecvWorker, SerialSendWorker
from sample_graph import SampleGraph
from version import VERSION_FA
from wave_helper import get_freq_amp_from_data

ICON_PATH = "./icos/tt.ico"
CONFIG_PATH = "./conf/config_fa.json"
CONFIG = dict()
try:
    with open(CONFIG_PATH, "r", encoding="utf-8") as f:
        CONFIG = simplejson.load(f)
    if not all((i in CONFIG.keys() for i in ("log", "pd_criterion", "sh_criterion", "ignore_error_code"))):
        raise ValueError("lost key")
except Exception:
    logger.error(f"load conf failed \n{stackprinter.format()}")
    CONFIG = dict()
    CONFIG["log"] = dict(rotation="4 MB", retention=16)
    CONFIG["pd_criterion"] = {"610": [6000000, 14000000], "550": [6000000, 14000000], "405": [6000000, 14000000]}
    CONFIG["sh_criterion"] = {"610": (1434, 3080, 4882, 6894, 8818, 10578), "550": (2181, 3943, 5836, 7989, 10088, 12032), "405": (0, 0, 0, 0, 0, 0)}
    CONFIG["ignore_error_code"] = [300, 301, 302, 303, 304, 305]
    try:
        with open(CONFIG_PATH, "w", encoding="utf-8") as f:
            simplejson.dump(CONFIG, f, indent=4)
    except Exception:
        logger.error(f"dump conf failed \n{stackprinter.format()}")

rotation = CONFIG.get("log", {}).get("rotation", "4 MB")
retention = CONFIG.get("log", {}).get("retention", 16)
logger.add("./log/dc201_fa.log", rotation=rotation, retention=retention, enqueue=True, encoding="utf8")


Check_Info = namedtuple("Check_Info", "F A")


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
        self.setWindowTitle(f"DC201 FA {VERSION_FA}")
        self.dd = DC201_PACK()
        self.pack_index = -1
        self.threadpool = QThreadPool()
        self.task_queue = queue.Queue()
        self.henji_queue = queue.Queue()
        self.serial = serial.Serial(port=None, baudrate=115200, timeout=0.01)
        self.analyse_list = []
        self.analyse_lasst_idx = -1
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

    def create_led_gb(self):
        self.led_gb = QGroupBox("LED")
        layout = QGridLayout(self.led_gb)
        layout.setContentsMargins(1, 1, 1, 1)
        layout.setSpacing(1)
        self.led_sub_gbs = [QGroupBox(name) for name in ("610", "550", "405")]
        lys = [QHBoxLayout(gb) for gb in self.led_sub_gbs]
        self.led_dac_sps = [QSpinBox(minimum=0, maximum=800, value=10, singleStep=5, minimumWidth=60) for i in range(3)]
        for i, ly in enumerate(lys):
            sp = self.led_dac_sps[i]
            ly.addWidget(sp)
            ly.setContentsMargins(1, 1, 1, 1)
            ly.setSpacing(1)
        self.led_cks = []
        for i in range(13):
            c = QCheckBox()
            c.setChecked(True)
            self.led_cks.append(c)
            if i < 6:
                lys[0].addWidget(QLabel(f"{i % 6 + 1}"))
                lys[0].addWidget(c)
                lys[0].addStretch(1)
            elif i < 12:
                lys[1].addWidget(QLabel(f"{i % 6 + 1}"))
                lys[1].addWidget(c)
                lys[1].addStretch(1)
            else:
                lys[2].addWidget(QLabel(f"{i % 6 + 1}"))
                lys[2].addWidget(c)
                lys[2].addStretch(1)
        for idx, gb in enumerate(self.led_sub_gbs):
            gb.mouseDoubleClickEvent = partial(self.on_led_sub_gb_double_click, idx=idx)
        self.led_set_bt = QPushButton("设置", maximumWidth=40, clicked=self.on_led_set)
        self.led_reset_bt = QPushButton("重置", maximumWidth=40, clicked=self.on_led_reset)
        layout.addWidget(self.led_sub_gbs[0], 0, 0, 1, 6)
        layout.addWidget(self.led_sub_gbs[1], 0, 6, 1, 6)
        layout.addWidget(self.led_sub_gbs[2], 0, 12, 1, 2)
        layout.addWidget(self.led_set_bt, 0, 14, 1, 1)
        layout.addWidget(self.led_reset_bt, 0, 15, 1, 1)

    def create_pd_gb(self):
        self.pd_gb = QGroupBox("PD")
        layout = QHBoxLayout(self.pd_gb)
        self.selftest_pd_bt = QPushButton("PD测试(T)", clicked=self.on_selftest_pd_debug, shortcut="Ctrl+T")
        layout.addWidget(self.selftest_pd_bt)
        self.self_test_pd_lb = QLabel("测试结果")
        self.self_test_pd_lb.setAlignment(Qt.AlignCenter)
        layout.addWidget(self.self_test_pd_lb)
        self.selftest_clr_bt = QPushButton("清除数据(C)", clicked=self.on_selftest_clr_plot, shortcut="Ctrl+C")
        layout.addWidget(self.selftest_clr_bt)

    def create_ctl_gb(self):
        self.ctl_gb = QGroupBox("控制")
        layout = QHBoxLayout(self.ctl_gb)
        self.selftest_all_test_bt = QPushButton("全检(A)", clicked=self.on_selftest_all_test, shortcut="Ctrl+A")
        layout.addWidget(self.selftest_all_test_bt)
        self.selftest_reboot_bt = QPushButton("重启(R)", clicked=self._reboot_board, shortcut="Ctrl+R")
        layout.addWidget(self.selftest_reboot_bt)

    def create_serial_gb(self):
        self.serial_gb = QGroupBox("串口")
        serial_ly = QHBoxLayout(self.serial_gb)
        serial_ly.setContentsMargins(3, 3, 3, 3)
        serial_ly.setSpacing(0)
        self.serial_switch_bt = QPushButton("打开串口")
        self.serial_refresh_bt = QPushButton("刷新(F5)", shortcut="F5")
        self.serial_post_co = QComboBox()
        self.serialRefreshPort()
        serial_ly.addWidget(self.serial_post_co)
        serial_ly.addWidget(self.serial_refresh_bt)
        serial_ly.addWidget(self.serial_switch_bt)
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
            self.threadpool.waitForDone(1000)
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

    def _enable_factory_temp(self):
        self._serialSendPack(0xD4, (64, 1))

    def _getStatus(self):
        self._serialSendPack(0x07)
        self._getHeater()
        self._getDebuFlag()
        self._getDeviceID()
        self._enable_factory_temp()

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
        self.serial_recv_worker.signals.owari.emit()
        self.serial_send_worker.signals.owari.emit()
        self.threadpool.waitForDone(1000)
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
        elif cmd_type == 0x32:
            self.updatLED_DAC(info)
        elif cmd_type == 0x34:
            self.parse_PD_Data(info)
        else:
            logger.debug(f"recv pack info | {info.text}")

    def closeEvent(self, event):
        if not self.serial_post_co.isEnabled():
            self.serial_recv_worker.signals.owari.emit()
            self.serial_send_worker.signals.owari.emit()
            self.threadpool.waitForDone(1000)
            self.serial.close()
        sys.exit()

    def updateSelfCheckDialog(self, info):
        raw_bytes = info.content
        item = raw_bytes[6]
        result = raw_bytes[7]
        if item == 3:
            if result > 0:
                self.selftest_temp_env_gb.setStyleSheet("QGroupBox:title {color: red};")
            else:
                self.selftest_temp_env_gb.setStyleSheet("QGroupBox:title {color: green};")
            temp = struct.unpack("f", raw_bytes[8:12])[0]
            self.selftest_temp_env_lb.setText(f"{temp:.2f}")
            if 16 < temp < 46:
                self._setColor(self.selftest_temp_env_lb, nbg="green")
            else:
                self._setColor(self.selftest_temp_env_lb, nbg="red")
        elif item == 4:
            if result > 0:
                self._setColor(self.selftest_storge_lb_f, nbg="red")
            else:
                self._setColor(self.selftest_storge_lb_f, nbg="green")
        elif item == 6:
            if result > 0:
                self.selftest_motor_white_gb.setStyleSheet("QGroupBox:title {color: red};")
            else:
                self.selftest_motor_white_gb.setStyleSheet("QGroupBox:title {color: green};")
            for i, lb in enumerate(self.selftest_motor_white_bts[:2]):
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
            for i, lb in enumerate(self.selftest_motor_heater_bts[:2]):
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
            for i, lb in enumerate(self.selftest_motor_tray_bts[:2]):
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
        if error_code in CONFIG["ignore_error_code"]:
            logger.debug(f"ignore error code | {error_code}")
            return
        error_content = self.getErrorContent(error_code)
        level = QMessageBox.Warning
        msg = QMessageBox(self)
        msg.setIcon(level)
        msg.setWindowTitle(f"故障信息 | {datetime.now()}")
        msg.setText(f"故障码 {error_code}\n{error_content}")
        msg.show()

    def parse_PD_Data(self, info):
        def unstruct_u24(b):
            return b[2] << 16 | b[1] << 8 | b[0]

        payload_byte = info.content[6:-1]
        logger.debug(f"recv pd debug payload_byte | {len(payload_byte)}")
        channel = payload_byte[0]
        num = (len(payload_byte) - 1) // 3
        raw_data = [unstruct_u24(payload_byte[1 + 3 * j : 4 + 3 * j]) for j in range(num)]
        logger.success(f"channel {channel} | num {num} | raw data {raw_data}")
        if channel != self.analyse_lasst_idx:
            self.analyse_list.append(raw_data)
            self.analyse_lasst_idx = channel
        if len(self.analyse_list) != channel:
            logger.error("analyse data lost")
        elif channel == 6:
            check_info_list = []
            for idx, raw_data in enumerate(self.analyse_list):
                F, A = get_freq_amp_from_data(raw_data)
                check_info_list.append(Check_Info(F, A))
                logger.debug(f"channel {idx + 1} | F {F:3f} | A {A:.3f}")
                self.pd_plot_graph.plot_data_bantch_update(idx, raw_data)
            msg = QMessageBox(self)
            msg_content = "\n".join((f"通道 {i + 1} 周期 {c.T} 幅值 {c.A}" for i, c in enumerate(check_info_list)))
            if all(1170 <= F <= 1172 for i in check_info_list) and all(1800000 < i.A < 2800000 for i in check_info_list):
                msg.setIcon(QMessageBox.Information)
                msg.setWindowTitle("PD测试通过")
                logger.success(f"PD测试通过 | {check_info_list}")
                detail_text = f"PASS\n{msg_content}"
                msg.setText(f"PASS\n{msg_content}")
                self.self_test_pd_lb.setText("PD测试通过")
                self.self_test_pd_lb.setToolTip(detail_text)
                self.self_test_pd_lb.setStyleSheet("background-color : green; color : #3d3d3d;")
            else:
                msg.setIcon(QMessageBox.Critical)
                msg.setWindowTitle("PD测试失败")
                logger.error(f"PD测试失败 | {check_info_list}")
                detail_text = f"FAIL\n{msg_content}"
                msg.setText(f"FAIL\n{msg_content}")
                self.self_test_pd_lb.setText("PD测试失败")
                self.self_test_pd_lb.setToolTip(detail_text)
                self.self_test_pd_lb.setStyleSheet("background-color : red; color : #3d3d3d;")
                msg.exec_()

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
        layout = QVBoxLayout(widget)
        ctl_layout = QGridLayout()
        self.create_selftest_wg()
        self.create_serial_gb()
        self.create_pd_gb()
        self.create_ctl_gb()
        self.create_led_gb()
        self.create_status_bar()
        self.create_plot()
        ctl_layout.addWidget(self.selftest_wg, 0, 0, 3, 8)
        ctl_layout.addWidget(self.led_gb, 3, 0, 1, 8)
        ctl_layout.addWidget(self.serial_gb, 4, 0, 1, 3)
        ctl_layout.addWidget(self.pd_gb, 4, 3, 1, 3)
        ctl_layout.addWidget(self.ctl_gb, 4, 6, 1, 2)
        layout.addWidget(self.pd_plot_wg)
        layout.addLayout(ctl_layout)
        self.setCentralWidget(widget)
        self.resize(800, 600)

    def resizeEvent(self, event):
        logger.debug(f"windows size | {self.size()}")

    def create_selftest_wg(self):
        self.selftest_wg = QWidget(self)
        selftest_wg_ly = QGridLayout(self.selftest_wg)

        self.selftest_temp_env_gb = QGroupBox("环境", objectName="环境")
        self.selftest_temp_env_gb.setLayout(QHBoxLayout())
        self.selftest_temp_env_gb.layout().setSpacing(0)
        self.selftest_temp_env_gb.layout().setContentsMargins(0, 0, 0, 0)
        self.selftest_temp_env_lb = QLabel("**.**")
        self.selftest_temp_env_lb.setAlignment(Qt.AlignCenter)
        self.selftest_temp_env_lb.mouseReleaseEvent = self.onSelftest_temp_env_gb_Clicked
        self.selftest_temp_env_gb.layout().addWidget(self.selftest_temp_env_lb)

        selftest_motor_ly = QGridLayout()
        self.selftest_motor_heater_gb = QGroupBox("上加热体")
        self.selftest_motor_heater_gb.setLayout(QHBoxLayout())
        self.selftest_motor_heater_gb.layout().setContentsMargins(3, 3, 3, 3)
        self.selftest_motor_heater_gb.layout().setSpacing(3)
        self.selftest_motor_heater_bts = [QPushButton(t, maximumWidth=35) for t in ("上", "下", "Test")]
        for bt in self.selftest_motor_heater_bts:
            bt.clicked.connect(self.onSelftest_motor_heater_bts_clicked)
            self.selftest_motor_heater_gb.layout().addWidget(bt)
        self.selftest_motor_tray_gb = QGroupBox("托盘")
        self.selftest_motor_tray_gb.setLayout(QHBoxLayout())
        self.selftest_motor_tray_gb.layout().setContentsMargins(3, 3, 3, 3)
        self.selftest_motor_tray_gb.layout().setSpacing(3)
        self.selftest_motor_tray_bts = [QPushButton(t, maximumWidth=35) for t in ("进", "出", "Test")]
        for bt in self.selftest_motor_tray_bts:
            bt.clicked.connect(self.onSelftest_motor_tray_gb_clicked)
            self.selftest_motor_tray_gb.layout().addWidget(bt)
        self.selftest_motor_white_gb = QGroupBox("白板")
        self.selftest_motor_white_gb.setLayout(QHBoxLayout())
        self.selftest_motor_white_gb.layout().setContentsMargins(3, 3, 3, 3)
        self.selftest_motor_white_gb.layout().setSpacing(3)
        self.selftest_motor_white_bts = [QPushButton(t, maximumWidth=35) for t in ("PD", "白", "Test")]
        for bt in self.selftest_motor_white_bts:
            bt.clicked.connect(self.onSelftest_motor_white_gb_clicked)
            self.selftest_motor_white_gb.layout().addWidget(bt)
        self.selftest_motor_scan_gb = QGroupBox("扫码")
        self.selftest_motor_scan_gb.setLayout(QHBoxLayout())
        self.selftest_motor_scan_gb.layout().setContentsMargins(3, 3, 3, 3)
        self.selftest_motor_scan_gb.layout().setSpacing(3)
        self.selftest_motor_scan_m = QLabel("电机")
        self.selftest_motor_scan_m.mouseReleaseEvent = self.onSelftest_motor_scan_m_Clicked
        self.selftest_motor_scan_l = QLabel("*" * 10)
        self.selftest_motor_scan_l.mouseReleaseEvent = self.onSelftest_motor_scan_l_Clicked
        self.selftest_motor_scan_m.setAlignment(Qt.AlignCenter)
        self.selftest_motor_scan_l.setAlignment(Qt.AlignCenter)
        self.selftest_motor_scan_gb.layout().addWidget(self.selftest_motor_scan_m, stretch=1)
        self.selftest_motor_scan_gb.layout().addWidget(QVLine())
        self.selftest_motor_scan_gb.layout().addWidget(self.selftest_motor_scan_l, stretch=2)
        selftest_motor_ly.addWidget(self.selftest_motor_heater_gb, 0, 0, 1, 1)
        selftest_motor_ly.addWidget(self.selftest_motor_tray_gb, 0, 1, 1, 1)
        selftest_motor_ly.addWidget(self.selftest_motor_white_gb, 0, 2, 1, 1)
        selftest_motor_ly.addWidget(self.selftest_motor_scan_gb, 0, 3, 1, 3)

        selftest_storge_gb = QGroupBox("存储")
        selftest_storge_ly = QVBoxLayout(selftest_storge_gb)
        selftest_storge_ly.setContentsMargins(3, 3, 3, 3)
        selftest_storge_ly.setSpacing(3)
        self.selftest_storge_lb_f = QLabel("片外 Flash")
        self.selftest_storge_lb_f.mouseReleaseEvent = self.onSelftest_storge_lb_f_Clicked
        self.selftest_storge_lb_f.setAlignment(Qt.AlignCenter)
        selftest_storge_ly.addWidget(self.selftest_storge_lb_f)

        selftest_wg_ly.addWidget(self.selftest_temp_env_gb, 0, 0, 2, 1)
        selftest_wg_ly.addLayout(selftest_motor_ly, 0, 1, 2, 12)
        selftest_wg_ly.addWidget(selftest_storge_gb, 0, 13, 2, 1)

    def onSelftest_temp_env_gb_Clicked(self, event):
        self._clear_widget_style_sheet(self.selftest_temp_env_gb)
        self._serialSendPack(0xDA, (0x03,))
        self._enable_factory_temp()

    def onSelftest_motor_heater_bts_clicked(self, event):
        self._clear_widget_style_sheet(self.selftest_motor_heater_gb)
        idx = self.selftest_motor_heater_bts.index(self.sender())
        if idx == 2:
            self._serialSendPack(0xDA, (0x07,))
        else:
            self._serialSendPack(0xD0, (2, idx))

    def onSelftest_motor_tray_gb_clicked(self, event):
        self._clear_widget_style_sheet(self.selftest_motor_tray_gb)
        idx = self.selftest_motor_tray_bts.index(self.sender())
        if idx == 0:
            self._serialSendPack(0x05)
        elif idx == 1:
            self._serialSendPack(0x04)
        else:
            self._serialSendPack(0xDA, (0x08,))

    def onSelftest_motor_white_gb_clicked(self, event):
        self._clear_widget_style_sheet(self.selftest_motor_white_gb)
        sender = self.sender()
        idx = self.selftest_motor_white_bts.index(sender)
        if idx < 2:
            self._serialSendPack(0xD0, (3, idx))
        else:
            self._serialSendPack(0xDA, (0x06,))

    def onSelftest_motor_scan_m_Clicked(self, event):
        self._setColor(self.selftest_motor_scan_m)
        self._serialSendPack(0xDA, (0x09,))
        self.selftest_motor_scan_gb.setStyleSheet("QGroupBox:title {color: };")

    def onSelftest_motor_scan_l_Clicked(self, event):
        self._setColor(self.selftest_motor_scan_l)
        self.selftest_motor_scan_l.setText("*" * 10)
        self._serialSendPack(0xDA, (0x0A,))
        self.selftest_motor_scan_gb.setStyleSheet("QGroupBox:title {color: };")

    def onSelftest_storge_lb_f_Clicked(self, event):
        self._setColor(self.selftest_storge_lb_f)
        self._serialSendPack(0xDA, (0x04,))

    def on_selftest_pd_debug(self, event=None):
        self._serialSendPack(0x34, (1,))
        self.analyse_list.clear()
        self.analyse_lasst_idx = -1
        self.self_test_pd_lb.setText("测试结果")
        self.self_test_pd_lb.setStyleSheet("background-color : white; color : #3d3d3d;")

    def on_selftest_clr_plot(self, event=None):
        self.pd_plot_graph.clear_plot()
        for i in range(6):
            self.pd_plot_graph.plot_data_new(name=f"CH-{i + 1}")

    def on_selftest_all_test(self, event):
        self.on_selftest_clr_plot()
        self.selftest_temp_env_lb.setText("**.**")
        self.on_led_set()
        self._clear_widget_style_sheet(self.selftest_temp_env_gb)
        self._clear_widget_style_sheet(self.selftest_motor_heater_gb)
        self._clear_widget_style_sheet(self.selftest_motor_tray_gb)
        self._clear_widget_style_sheet(self.selftest_motor_white_gb)
        self._setColor(self.selftest_motor_scan_m)
        self._setColor(self.selftest_motor_scan_l)
        self.selftest_motor_scan_l.setText("*" * 10)
        self._setColor(self.selftest_storge_lb_f)
        self.on_selftest_pd_debug()
        self._serialSendPack(0xDA, (0xFA,))
        self._enable_factory_temp()

    def led_dac_set(self):
        data = bytearray()
        for idx, sp in enumerate(self.led_dac_sps):
            data += struct.pack("H", sp.value())
        self._serialSendPack(0x33, data)

    def on_led_set(self, event=None):
        mask = 0
        for i, c in enumerate(self.led_cks):
            if c.checkState():
                mask |= 1 << i
        data = struct.pack("H", mask)
        self._serialSendPack(0x35, (*data,))
        QTimer.singleShot(300, self.led_dac_set)
        QTimer.singleShot(800, self.on_led_dac_value_read)

    def on_led_dac_value_read(self, event=None):
        self._serialSendPack(0x32)

    def updatLED_DAC(self, info):
        for i, sp in enumerate(self.led_dac_sps):
            sp.setValue(int.from_bytes(info.content[6 + i * 2 : 8 + i * 2], byteorder="little"))

    def on_led_reset(self, event):
        for idx, sp in enumerate(self.led_dac_sps):
            sp.setValue(10)
        for i, c in enumerate(self.led_cks):
            c.setCheckState(0)
        self.on_led_set()

    def on_led_sub_gb_double_click(self, event, idx):
        event_button = event.button()
        if idx == 0:
            cks = self.led_cks[:6]
        elif idx == 1:
            cks = self.led_cks[6:12]
        else:
            cks = self.led_cks[12:]
        for ck in cks:
            if event_button == Qt.LeftButton:
                ck.setCheckState(2)
            elif event_button == Qt.RightButton:
                ck.setCheckState(0)
            elif event_button == Qt.MidButton:
                ck.setCheckState(0 if ck.checkState() else 2)


if __name__ == "__main__":
    import sys
    import traceback

    def trap_exc_during_debug(exc_type, exc_value, exc_traceback):
        logger.error(stackprinter.format())
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
