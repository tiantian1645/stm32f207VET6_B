import serial
from loguru import logger
from threading import Thread
from queue import Queue, Empty
from bytes_helper import bytesPuttyPrint

queue_1 = Queue()
queue_2 = Queue()

ser_1 = serial.Serial("COM14", baudrate=115200, timeout=0.1)
ser_2 = serial.Serial("COM10", baudrate=115200, timeout=0.1)


def task(ser, queue_in, queue_out):
    while True:
        if ser.in_waiting:
            recv_data = ser.read(ser.in_waiting)
            queue_in.put(recv_data)
            logger.debug(f"{ser.port:5s} ==> {bytesPuttyPrint(recv_data)}")
        try:
            trasn_data = queue_out.get_nowait()
        except Empty:
            pass
        else:
            ser.write(trasn_data)
            logger.debug(f"{ser.port:5s} <== {bytesPuttyPrint(trasn_data)}")


t1 = Thread(target=task, args=(ser_1, queue_1, queue_2), daemon=True)
t2 = Thread(target=task, args=(ser_2, queue_2, queue_1), daemon=True)

t1.start()
t2.start()
t1.join()
t2.join()
