import os
import queue
import random
import threading
import time
from collections import namedtuple

import loguru
import serial
import stackprinter

import dc201_pack
from bytes_helper import bytesPuttyPrint

logger = loguru.logger
logger.add(r"E:\pylog\stm32f207_serial.log", enqueue=True, rotation="10 MB")

TEST_BIN_PATH = r"C:\Users\Administrator\STM32CubeIDE\workspace_1.0.2\stm32F207VET6_Bootloader_APP\Debug\stm32F207VET6_Bootloader_APP.bin"
REAL_BIN_PATH = r"C:\Users\Administrator\STM32CubeIDE\workspace_1.0.2\stm32f207VET6_B\Debug\stm32f207VET6_B.bin"

dd = dc201_pack.DC201_PACK()

BarcodeInfo = namedtuple("BarcodeInfo", "name data")
BARCODE_LIST = (
    BarcodeInfo("HB", "1415190701"),
    BarcodeInfo("AMY", "1411190601"),
    BarcodeInfo("UA", "1413190703"),
    BarcodeInfo("HB", "1415190502"),
    BarcodeInfo("AMY", "1411190703"),
    BarcodeInfo("CREA", "1418190602"),
)


def dep32(n):
    return ((n >> 24) & 0xFF, (n >> 16) & 0xFF, (n >> 8) & 0xFF, n & 0xFF)


def test_conf(fn_a, fn_g, fn_c):
    """
    test_conf(lambda x: x % 3 + 1, lambda x: x % 3 + 1, lambda x: x + 1)
    """
    result = []
    for i in range(6):
        result.append(fn_a(i))
        result.append(fn_g(i))
        result.append(fn_c(i))
    return result


def write_pack_generator(pack_index, cmd_type, addr, num):
    pack = dd.buildPack(
        0x13,
        pack_index,
        cmd_type,
        # (*(addr >> 16, (addr >> 8) & 0xFF, addr & 0xFF), *(num >> 16, (num >> 8) & 0xFF, num & 0xFF), *((addr + i) & 0xFF for i in range(num))),
        (*(addr >> 16, (addr >> 8) & 0xFF, addr & 0xFF), *(num >> 16, (num >> 8) & 0xFF, num & 0xFF), *(random.randint(0, 255) for i in range(num))),
    )
    return pack


def read_pack_generator(pack_index, cmd_type, addr, num):
    pack = dd.buildPack(0x13, pack_index, cmd_type, (*(addr >> 16, (addr >> 8) & 0xFF, addr & 0xFF), *(num >> 16, (num >> 8) & 0xFF, num & 0xFF)))
    return pack


def iter_test_bin(file_path=TEST_BIN_PATH, chunk_size=224):
    try:
        with open(file_path, "rb") as f:
            while True:
                data = f.read(chunk_size)
                if not data:
                    break
                yield (data)
    except Exception:
        logger.error("read file error \n{}".format(stackprinter.format()))


def write_firmware_pack():
    total = os.path.getsize(TEST_BIN_PATH)
    chunk_size = 224
    addr = 0
    pack_index = 1
    for data in iter_test_bin(TEST_BIN_PATH, chunk_size):
        pack = dd.buildPack(0x13, pack_index, 0xDC, (*dep32(total)[::-1], *dep32(addr)[::-1], len(data), *(i for i in data)))
        addr += chunk_size
        pack_index += 1
        yield pack


def iter_test_bin_FC(file_path=REAL_BIN_PATH, chunk_size=1024):
    try:
        with open(file_path, "rb") as f:
            while True:
                data = f.read(chunk_size)
                if len(data) > 0 and len(data) < chunk_size:
                    if len(data) > 512:
                        chunk_size = 1024
                    elif chunk_size > 256:
                        chunk_size = 512
                    else:
                        chunk_size = 256
                    data = data.ljust(chunk_size, b"\x00")
                if not data:
                    break
                yield (data)
    except Exception:
        logger.error("read file error \n{}".format(stackprinter.format()))


