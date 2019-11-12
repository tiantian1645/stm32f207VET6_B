import queue
import sys
import time
import loguru
import stackprinter

from bytes_helper import bytesPuttyPrint
from dc201_pack import DC201_PACK

from PySide2.QtCore import QObject, QRunnable, Signal as pyqtSignal, Slot as pyqtSlot

logger = loguru.logger.bind(name="serial_worker")


class SeialWorkerSignals(QObject):
    owari = pyqtSignal()
    henji = pyqtSignal(object)
    finished = pyqtSignal()
    error = pyqtSignal(tuple)
    result = pyqtSignal(object)
    serial_statistic = pyqtSignal(object)


class SerialRecvWorker(QRunnable):
    def __init__(self, serial, henji_queue, logger=loguru.logger):
        super(SerialRecvWorker, self).__init__()
        self.serial = serial
        self.logger = logger
        self.henji_queue = henji_queue
        self.need_henji = ()
        self.dd = DC201_PACK()
        self.signals = SeialWorkerSignals()
        self.signals.owari.connect(self.stoptask)
        self.signals.henji.connect(self.setNeedHenji)
        self.stop = False

    def stoptask(self):
        self.stop = True

    @pyqtSlot()
    def setNeedHenji(self, write_data):
        if write_data[5] == 0xD9:
            self.need_henji = (0xD4,)
        elif write_data[5] == 0x0F:
            self.need_henji = (0xFA,)
        elif write_data[5] == 0xFC:
            self.need_henji = (0xAA, 0xB5)
        else:
            self.need_henji = (0xAA,)
        self.temp_wrote = write_data
        logger.info("response setNeedHenji | {} | {}".format(bytesPuttyPrint(write_data), self.need_henji))

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

                self.logger.debug("get raw bytes | {}".format(bytesPuttyPrint(recv_data)))
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
                            self.logger.warning("reply ack pack | {} --> {}".format(bytesPuttyPrint(write_data), info.text))
                            self.signals.result.emit(info)
                        if fun_code in self.need_henji:
                            logger.info("put henji | {} | {} | {}".format(self.need_henji, fun_code, bytesPuttyPrint(self.temp_wrote)))
                            self.henji_queue.put(info)
                if info is not None:
                    if info.is_head and info.is_crc and info.is_tail:
                        recv_buffer = b""
                    else:
                        recv_buffer = info.content
        except Exception:
            exctype, value = sys.exc_info()[:2]
            trace_back_text = stackprinter.format()
            logger.error("serial recv worker run excpetion \n{}".format(trace_back_text))
            self.signals.error.emit((exctype, value, trace_back_text))
        finally:
            logger.info("serial recv worker total run | {:.2f} S".format(time.time() - start_time))
            self.signals.finished.emit()


class SerialSendWorker(QRunnable):
    def __init__(self, serial, task_queue, henji_queue, logger=loguru.logger):
        super(SerialSendWorker, self).__init__()
        self.serial = serial
        self.logger = logger
        self.task_queue = task_queue
        self.henji_queue = henji_queue
        self.dd = DC201_PACK()
        self.signals = SeialWorkerSignals()
        self.signals.owari.connect(self.stopTask)
        self.stop = False

    def stopTask(self):
        self.stop = True

    def getWriteData(self, timeout=0.01):
        try:
            data = self.task_queue.get(timeout=timeout)
        except queue.Empty:
            return None
        else:
            return data

    def waitHenji(self, write_data, timeout=2):
        self.signals.henji.emit(write_data)
        logger.info("invoke set henji | {}".format(bytesPuttyPrint(write_data)))
        if write_data[5] == 0x0F:
            time.sleep(3)
        try:
            info = self.henji_queue.get(timeout=timeout)
        except queue.Empty:
            return (False, write_data, None)
        if write_data[5] == 0xD9 and info.content[5] == 0xD4:
            return (True, write_data, info)
        elif write_data[5] == 0xFC and info.content[5] == 0xAA:
            return (True, write_data, info)
        elif write_data[5] == 0x0F and info.content[5] == 0xFA:
            return (True, write_data, info)
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
                logger.debug("serial write data | {}".format(bytesPuttyPrint(write_data)[:80]))
                self.serial.write(write_data)
                self.signals.serial_statistic.emit(("w", write_data))
                wait_result = self.waitHenji(write_data)
                self.signals.result.emit(wait_result)
        except Exception:
            exctype, value = sys.exc_info()[:2]
            trace_back_text = stackprinter.format()
            logger.error("serial send worker run excpetion \n{}".format(trace_back_text))
            self.signals.error.emit((exctype, value, trace_back_text))
        finally:
            logger.info("serial send worker total run | {:.2f} S".format(time.time() - start_time))
            self.signals.finished.emit()
