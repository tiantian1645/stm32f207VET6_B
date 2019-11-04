import loguru
import stackprinter
from bytes_helper import bytesPuttyPrint, crc8, str2Bytes
import struct
from functools import lru_cache
from collections import namedtuple

logger = loguru.logger.bind(name=__name__)
PackInfo = namedtuple("PackInfo", "type is_head is_crc content text")
TestPackInfo = namedtuple("TestPackInfo", "type is_head is_crc content text")

TEST_BIN_PATH = r"C:\Users\Administrator\STM32CubeIDE\workspace_1.0.2\stm32F207VET6_Bootloader_APP\Debug\stm32F207VET6_Bootloader_APP.bin"
REAL_BIN_PATH = r"C:\Users\Administrator\STM32CubeIDE\workspace_1.0.2\stm32f207VET6_B\Debug\stm32f207VET6_B.bin"


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


def write_firmware_pack_FC(dd, file_path=REAL_BIN_PATH, chunk_size=1024):
    # total = os.path.getsize(REAL_BIN_PATH)
    addr = 0
    pack_index = 1
    for data in iter_test_bin_FC(file_path, chunk_size):
        pack = dd.buildPack(0x13, pack_index, 0xFC, (len(data) // 256, *(i for i in data)))
        addr += chunk_size
        pack_index += 1
        yield pack
    yield dd.buildPack(0x13, pack_index, 0xFC, (0,))


class DC201_PACK:
    def __init__(self):
        self.pack_head = struct.pack(">H", 0x69AA)
        self.pack_info_nt = TestPackInfo

    @lru_cache(maxsize=1024)
    def crc8(self, pack):
        return crc8(pack)

    def checkCRC(self, pack):
        return self.crc8(pack) == b"\x00"

    def dealJunkPack(self, pack, start):
        if start > 0:
            junk_pack = pack[0:start]
            logger.debug("discard junk pack | {}".format(bytesPuttyPrint(junk_pack)))

    def buildPack(self, device_id, pack_index, cmd_type, payload=None):
        pack_index = pack_index & 0xFF
        if payload is None:
            payload_len = 0
            pack_bytes_wc = self.pack_head + struct.pack("BBBB", payload_len + 3, pack_index, device_id, cmd_type)
        else:
            payload_len = len(payload)
            if payload_len > 255:
                pack_bytes_wc = self.pack_head + struct.pack("BBBB{}".format("B" * payload_len), 0, pack_index, device_id, cmd_type, *payload)
            else:
                pack_bytes_wc = self.pack_head + struct.pack("BBBB{}".format("B" * payload_len), payload_len + 3, pack_index, device_id, cmd_type, *payload)
        pack_bytes = pack_bytes_wc + self.crc8(pack_bytes_wc[4:])
        return pack_bytes

    def iterIntactPack(self, pack):
        try:
            if len(pack) < 7:
                yield self.pack_info_nt("M", pack.startswith(self.pack_head), False, pack, bytesPuttyPrint(pack))
            while len(pack) >= 7:
                try:
                    start = pack.index(self.pack_head)
                except ValueError:
                    self.dealJunkPack(pack, len(pack))
                    pack = pack[len(pack) :]
                    return
                if start + 3 > len(pack):
                    self.dealJunkPack(pack, start)
                    pack = pack[start:]
                    yield self.pack_info_nt("M", True, False, pack, bytesPuttyPrint(pack))
                    continue
                pack_length = pack[start + 2] + 4
                self.dealJunkPack(pack, start)
                sub_pack = pack[start : start + pack_length]
                pack = pack[start + pack_length :]
                if len(pack) <= 7:
                    type_s = "O"
                else:
                    type_s = "M"
                yield self.pack_info_nt(type_s, sub_pack.startswith(self.pack_head), self.checkCRC(sub_pack[4:]), sub_pack, bytesPuttyPrint(sub_pack))
            if pack:
                yield self.pack_info_nt("O", pack.startswith(self.pack_head), self.checkCRC(pack[4:]), pack, bytesPuttyPrint(pack))

        except Exception:
            logger.error("iter intact pack exception\n{}".format(stackprinter.format()))


if __name__ == "__main__":
    dc201pack = DC201_PACK()

    pack = str2Bytes(
        """42 45 47 3a 00 0D 00 01 00 04 00 00 77
        42 45 47 3a 00 13 00 01 00 22 00 02 00 fa 07 d0 00 01 15
        42 45 47 3A 00 0D 00 01 00 05 00 00 DC
        42 45 47 3A 00 0D 00 01 00 00 42 45 47 3A"""
    )

    for i in dc201pack.iterIntactPack(pack):
        print(i)

    pack = str2Bytes(
        """11 22 33 42 45 47 3a 00 0D 00 01 00 04 00 00 77 88 99 10
        42 45 47 3a 00 13 00 01 00 22 00 02 00 fa 07 d0 00 01 15
        42 45 47 3A 00 0D 00 01 00 05 00 00 DC
        42 45 47 3A 00 0D 00 01 00 00 42 45 47 3A"""
    )

    for i in dc201pack.iterIntactPack(pack):
        print(i)
