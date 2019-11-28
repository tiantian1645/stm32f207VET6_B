import queue
import sys
import time
import loguru
import stackprinter
from collections import namedtuple
from bytes_helper import bytesPuttyPrint
from dc201_pack import DC201_PACK

# from PySide2.QtCore import QObject, QRunnable, Signal as pyqtSignal, Slot as pyqtSlot
from PyQt5.QtCore import QObject, QRunnable, pyqtSignal, pyqtSlot

HenjiConf = namedtuple("HenjiConf", "shitsumon henji")
logger = loguru.logger.bind(name="serial_worker")


class SeialWorkerSignals(QObject):
    owari = pyqtSignal()
    finished = pyqtSignal()
    error = pyqtSignal(tuple)
    result = pyqtSignal(object)
    serial_statistic = pyqtSignal(object)


HENJI_TABLE = (
    HenjiConf(shitsumon=0xD9, henji=(0xD4,)),
    HenjiConf(shitsumon=0x0F, henji=(0xFA,)),
    HenjiConf(shitsumon=0xFC, henji=(0xFB,)),
    HenjiConf(shitsumon=0xDE, henji=(0xDE,)),
)


class SerialRecvWorker(QRunnable):
    def __init__(self, serial, henji_queue, logger=loguru.logger):
        super(SerialRecvWorker, self).__init__()
        self.serial = serial
        self.logger = logger
        self.henji_queue = henji_queue
        self.need_henji = (0xAA,)
        self.dd = DC201_PACK()
        self.signals = SeialWorkerSignals()
        self.signals.owari.connect(self.stoptask)
        # self.signals.henji.connect(self.setNeedHenji)
        self.stop = False

    def stoptask(self):
        self.stop = True

    def _str_Henji(self):
        logger.debug(f"self.need_henji {self.need_henji}")
        if len(self.need_henji) > 0:
            return ", ".join(f"0x{i:02X}" for i in self.need_henji)
        else:
            logger.error(f"self.need_henji is empty | {self.need_henji}")
            return "(, )"

    def _find_Henji(self, write_data):
        for ht in HENJI_TABLE:
            if ht.shitsumon == write_data[5]:
                self.need_henji = ht.henji
                return
        self.need_henji = (0xAA,)

    def setNeedHenji(self, write_data):
        self._find_Henji(write_data)
        self.temp_wrote = write_data
        logger.info(f"response setNeedHenji | write cmd 0x{write_data[5]:02X} | {self._str_Henji()}")

    @pyqtSlot()
    def run(self):
        try:
            start_time = time.time()
            recv_buffer = b""
            while True:
                # check stop
                if self.stop:
                    break
                # check recv task
                recv_data = self.serial.read(2048)
                if len(recv_data) <= 0:
                    continue

                self.logger.debug(f"get raw bytes | {bytesPuttyPrint(recv_data)}")
                self.signals.serial_statistic.emit(("r", recv_data))

                recv_buffer += recv_data
                if len(recv_buffer) < 7:
                    continue
                info = None
                for info in self.dd.iterIntactPack(recv_buffer):
                    if info.is_head and info.is_crc and len(info.content) >= 7:
                        ack = info.content[3]
                        fun_code = info.content[5]
                        if fun_code != 0xAA:
                            write_data = self.dd.buildPack(0x13, 0xFF - ack, 0xAA, (ack,))
                            self.serial.write(write_data)
                            self.signals.serial_statistic.emit(("w", write_data))
                            self.logger.info(f"reply ack pack | {bytesPuttyPrint(write_data)} --> {info.text}")
                            self.signals.result.emit(info)
                        if fun_code in self.need_henji:
                            logger.info(f"put henji | self.need_henji {self._str_Henji()} | fun_code 0x{fun_code:02X} | write cmd 0x{self.temp_wrote[5]:02X}")
                            self.henji_queue.put(info)
                        elif fun_code not in (0xAA, 0xA0, 0xEE, 0xD0):
                            logger.error(f"no put to henji | self.need_henji {self._str_Henji()} | info {info.text}")
                if info is not None:
                    if info.is_head and info.is_crc and info.is_tail:
                        recv_buffer = b""
                    else:
                        recv_buffer = info.content
        except Exception:
            exctype, value = sys.exc_info()[:2]
            trace_back_text = stackprinter.format()
            logger.error(f"serial recv worker run excpetion \n{trace_back_text}")
            self.signals.error.emit((exctype, value, trace_back_text))
        finally:
            logger.info(f"serial recv worker total run | {time.time() - start_time:.2f} S")
            self.signals.finished.emit()


class SerialSendWorker(QRunnable):
    def __init__(self, serial, task_queue, henji_queue, recv_worker, logger=loguru.logger):
        super(SerialSendWorker, self).__init__()
        self.serial = serial
        self.logger = logger
        self.task_queue = task_queue
        self.henji_queue = henji_queue
        self.dd = DC201_PACK()
        self.signals = SeialWorkerSignals()
        self.signals.owari.connect(self.stopTask)
        self.stop = False
        self.recv_worker = recv_worker

    def stopTask(self):
        self.stop = True
        self.henji_queue.put(None)

    def getWriteData(self, timeout=0.01):
        try:
            data = self.task_queue.get(timeout=timeout)
        except queue.Empty:
            return None
        else:
            return data

    def waitHenji(self, write_data, timeout=2):
        while True:
            try:
                self.henji_queue.get_nowait()
            except (queue.Empty, Exception):
                break
        self.recv_worker.setNeedHenji(write_data)
        logger.info(f"invoke set henji | write_data cmd 0x{write_data[5]:02X}")

        if write_data[5] == 0xFC and write_data[6] == 0x00:
            return (True, write_data, None)

        try:
            info = self.henji_queue.get(timeout=timeout)
        except queue.Empty:
            return (False, write_data, None)
        else:
            if info is None:
                return (False, write_data, None)

        if write_data[5] == 0xD9 and info.content[5] == 0xD4:
            return (True, write_data, info)
        elif write_data[5] == 0xFC and info.content[5] == 0xFB:
            if info.content[6] == 0x00:
                return (True, write_data, info)
            else:
                return (False, write_data, info)
        elif write_data[5] == 0x0F and info.content[5] == 0xFA:
            return (True, write_data, info)
        elif write_data[5] == 0xDE and info.content[5] == 0xDE:
            if info.content[6] == 0x00:
                return (True, write_data, info)
            else:
                return (False, write_data, info)
        return (False, write_data, info)

    @pyqtSlot()
    def run(self):
        try:
            start_time = time.time()
            while True:
                # check stop
                if self.stop:
                    break
                # check write task
                write_data = self.getWriteData(0.01)
                if write_data is None:
                    continue
                logger.debug(f"serial write data | {bytesPuttyPrint(write_data)}")
                self.serial.write(write_data)
                self.signals.serial_statistic.emit(("w", write_data))
                if write_data[5] in (0x0F, 0xDD, 0xFC):
                    timeout = 8
                else:
                    timeout = 2
                wait_result = self.waitHenji(write_data, timeout)
                self.signals.result.emit(wait_result)
        except Exception:
            exctype, value = sys.exc_info()[:2]
            trace_back_text = stackprinter.format()
            logger.error(f"serial send worker run excpetion \n{trace_back_text}")
            self.signals.error.emit((exctype, value, trace_back_text))
        finally:
            logger.info(f"serial send worker total run | {time.time() - start_time:.2f} S")
            self.signals.finished.emit()