def write_firmware_pack_FC(chunk_size=1024):
    # total = os.path.getsize(REAL_BIN_PATH)
    addr = 0
    pack_index = 1
    for data in iter_test_bin_FC(REAL_BIN_PATH, chunk_size):
        pack = dd.buildPack(0x13, pack_index, 0xFC, (len(data) // 256, *(i for i in data)))
        addr += chunk_size
        pack_index += 1
        yield pack
    yield dd.buildPack(0x13, pack_index, 0xFC, (0,))


def decode_pack(info):
    raw_byte = info.content
    cmd_type = raw_byte[5]
    if cmd_type == 0xB2:
        channel = raw_byte[6]
        length = raw_byte[7]
        if length == 0:
            payload = None
        else:
            try:
                payload = raw_byte[8 : 8 + length].decode("utf-8")
                for bi in BARCODE_LIST:
                    if payload == bi.data:
                        payload = "{0.name} --> {0.data}".format(bi)
            except UnicodeDecodeError:
                payload = bytesPuttyPrint(raw_byte[8 : 8 + length])
            except Exception:
                payload = stackprinter.format()
        logger.success("recv scan data | channel {} | length {:2d} | payload {}".format(channel, length, payload))
        return
    if cmd_type == 0xA0:
        temp_btm = int.from_bytes(raw_byte[6:8], byteorder="little") / 100
        temp_top = int.from_bytes(raw_byte[8:10], byteorder="little") / 100
        logger.success("recv temp data | buttom {:.2f} | top {:.2f}".format(temp_btm, temp_top))
        return
    logger.success("recv pack | {}".format(info.text))
    return


def read_task(ser, write_queue, firm_queue, delay=0.5):
    recv_buffer = b""
    while True:
        iw = ser.in_waiting
        if iw <= 0:
            time.sleep(0.001)
            continue

        recv_data = ser.read(iw)
        logger.debug("get raw bytes | {}".format(bytesPuttyPrint(recv_data)))
        recv_buffer += recv_data
        if len(recv_buffer) < 7:
            continue

        for info in dd.iterIntactPack(recv_buffer):
            recv_pack = info.content
            if info.is_head and info.is_crc and len(recv_pack) >= 7:
                decode_pack(info)
                ack = recv_pack[3]
                fun_code = recv_pack[5]
                if fun_code != 0xAA:
                    time.sleep(delay)
                    send_data = dd.buildPack(0x13, 0xFF - ack, 0xAA, (ack,))
                    ser.write(send_data)
                    logger.warning("send pack | {}".format(bytesPuttyPrint(send_data)))
                    if fun_code in (0xDC, 0xFB):
                        firm_queue.put(recv_pack[6])
        if info.is_head and info.is_crc:
            recv_buffer = b""
        else:
            recv_buffer = info.content


def send_task(ser, write_queue):
    while True:
        try:
            send_data = write_queue.get()
            if isinstance(send_data, bytes):
                logger.warning("send pack | {}".format(bytesPuttyPrint(send_data)))
                ser.write(send_data)
            else:
                logger.error("send data type error | {} | {}".format(type(send_data), send_data))
        except queue.Empty:
            logger.debug("empty send queue")
        except Exception:
            logger.error("send queue exception \n{}".format(stackprinter.format()))
            continue


ser = serial.Serial(port=None, baudrate=115200, timeout=1)
ser.port = "COM9"
ser.open()
write_queue = queue.Queue()
firm_queue = queue.Queue()

rt = threading.Thread(target=read_task, args=(ser, write_queue, firm_queue, 0.1))
st = threading.Thread(target=send_task, args=(ser, write_queue))
rt.setDaemon(True)
st.setDaemon(True)
rt.start()
st.start()


while True:
    try:
        firm_queue.get_nowait()
    except queue.Empty:
        break
start = time.time()
real_size = 0
for pack in write_firmware_pack_FC(chunk_size=1024):
    write_queue.put(pack)
    if len(pack) > 8:
        delta = len(pack) - 8
        logger.info("write pack addr 0x{:08X} ~ 0x{:08X}".format(real_size, real_size + delta))
        real_size += delta
    try:
        result = firm_queue.get(timeout=5)
        if result != 0:
            if result == 2:
                logger.success("get all recv")
            else:
                logger.error("get error recv")
            break
    except queue.Empty:
        logger.error("get recv timeout")
        break
    except Exception:
        logger.error("other error\n{}".format(stackprinter.format()))
        break
total_size = os.path.getsize(REAL_BIN_PATH)
logger.info(
    "finish loop file | complete {:.2%} | {} / {} Byte | in {:.2f}S".format(real_size / total_size, real_size, total_size, time.time() - start)
)

# C0F7830B3EA77504D202F2DF3407CAAD
# A24C047DD2E7E72D02E5C397A2346EFE

for pack in write_firmware_pack():
    write_queue.put(pack)
    try:
        result = firm_queue.get(timeout=2)
        if result != 0:
            if result == 4:
                logger.success("get all recv")
            else:
                logger.error("get error recv")
            break
    except queue.Empty:
        logger.error("get recv timeout")
        break


write_queue.put(dd.buildPack(0x13, 0, 0x01))
write_queue.put(dd.buildPack(0x13, 0, 0x03, test_conf(lambda x: x % 3 + 1, lambda x: x % 3 + 1, lambda x: 10)))
write_queue.put(dd.buildPack(0x13, 0, 0x04))
write_queue.put(dd.buildPack(0x13, 0, 0x05))
write_queue.put(dd.buildPack(0x13, 0, 0x06))


def serial_test(*args, **kwargs):
    try:
        if not ser.isOpen():
            ser.open()
        send_pack = dd.buildPack(*args, **kwargs)
        logger.warning("send pack | {}".format(bytesPuttyPrint(send_pack)))
        ser.write(send_pack)
        cnt = 0
        while True:
            recv_pack = ser.read(3)
            if len(recv_pack) < 3:
                if cnt == 0:
                    logger.error("recv pack error | {}".format(bytesPuttyPrint(recv_pack)))
                break
            second_read_num = recv_pack[-1] + 1
            recv_pack += ser.read(second_read_num)
            if len(recv_pack) < 3 + second_read_num:
                logger.error("recv second pack error | {}".format(bytesPuttyPrint(recv_pack)))
                break
            logger.success("recv pack | {}".format(bytesPuttyPrint(recv_pack)))
            if recv_pack[5] != 0xAA:
                ack_pack = dd.buildPack(0x13, random.randint(0, 255), 0xAA, (recv_pack[3],))
                ser.write(ack_pack)
                logger.warning("send pack | {}".format(bytesPuttyPrint(ack_pack)))
            cnt += 1
    except Exception:
        logger.error("exception in serial test\n{}".format(stackprinter.format()))
        ser.close()


def storge_write_test():
    w_result = b""
    addr = 0
    pack_index = 0
    max_addr = 0x800000
    # max_addr = 0x010000

    if not ser.isOpen():
        ser.open()
    ser.read(2048)
    while addr < max_addr:
        if addr + 0xE0 > max_addr:
            num = max_addr - addr
        else:
            num = 0xE0
        logger.debug("0x{:06X} ~ 0x{:06X}".format(addr, addr + num - 1))
        pack = write_pack_generator(pack_index & 0xFF, 0xD7, addr, num)
        w_result += pack[12:-1]
        try:
            logger.debug("send pack | {}".format(bytesPuttyPrint(pack)))
            ser.write(pack)
            recv_ack = ser.read(8)
            if len(recv_ack) == 0:
                logger.error("recv ack None stop")
                time.sleep(3)
                ser.read(2048)
                continue
            logger.success("recv ack | {}".format(bytesPuttyPrint(recv_ack)))
            recv_data = ser.read(8)
            if len(recv_ack) == 0:
                logger.error("recv data None stop")
                time.sleep(3)
                ser.read(2048)
                continue
            logger.success("recv data | {}".format(bytesPuttyPrint(recv_data)))
            last_packindex = recv_data[3]
            pack_index = last_packindex + 1
            ack_pack = dd.buildPack(0x13, pack_index & 0xFF, 0xAA, (last_packindex,))
            logger.debug("send ack | {}".format(bytesPuttyPrint(ack_pack)))
            ser.write(ack_pack)
            pack_index += 1
        except Exception:
            logger.debug("error {}".format(stackprinter.format()))
            break
        addr += 0xE0
    ser.close()
    return w_result


addr = 0
pack_index = 0
max_addr = 0x010000
r_result = b""

if not ser.isOpen():
    ser.open()
while addr < max_addr:
    if addr + 0xE0 > max_addr:
        num = max_addr - addr
    else:
        num = 0xE0
    logger.debug("0x{:06X} ~ 0x{:06X}".format(addr, addr + num - 1))
    pack = read_pack_generator(pack_index & 0xFF, 0xD6, addr, num)
    try:
        logger.debug("send pack | {}".format(bytesPuttyPrint(pack)))
        ser.write(pack)
        recv_ack = ser.read(8)
        if len(recv_ack) == 0:
            logger.error("recv ack None stop")
            time.sleep(3)
            ser.read(2048)
            continue
        logger.success("recv ack | {}".format(bytesPuttyPrint(recv_ack)))
        recv_data = ser.read(7 + num)
        if len(recv_ack) == 0:
            logger.error("recv data None stop")
            time.sleep(3)
            ser.read(2048)
            continue
        logger.success("recv data | {}".format(bytesPuttyPrint(recv_data)))
        r_result += recv_data[6:-1]
        last_packindex = recv_data[3]
        pack_index = last_packindex + 1
        ack_pack = dd.buildPack(0x13, pack_index & 0xFF, 0xAA, (last_packindex,))
        logger.debug("send ack | {}".format(bytesPuttyPrint(ack_pack)))
        ser.write(ack_pack)
        pack_index += 1
    except Exception:
        logger.debug("error {}".format(stackprinter.format()))
        break
    addr += 0xE0
ser.close()
