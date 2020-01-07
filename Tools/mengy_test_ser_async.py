import asyncio
import codecs
import struct
from collections import namedtuple
from decimal import Decimal

import serial_asyncio
from loguru import logger
import stackprinter
import click

try:
    import libscrc

    moubus = libscrc.modbus
except ModuleNotFoundError:
    import functools
    import re

    INITIAL_MODBUS = 0xFFFF
    INITIAL_DF1 = 0x0000

    def str2Bytes(s: str):
        s_hex = re.compile("[^a-fA-F0-9]").sub("", s)
        if len(s_hex) % 2 == 1:
            s_hex = f"{s_hex[:-1]}0{s_hex[-1]}"
        return bytes.fromhex(s_hex)

    class CRC_PACK(object):
        """docstring for CRC_PACK"""

        def __init__(self,):
            self.table = CRC_PACK.init_table()

        @classmethod
        def init_table(cls):
            # Initialize the CRC-16 table,
            #   build a 256-entry list, then convert to read-only tuple
            lst = []
            for i in range(256):
                data = i << 1
                crc = 0
                for j in range(8, 0, -1):
                    data >>= 1
                    if (data ^ crc) & 0x0001:
                        crc = (crc >> 1) ^ 0xA001
                    else:
                        crc >>= 1
                lst.append(crc)
            table = tuple(lst)
            return table

        @functools.lru_cache(maxsize=1024)
        def calc_b_Str(self, b_st, crc=INITIAL_MODBUS):
            """Given a bunary string and starting CRC, Calc a final CRC-16 """
            for ch in b_st:
                crc = (crc >> 8) ^ self.table[(crc ^ ch) & 0xFF]
            return crc

    CC = CRC_PACK()

    modbus = CC.calc_b_Str


queue_l = asyncio.Queue()
queue_h = asyncio.Queue()

# (起始校准点 (实际温度 输出温度), 终止校准点 (实际温度 输出温度))
TempLineCorrect = namedtuple("TempLineCorrect", "start_point stop_point")
# TempLineCorrect((10, 10), (15, 40)) 实际10度输出10度 实际15度输出40度
TEMP_LINE_CORRECTS = (
    TempLineCorrect((10, 10), (15, 40)),
    TempLineCorrect((15, 40), (20, 70)),
    TempLineCorrect((20, 40), (35, 70)),
    TempLineCorrect((35, 35), (40, 80)),
)


def bytesPuttyPrint(b):
    """
    b'\x01\x02' -> '01 02'
    """
    st = codecs.encode(b, "hex").decode("utf-8").upper()
    it = iter(st)
    result_str = " ".join(a + b for a, b in zip(it, it))
    return result_str


def check_whole_pack(raw_bytes):
    if len(raw_bytes) < 7:
        return False
    if raw_bytes[1] != 0x03:
        logger.error(f"error pack for fun code | {raw_bytes[1]}")
        return False
    if raw_bytes[2] + 5 != len(raw_bytes):
        # logger.error(f"error pack for paclk length | {raw_bytes[2] + 5} -> {len(raw_bytes)}")
        return False
    crc = modbus(bytes(raw_bytes))
    if crc != 0:
        logger.error(f"error pack for moubus crc check failed | {bytesPuttyPrint(raw_bytes)} -> {crc}")
        return False
    return True


def build_temp_100_pack(temp_datas, addr=1):
    logger.debug(f"build temp datas | {temp_datas}")
    data = struct.pack("BBB", addr, 0x03, len(temp_datas) * 2)
    for temp_data in temp_datas:
        data += struct.pack(">H", int(temp_data * 100))
    data += struct.pack("H", modbus(data))
    # logger.debug(f"build temp pack | {temp_datas} -> {bytesPuttyPrint(data)}")
    return data


