from bytes_helper import bytesPuttyPrint
import dc201_pack
import serial
import loguru
import stackprinter


def dep32(n):
    return ((n >> 24) & 0xFF, (n >> 16) & 0xFF, (n >> 8) & 0xFF, n & 0xFF)


logger = loguru.logger
logger.add(r"E:\pylog\stm32f207_serial.log")

dd = dc201_pack.DC201_PACK()

ser = serial.Serial(port=None, baudrate=115200, timeout=1)
ser.port = "COM13"


def serial_test(*args, **kwargs):
    try:
        if not ser.isOpen():
            ser.open()
        send_pack = dd.buildPack(*args, **kwargs)
        logger.info("send pack | {}".format(bytesPuttyPrint(send_pack)))
        ser.write(send_pack)
        recv_pack = ser.read(3)
        if recv_pack:
            if len(recv_pack) == 3:
                recv_pack += ser.read(recv_pack[-1] + 2)
            logger.info("recv pack | {}".format(bytesPuttyPrint(recv_pack)))
        else:
            logger.warning("recv pack | None")
    except Exception:
        logger.error("exception in serial test\n{}".format(stackprinter.format()))
    finally:
        ser.close()
