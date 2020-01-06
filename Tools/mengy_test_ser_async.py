import asyncio
import serial_asyncio
from loguru import logger
from bytes_helper import bytesPuttyPrint
from libscrc import modbus
import struct

queue_l = asyncio.Queue()
queue_h = asyncio.Queue()


def build_temp_100_pack(temp_datas, addr=1):
    data = struct.pack("BBB", addr, 0x03, len(temp_datas))
    for temp_data in temp_datas:
        data += struct.pack(">H", int(temp_data * 100))
    data += struct.pack("H", modbus(data))
    logger.debug(f"build temp pack | {temp_datas} -> {bytesPuttyPrint(data)}")
    return data


def decode_temp_100_pack(raw_bytes):
    crc = modbus(raw_bytes)
    if crc != 0:
        logger.error(f"error pack for moubus crc check failed | {bytesPuttyPrint(raw_bytes)} -> {crc}")
        return None
    # 01 03 08 09 C4 0A 28 0B B8 0C 1C 0C 80 0C E4 0D 48 0D AC B0 EF
    if raw_bytes[1] != 0x03:
        logger.error(f"error pack for fun code | {raw_bytes[1]}")
        return None
    if raw_bytes[2] * 2 + 5 != len(raw_bytes):
        logger.error(f"error pack for paclk length | {raw_bytes[2] * 2 + 5} -> {len(raw_bytes)}")
        return None
    result = [struct.unpack(">H", raw_bytes[3 + i * 2 : 5 + i * 2])[0] / 100 for i in range(raw_bytes[2])]
    return result


def example_func(temp):
    return temp * 2


def correct(raw_bytes, func):
    raw_temps = decode_temp_100_pack(raw_bytes)
    new_temps = tuple(map(func, raw_temps))
    pack = build_temp_100_pack(new_temps)
    return pack


# test
# decode_temp_100_pack(correct(build_temp_100_pack((25, 26, 30, 31, 32, 33, 34, 35)), example_func))


class FCOM10(asyncio.Protocol):
    def connection_made(self, transport):
        """Store the serial transport and prepare to receive data.
        """
        self.transport = transport
        self.queue_l = queue_l
        self.queue_h = queue_h
        logger.debug("FCOM10 connection created")
        asyncio.ensure_future(self.send())
        logger.debug("FCOM10.send() scheduled")

    def data_received(self, data):
        """Store characters until a newline is received.
        """
        asyncio.ensure_future(self.queue_h.put(data))

    def connection_lost(self, exc):
        logger.debug("FCOM10 closed")

    async def send(self):
        while True:
            data = await self.queue_l.get()
            self.transport.serial.write(data)
            logger.debug(f"FCOM10 ==> {bytesPuttyPrint(data)}")


class FCOM14(asyncio.Protocol):
    def connection_made(self, transport):
        """Store the serial transport and schedule the task to send data.
        """
        self.transport = transport
        self.queue_l = queue_l
        self.queue_h = queue_h
        logger.debug("FCOM14 connection created")
        asyncio.ensure_future(self.send())
        logger.debug("FCOM14.send() scheduled")

    def connection_lost(self, exc):
        logger.debug("Writer closed")

    def data_received(self, data):
        """Store characters until a newline is received.
        """
        logger.debug(f"FCOM14 <== {bytesPuttyPrint(data)}")
        asyncio.ensure_future(self.queue_l.put(data))

    async def send(self):
        """Send four newline-terminated messages, one byte at a time.
        """
        while True:
            data = await self.queue_h.get()
            self.transport.serial.write(data)


loop = asyncio.get_event_loop()
reader = serial_asyncio.create_serial_connection(loop, FCOM10, "COM10", baudrate=115200)
writer = serial_asyncio.create_serial_connection(loop, FCOM14, "COM14", baudrate=115200)
asyncio.ensure_future(reader)
logger.debug("Reader scheduled")
asyncio.ensure_future(writer)
logger.debug("Writer scheduled")
# loop.call_later(10, loop.stop)
loop.run_forever()
logger.debug("Done")