def decode_temp_100_pack(raw_bytes):
    result = [struct.unpack(">H", raw_bytes[3 + i * 2 : 5 + i * 2])[0] / 100 for i in range(raw_bytes[2] // 2)]
    return result


def example_func(temp):
    new_temp = temp * 2
    return new_temp


def lin_equ(l1, l2):
    """Line encoded as l=(x,y)."""
    m = Decimal((l2[1] - l1[1])) / Decimal(l2[0] - l1[0])
    c = l2[1] - (m * l2[0])
    return m, c


def example_func_line(temp):
    """
    线性映射 从 TEMP_LINE_CORRECTS 遍历配置 找到 实际温度 temp 所在的区间配置 进行映射返回
    否测返回原值
    temp 实际温度
    """
    for temp_conf in TEMP_LINE_CORRECTS:
        if temp_conf.start_point[0] <= temp <= temp_conf.stop_point[0]:
            m, c = lin_equ(temp_conf.start_point, temp_conf.stop_point)
            return m * Decimal(temp) + c
    return temp


def correct(raw_bytes, func):
    raw_temps = decode_temp_100_pack(raw_bytes)
    # logger.debug(f"raw temp is | {raw_temps}")
    new_temps = tuple(map(func, raw_temps))
    pack = build_temp_100_pack(new_temps)
    return pack


def correct_local(raw_bytes, func):
    for i in range(raw_bytes[2] // 2):
        old_temp = struct.unpack(">H", raw_bytes[3 + i * 2 : 5 + i * 2])[0] / 100
        new_temp = func(old_temp)
        data_temp = int(new_temp * 100)
        raw_bytes[3 + i * 2 : 5 + i * 2] = struct.pack(">H", data_temp % 65536)
    raw_bytes[-2:] = struct.pack("H", modbus(bytes(raw_bytes[:-2])))
    return bytes(raw_bytes)


# test
# decode_temp_100_pack(correct(build_temp_100_pack((25, 26, 30, 31, 32, 33, 34, 35)), example_func))


class FCOM_Real(asyncio.Protocol):
    def connection_made(self, transport):
        """Store the serial transport and prepare to receive data.
        """
        self.data = bytearray()
        self.transport = transport
        self.queue_l = queue_l
        self.queue_h = queue_h
        logger.debug("FCOM_Real connection created")
        asyncio.ensure_future(self.send())
        logger.debug("FCOM_Real.send() scheduled")

    def data_received(self, data):
        """Store characters until a newline is received.
        """
        # logger.debug(f"FCOM_Real <== {bytesPuttyPrint(data)}")
        self.data += data
        if check_whole_pack(self.data):
            asyncio.ensure_future(self.queue_h.put(correct_local(self.data, example_func_line)))
            self.data.clear()
        elif len(self.data) >= 21:
            self.data.clear()

    def connection_lost(self, exc):
        logger.debug("FCOM_Real closed")

    async def send(self):
        while True:
            data = await self.queue_l.get()
            self.transport.serial.write(data)
            logger.debug(f"FCOM_Real ==> {bytesPuttyPrint(data)}")


class FCOM_Virt(asyncio.Protocol):
    def connection_made(self, transport):
        """Store the serial transport and schedule the task to send data.
        """
        self.transport = transport
        self.queue_l = queue_l
        self.queue_h = queue_h
        logger.debug("FCOM_Virt connection created")
        asyncio.ensure_future(self.send())
        logger.debug("FCOM_Virt.send() scheduled")

    def connection_lost(self, exc):
        logger.debug("Writer closed")

    def data_received(self, data):
        """Store characters until a newline is received.
        """
        # logger.debug(f"FCOM_Virt <== {bytesPuttyPrint(data)}")
        asyncio.ensure_future(self.queue_l.put(data))

    async def send(self):
        """Send four newline-terminated messages, one byte at a time.
        """
        while True:
            data = await self.queue_h.get()
            self.transport.serial.write(data)
            # logger.debug(f"FCOM_Virt ==> {bytesPuttyPrint(data)}")


@click.command()
@click.option("--port_real", "-pr", help="实际物理端口号 例如 COM6", default="COM6")
@click.option("--port_virt", "-pv", help="虚拟物理端口对 其中之一端口号 例如 COM14", default="COM14")
def main(port_real, port_virt):
    loop = asyncio.get_event_loop()

    reader = serial_asyncio.create_serial_connection(loop, FCOM_Real, port_real, baudrate=9600)
    writer = serial_asyncio.create_serial_connection(loop, FCOM_Virt, port_virt, baudrate=9600)

    asyncio.ensure_future(reader)
    logger.debug("Reader scheduled")

    asyncio.ensure_future(writer)
    logger.debug("Writer scheduled")

    # loop.call_later(10, loop.stop)
    loop.run_forever()
    logger.debug("Done")


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        logger.info("KeyboardInterrupt")
    except Exception:
        logger.error(f"exception in main loop\n{stackprinter.format()}")
