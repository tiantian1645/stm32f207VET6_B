import queue
import struct
import sys
import time
import traceback
from collections import namedtuple
from datetime import datetime
from functools import partial
from pathlib import Path

import serial
import serial.tools.list_ports
import simplejson
import stackprinter
from loguru import logger
from PyQt5.QtCore import QMutex, Qt, QThreadPool, QTimer, QDateTime
from PyQt5.QtGui import QIcon
from PyQt5.QtWidgets import (
    QApplication,
    QComboBox,
    QDesktopWidget,
    QFileDialog,
    QFrame,
    QGridLayout,
    QGroupBox,
    QHBoxLayout,
    QLabel,
    QLineEdit,
    QMainWindow,
    QMessageBox,
    QPushButton,
    QVBoxLayout,
    QWidget,
    QDateTimeEdit,
)

from bytes_helper import bytesPuttyPrint
from dc201_pack import DC201_PACK
from led_db import creat_new_record, dump_all_record, led_db_init
from qt_serial import SerialRecvWorker, SerialSendWorker
from qt_timeout_msgbox import TimerMessageBox
from version import VERSION_LED

ICON_PATH = "./icos/led.ico"
CONF_PATH = "./conf/led_config.json"
DEFAULT_CONF = {
    "610": {"num": 3, "range": (1000, 2000)},
    "550": {"num": 3, "range": (1000, 2000)},
    "405": {"num": 3, "range": (1000, 2000)},
}
SampleRawData = namedtuple("SampleRawData", "led channel white_pd gray_pd od")
logger.add("./log/dc201_led.log", rotation="8 MB", retention=12, enqueue=True, encoding="utf8")


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
        self.setWindowTitle(f"DC201 LED工装 {VERSION_LED}")
        self.serial = serial.Serial(port=None, baudrate=115200, timeout=0.01)
        self.task_queue = queue.Queue()
        self.henji_queue = queue.Queue()
        self.threadpool = QThreadPool()
        self.dd = DC201_PACK()
        self.pack_index = 1
        self.device_id = ""
        self.version = ""
        self.device_datetime = datetime.now()
        self.sample_raw_data_buffer = []
        self.serial_recv_worker = None
        self.serial_send_worker = None
        self.load_conf()
        self.initUI()
        self.center()

    def load_conf(self):
        try:
            self.conf_data = simplejson.loads(Path(CONF_PATH).read_text())
            logger.success(f"load config {self.conf_data}")
        except Exception:
            self.conf_data = DEFAULT_CONF
            self.dump_conf()

    def dump_conf(self):
        try:
            Path(CONF_PATH).write_text(simplejson.dumps(self.conf_data, indent=4))
        except Exception:
            logger.error(f"dump conf error\n{stackprinter.format()}")

    def getPackIndex(self):
        self.pack_index += 1
        if self.pack_index <= 0 or self.pack_index > 255:
            self.pack_index = 1
        return self.pack_index

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

    def center(self):
        # logger.debug(f"invoke center in ModernWidget")
        qr = self.frameGeometry()
        cp = QDesktopWidget().availableGeometry().center()
        qr.moveCenter(cp)
        self.move(qr.topLeft())

    def resizeEvent(self, event):
        # logger.debug(f"windows size | {self.size()}")
        pass

    def closeEvent(self, event):
        logger.debug("invoke close event")
        if self.serial_recv_worker is not None:
            self.serial_recv_worker.signals.owari.emit()
        if self.serial_send_worker is not None:
            self.serial_send_worker.signals.owari.emit()
        self.threadpool.waitForDone(1000)
        self.serial.close()
        sys.exit()

    def initUI(self):
        # 串口
        self.serial_wg = QWidget()
        serial_ly = QHBoxLayout(self.serial_wg)
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
        kirakira_wg = QWidget()
        kirakira_ly = QHBoxLayout(kirakira_wg)
        kirakira_ly.setContentsMargins(5, 0, 5, 0)
        kirakira_ly.setSpacing(5)
        self.kirakira_recv_lb = QLabel("  ")
        self.kirakira_recv_time = time.time()
        self.kirakira_send_lb = QLabel("  ")
        self.kirakira_send_time = time.time()
        kirakira_ly.addWidget(QLabel("发"))
        kirakira_ly.addWidget(self.kirakira_send_lb)
        kirakira_ly.addWidget(QLabel("收"))
        kirakira_ly.addWidget(self.kirakira_recv_lb)
        serial_ly.addWidget(QVLine())
        serial_ly.addWidget(kirakira_wg)
        serial_ly.addWidget(QVLine())
        # 测试结果
        self.sample_result_gb = QGroupBox("测试")
        sample_result_ly = QGridLayout(self.sample_result_gb)
        self.sample_result_610_lbs = [QLabel("***** | *.**", alignment=Qt.AlignRight | Qt.AlignVCenter) for _ in range(6)]
        self.sample_result_550_lbs = [QLabel("***** | *.**", alignment=Qt.AlignRight | Qt.AlignVCenter) for _ in range(6)]
        self.sample_result_405_lbs = [QLabel("***** | *.**", alignment=Qt.AlignRight | Qt.AlignVCenter) for _ in range(1)]
        self.sample_start_bts = [QPushButton(name, clicked=partial(self.on_sample_start, idx=i)) for i, name in enumerate(("610", "550", "405"))]
        for i in range(6):
            sample_result_ly.addWidget(QLabel(f"通道 {i+ 1}", alignment=Qt.AlignRight | Qt.AlignVCenter), 0, i + 1)

        for i, bt in enumerate(self.sample_start_bts):
            sample_result_ly.addWidget(bt, i + 1, 0)
        for i, lb in enumerate(self.sample_result_610_lbs):
            sample_result_ly.addWidget(lb, 1, i + 1)
        for i, lb in enumerate(self.sample_result_550_lbs):
            sample_result_ly.addWidget(lb, 2, i + 1)
        for i, lb in enumerate(self.sample_result_405_lbs):
            sample_result_ly.addWidget(lb, 3, i + 1)
        widget = QWidget()
        layout = QVBoxLayout(widget)
        layout.addWidget(self.sample_result_gb)
        temp_ly = QHBoxLayout()
        temp_ly.addWidget(self.serial_wg)
        self.sample_record_label_ed = QLineEdit()
        temp_ly.addWidget(QLabel("标签:"))
        temp_ly.addWidget(self.sample_record_label_ed)
        self.sample_record_dump_filte_datetime = QDateTimeEdit(calendarPopup=True)
        self.sample_record_dump_filte_datetime.setDateTime(QDateTime.currentDateTime())
        temp_ly.addWidget(self.sample_record_dump_filte_datetime)
        self.last_db_dump_dir = Path("./")
        temp_ly.addWidget(QPushButton("导出", clicked=self.on_sample_record_output))
        layout.addLayout(temp_ly)
        self.setWindowIcon(QIcon(ICON_PATH))
        self.setCentralWidget(widget)
        self.resize(650, 200)

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

    def _getStatus(self):
        self._serialSendPack(0x07)
        self._serialSendPack(0xDF)

    def sample_bts_disable(self):
        for bt in self.sample_start_bts:
            bt.setEnabled(False)

    def sample_bts_enable(self):
        for bt in self.sample_start_bts:
            bt.setEnabled(True)

    def on_sample_start(self, event, idx=0):
        self.sample_bts_disable()
        if idx == 0:
            for lb in self.sample_result_610_lbs:
                lb.setText("***** | *.**")
                lb.setToolTip("-----")
                lb.setStyleSheet("")
        elif idx == 1:
            for lb in self.sample_result_550_lbs:
                lb.setText("***** | *.**")
                lb.setToolTip("-----")
                lb.setStyleSheet("")
        elif idx == 2:
            for lb in self.sample_result_405_lbs:
                lb.setText("***** | *.**")
                lb.setToolTip("-----")
                lb.setStyleSheet("")
        self.sample_idx = idx
        self._serialSendPack(0x01)
        point_num = self.conf_data[["610", "550", "405"][idx]]["num"]
        if idx == 2:
            conf_data = [0x01, idx + 0x01, point_num] + [0x00, 0x01, 0x00] * 5 + [0x03]
        else:
            conf_data = [0x01, idx + 0x01, point_num] * 6 + [0x03]
        self._serialSendPack(0x08, conf_data)
        QTimer.singleShot(point_num * 10 * 1000, self.sample_bts_enable)

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

    def onSerialRecvWorkerResult(self, info):
        cmd_type = info.content[5]
        logger.info(f"emit from serial worker result signal | cmd_type 0x{cmd_type:02X} | {info.text}")
        if cmd_type == 0xB3:
            length = info.content[6]
            channel = info.content[7]
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
                if channel == 1:
                    self.sample_raw_data_buffer.clear()
                data = []
                for i in range(length):
                    data.append(struct.unpack("I", info.content[8 + i * 12 + 0 : 8 + i * 12 + 4])[0])
                for i in range(length):
                    data.append(struct.unpack("I", info.content[8 + i * 12 + 4 : 8 + i * 12 + 8])[0])
                for i in range(length):
                    data.append(struct.unpack("H", info.content[8 + i * 12 + 8 : 8 + i * 12 + 10])[0])
                for i in range(length):
                    data.append(struct.unpack("H", info.content[8 + i * 12 + 10 : 8 + i * 12 + 12])[0])
                logger.debug(f"hit :length {length} channel {channel} data | {data} | {bytesPuttyPrint(info.content)}")
                led_name = ["610", "550", "405"][self.sample_idx]
                sample_raw_data = SampleRawData(
                    led=led_name,
                    channel=channel,
                    white_pd=data[0 * length : 1 * length],
                    gray_pd=data[1 * length : 2 * length],
                    od=data[2 * length : 3 * length],
                )
                self.sample_raw_data_buffer.append(sample_raw_data)
                v_min = min(sample_raw_data.od)
                v_max = max(sample_raw_data.od)
                c_min = min(self.conf_data[led_name]["range"])
                c_max = max(self.conf_data[led_name]["range"])
                sample_finish = False
                if self.sample_idx == 0:
                    if channel == 6:
                        sample_finish = True
                    lb = self.sample_result_610_lbs[channel - 1]
                elif self.sample_idx == 1:
                    if channel == 6:
                        sample_finish = True
                    lb = self.sample_result_550_lbs[channel - 1]
                elif self.sample_idx == 2:
                    if channel == 1:
                        sample_finish = True
                    lb = self.sample_result_405_lbs[channel - 1]
                else:
                    lb = None
                    logger.error(f"error self.sample_idx {self.sample_idx}")
                    return
                if v_min < c_min or v_max > c_max:
                    lb.setStyleSheet("background-color: red")
                else:
                    lb.setStyleSheet("background-color: green")
                lb.setToolTip(f"W_PD {sample_raw_data.white_pd} | R_PD {sample_raw_data.gray_pd} | OD {sample_raw_data.od}")
                avg = sum(sample_raw_data.od) / length
                avg_white = sum(sample_raw_data.white_pd) / length
                avg_gray = sum(sample_raw_data.gray_pd) / length
                lb.setText(f"{avg:.0f} | {avg_white / avg_gray:.2f}")
                if sample_finish:
                    self.sample_bts_enable()
                    logger.debug(f"sample finish {self.sample_raw_data_buffer}")
                    occur = datetime.now()
                    for srd in self.sample_raw_data_buffer:
                        creat_new_record(
                            label="no_label" if not self.sample_record_label_ed.text() else self.sample_record_label_ed.text(),
                            occur=occur,
                            led=led_name,
                            channel=srd.channel,
                            white_pd=srd.white_pd,
                            gray_pd=srd.gray_pd,
                            od=srd.od,
                        )
                    msg = TimerMessageBox(self)
                    msg.setIcon(QMessageBox.Information)
                    msg.setWindowTitle("测试完成")
                    od_statistic = "\n".join(f"\t通道 {i.channel}: {sum(i.od) / len(i.od):.0f}\t\t\t" for i in self.sample_raw_data_buffer)
                    msg.setText(f"{led_name} OD")
                    msg.setInformativeText(od_statistic)
                    detailed_text = (
                        "白板PD\n"
                        + "\n".join(f"通道 {i.channel} {i.white_pd}" for i in self.sample_raw_data_buffer)
                        + "\n\n==================\n\n"
                        + "灰条PD\n"
                        + "\n".join(f"通道 {i.channel} {i.gray_pd}" for i in self.sample_raw_data_buffer)
                        + "\n\n==================\n\n"
                        + "OD\n"
                        + "\n".join(f"通道 {i.channel} {i.od}" for i in self.sample_raw_data_buffer)
                    )
                    msg.setDetailedText(detailed_text)
                    msg.setFixedSize(300, 200)
                    msg.show()
            else:
                logger.error(f"error data length | {len(info.content)} --> {length} | {info.text}")
                return

    def onSerialSendWorkerResult(self, write_result):
        result, write_data, info = write_result
        logger.debug(f"send result | {result} | write {bytesPuttyPrint(write_data)} | info | {info}")

    def onSerialStatistic(self, info):
        if info[0] == "w" and time.time() - self.kirakira_recv_time > 0.1:
            self.kirakira_recv_time = time.time()
            self.kirakira_send_lb.setStyleSheet("background-color : green; color : #3d3d3d;")
            QTimer.singleShot(100, lambda: self.kirakira_send_lb.setStyleSheet("background-color : white; color : #3d3d3d;"))
        elif info[0] == "r" and time.time() - self.kirakira_send_time > 0.1:
            self.kirakira_send_time = time.time()
            self.kirakira_recv_lb.setStyleSheet("background-color : red; color : #3d3d3d;")
            QTimer.singleShot(100, lambda: self.kirakira_recv_lb.setStyleSheet("background-color : white; color : #3d3d3d;"))

    def on_sample_record_output(self, event=None):
        start_datetime = self.sample_record_dump_filte_datetime.dateTime().toPyDateTime()
        logger.debug(f"dump record after {start_datetime}")
        fd = QFileDialog()
        default_path = self.last_db_dump_dir / f"dc201_led_{datetime.now():%Y-%m-%d_%H%M%S}.csv"
        file_path, _ = fd.getSaveFileName(filter="CSV 逗号分隔值文件 (*.csv)", directory=default_path.as_posix())
        if file_path:
            result = dump_all_record(file_path, start_datteime=start_datetime)
            msg = QMessageBox(self)
            if result is False:
                msg.setIcon(QMessageBox.Critical)
                msg.setText("导出失败")
            else:
                msg.setIcon(QMessageBox.Information)
                msg.setText("导出成功")
            msg.setInformativeText(file_path)
            msg.show()
            self.last_db_dump_dir = Path(file_path).parent


if __name__ == "__main__":

    def trap_exc_during_debug(exc_type, exc_value, exc_traceback):
        t = "\n".join(traceback.format_exception(exc_type, exc_value, exc_traceback))
        logger.error(f"sys execpt hook\n{t}")

    sys.excepthook = trap_exc_during_debug
    try:
        led_db_init("./data/led_sample.sqlite")
        app = QApplication(sys.argv)
        window = MainWindow()
        window.show()
        app.setWindowIcon(QIcon(ICON_PATH))
        app.exec_()
    except Exception:
        logger.error(f"exception in main loop \n{stackprinter.format()}")
