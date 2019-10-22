from bytes_helper import bytesPuttyPrint, str2Bytes
import dc201_pack
import serial
import loguru
import stackprinter
import random
import time
import queue
import threading


logger = loguru.logger
logger.add(r"E:\pylog\stm32f207_serial.log")

dd = dc201_pack.DC201_PACK()


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


def read_task(ser, write_queue, delay=0.5):
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
                logger.success("recv pack | {}".format(info.text))
                ack = recv_pack[3]
                fun_code = recv_pack[5]
                if fun_code != 0xAA:
                    time.sleep(delay)
                    send_data = dd.buildPack(0x13, 0xFF - ack, 0xAA, (ack,))
                    ser.write(send_data)
                    logger.warning("send pack | {}".format(bytesPuttyPrint(send_data)))
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
ser.port = "COM3"
ser.open()
write_queue = queue.Queue()

rt = threading.Thread(target=read_task, args=(ser, write_queue, 0.1))
st = threading.Thread(target=send_task, args=(ser, write_queue))
rt.setDaemon(True)
st.setDaemon(True)
rt.start()
st.start()


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
