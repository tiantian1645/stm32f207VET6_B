import loguru
import stackprinter
from bytes_helper import bytesPuttyPrint, crc8, str2Bytes
import struct
from functools import lru_cache
from collections import namedtuple

logger = loguru.logger.bind(name=__name__)
PackInfo = namedtuple("PackInfo", "type is_head is_crc content text")
TestPackInfo = namedtuple("TestPackInfo", "type is_head is_crc content text")


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
        if payload is None:
            payload_len = 0
            pack_bytes_wc = self.pack_head + struct.pack("BBBB", payload_len + 3, pack_index, device_id, cmd_type)
        else:
            payload_len = len(payload)
            pack_bytes_wc = self.pack_head + struct.pack(
                "BBBB{}".format("B" * payload_len), payload_len + 3, pack_index, device_id, cmd_type, *payload
            )
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
