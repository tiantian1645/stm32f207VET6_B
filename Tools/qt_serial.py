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
    finished = pyqtSignal()
    error = pyqtSignal(tuple)
    result = pyqtSignal(object)
    serial_statistic = pyqtSignal(object)


class SerialRecvWorker(QRunnable):
    def __init__(self, serial, task_queue, firm_queue, logger=loguru.logger):
        super(SerialRecvWorker, self).__init__()
        self.serial = serial
        self.logger = logger
        self.task_queue = task_queue
        self.firm_queue = firm_queue
        self.firm_start_flag = False
        self.firm_over_flag = False
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

    def _deal_recv(self, info):
        ack = info.content[3]
        fun_code = info.content[5]
        if fun_code != 0xAA:
            write_data = self.dd.buildPack(0x13, 0xFF - ack, 0xAA, (ack,))
            self.serial.write(write_data)
            self.signals.serial_statistic.emit(("w", write_data))
            self.logger.warning("send pack | {}".format(bytesPuttyPrint(write_data)))
            self.signals.result.emit(info)
            if fun_code == 0xFA:
                self.firm_queue.put(3)
                self.firm_start_flag = True
            if self.firm_start_flag:
                if fun_code == 0xB5:
                    self.firm_queue.put(1)
        else:
            if self.firm_over_flag:
                self.firm_queue.put(2)
            else:
                self.firm_queue.put(0)

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
                    if write_data[5] == 0xFC:
                        if write_data[6] == 0:
                            self.firm_over_flag = True
                    if write_data[5] == 0x0F:
                        self.firm_start_flag = False
                        self.firm_over_flag = False
                    if write_data[5] == 0xD9:
                        time.sleep(0.5)
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
                    if info.is_head and info.is_crc and len(info.content) >= 7:
                        self._deal_recv(info)
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
