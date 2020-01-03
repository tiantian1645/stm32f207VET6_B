import loguru
import os
import zlib
import stackprinter
from bytes_helper import bytesPuttyPrint, crc8, str2Bytes, bytes2Float
import struct
from functools import lru_cache
from collections import namedtuple
from enum import Enum
import numpy as np

logger = loguru.logger.bind(name=__name__)
PackInfo = namedtuple("PackInfo", "type is_head is_crc content text")
TestPackInfo = namedtuple("TestPackInfo", "type is_head is_crc is_tail content text")

IDCardInfo = namedtuple("IDCardInfo", "ver_len ver_inf branch sample_type channel_confs channel_standards channel_animal_standards")
ChanelConfInfo = namedtuple("ChanelConfInfo", "name wave houhou unit precision sample_time sample_point start_point stop_point calc_point min max valid_date")
BaseInfo = namedtuple("BaseInfo", "imi raw")

ParamInfo = namedtuple("ParamInfo", "cc_temp_tops cc_temp_btms cc_temp_env cc_heaters cc_ts")
IlluminateInfo = namedtuple("IlluminateInfo", "channel wave pairs")

TEST_BIN_PATH = r"C:\Users\Administrator\STM32CubeIDE\workspace_1.0.2\stm32F207VET6_Bootloader_APP\Debug\stm32F207VET6_Bootloader_APP.bin"
REAL_BIN_PATH = r"C:\Users\Administrator\STM32CubeIDE\workspace_1.0.2\stm32f207VET6_B\Debug\stm32f207VET6_B.bin"
BL_PATH = r"C:\Users\Administrator\STM32CubeIDE\workspace_1.0.2\stm23F207VET6_Bootloader\Debug\stm23F207VET6_Bootloader.bin"


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


def iter_test_bin_BL(file_path=BL_PATH, chunk_size=224):
    try:
        total = os.path.getsize(file_path)
        crc = 0
        with open(file_path, "rb") as f:
            start = 0
            while True:
                data = f.read(chunk_size)
                if not data:
                    # https://stackoverflow.com/questions/34760988/python-struct-error
                    yield struct.pack("=HHHI", total, start, 0, crc)
                    break
                num = len(data)
                yield struct.pack(f"HHH{'B' * num}", total, start, num, *data)
                start += num
                crc = zlib.crc32(data, crc) & 0xFFFFFFFF
    except Exception:
        logger.error("read file error \n{}".format(stackprinter.format()))


def write_firmware_pack_BL(dd, file_path=BL_PATH, chunk_size=224):
    addr = 0
    pack_index = 1
    for data in iter_test_bin_BL(file_path, chunk_size):
        pack = dd.buildPack(0x13, pack_index, 0xDE, data)
        addr += chunk_size
        pack_index += 1
        yield pack


class DC201ErrorCode(Enum):
    eError_Motor_Heater_Debug = (1, "上加热体电机错误调试")
    eError_Motor_White_Debug = (2, "白板电机错误调试")
    eError_Motor_Tray_Debug = (3, "托盘电机错误调试")
    eError_Motor_Scan_Debug = (4, "扫码电机错误调试")
    eError_Scan_Debug = (5, "扫码枪错误调试")
    eError_Comm_Out_Resend_1 = (6, "采样板串口第一次发送无回应")
    eError_Comm_Out_Resend_2 = (7, "采样板串口第二次发送无回应")
    eError_Motor_Task_Busy = (100, "电机任务忙")
    eError_Motor_Notify_No_Read = (101, "电机任务通知不能读")
    eError_Storge_Task_Busy = (102, "存储任务忙")
    eError_ID_Card_Deal_Param = (103, "ID Code卡操作参数异常")
    eError_Out_Flash_Deal_Param = (104, "外部Flash操作参数异常")
    eError_Comm_Main_Wrong_ID = (105, "主串口异常ID")
    eError_Comm_Main_Unknow_CMD = (106, "主串口异常功能码")
    eError_Comm_Main_Param_Error = (107, "主串口报文参数异常")
    eError_Comm_Data_Wrong_ID = (108, "采样板串口异常ID")
    eError_Comm_Data_Unknow_CMD = (109, "采样板串口异常功能码")
    eError_Comm_Data_Param_Error = (110, "采样板串口报文参数异常")
    eError_Comm_Out_Wrong_ID = (111, "外串口异常ID")
    eError_Comm_Out_Unknow_CMD = (112, "外串口异常功能码")
    eError_Comm_Out_Param_Error = (113, "外串口报文参数异常")
    eError_Temp_BTM_Not_In_Range = (114, "下加热体温度不在范围内")
    eError_Temp_TOP_Not_In_Range = (115, "上加热体温度不在范围内")
    eError_Motor_Heater_Timeout_Up = (200, "上加热体电机运动超时 上升方向")
    eError_Motor_Heater_Timeout_Down = (201, "上加热体电机运动超时 下降方向")
    eError_Motor_Heater_Status_Warui = (202, "上加热体电机驱动状态异常")
    eError_Motor_White_Timeout_PD = (203, "白板电机运动超时 PD方向")
    eError_Motor_White_Timeout_WH = (204, "白板电机运动超时 白物质方向")
    eError_Motor_White_Status_Warui = (205, "白板电机驱动状态异常")
    eError_Motor_Tray_Busy = (206, "托盘电机驱动忙")
    eError_Motor_Tray_Timeout = (207, "托盘电机运动超时")
    eError_Motor_Tray_Status_Warui = (208, "托盘电机驱动状态异常")
    eError_Motor_Scan_Busy = (209, "扫码电机驱动忙")
    eError_Motor_Scan_Timeout = (210, "扫码电机运动超时")
    eError_Motor_Scan_Status_Warui = (211, "扫码电机驱动状态异常")
    eError_Scan_Connect_Timeout = (212, " 扫码枪通讯超时")
    eError_Scan_Config_Failed = (213, " 扫码枪配置失败")
    eError_ID_Card_Read_Failed = (214, " ID Code卡读取失败")
    eError_ID_Card_Write_Failed = (215, " ID Code卡写入失败")
    eError_Out_Flash_Read_Failed = (216, " 外部Flash读取失败")
    eError_Out_Flash_Write_Failed = (217, " 外部Flash写入失败")
    eError_Out_Flash_Storge_Param_Out_Of_Range = (218, " 外部Flash存储参数越限")
    eError_Comm_Main_Busy = (219, " 主串口发送忙")
    eError_Comm_Main_Send_Failed = (220, " 主串口发送失败")
    eError_Comm_Main_Not_ACK = (221, " 主串口没有收到ACK")
    eError_Comm_Data_Busy = (222, " 采样板串口发送忙")
    eError_Comm_Data_Send_Failed = (223, " 采样板串口发送失败")
    eError_Comm_Data_Not_ACK = (224, " 采样板串口没有收到ACK")
    eError_Comm_Data_Not_Conf = (225, " 采样板配置信息未到")
    eError_Comm_Data_Invalid_Conf = (226, " 采样信息无效")
    eError_Comm_Out_Busy = (227, " 外串口发送忙")
    eError_Comm_Out_Send_Failed = (228, " 外串口发送失败")
    eError_Comm_Out_Not_ACK = (229, " 外串口没有收到ACK")
    eError_Sample_Incomlete = (230, " 采样未完成")
    eError_Stary_Incomlete = (231, " 杂散光测试未完成")
    eError_Stary_Doing = (232, " 杂散光测试中")
    eError_Temp_BTM_Stable_Timeout = (233, " 等待下加热体温度稳定超时")
    eError_Temp_BTM_Stable_Waiting = (234, " 等待下加热体温度稳定中")
    eError_Temperature_Top_Abnormal = (300, "上加热体温度异常")
    eError_Temperature_Top_TooHigh = (301, "上加热体温度过高")
    eError_Temperature_Top_TooLow = (302, "上加热体温度过低")
    eError_Temperature_Btm_Abnormal = (303, "下加热体温度异常")
    eError_Temperature_Btm_TooHigh = (304, "下加热体温度过高")
    eError_Temperature_Btm_TooLow = (305, "下加热体温度过低")
    eError_ID_Card_Not_Insert = (400, "ID Code卡未插卡")
    eError_Out_Flash_Unknow = (401, "外部Flash型号无法识别")


class DC201_IDCardInfo:
    def __init__(self, data):
        self.data = data
        self.channel_confs = [None] * 6
        self.channel_standards = [None] * 6
        self.channel_animal_standards = [None] * 6
        self.info = None

    def decode(self):
        self.ver_len = self.data[0]
        self.ver_inf = self.genIMI_uint32(self.data[1:5])
        self.branch = self.genIMI_ASCII(self.data[5:15])
        self.sample_type = self.genIMI_SampleType(self.data[15])
        for i in range(6):
            offset = i * 50
            self.channel_confs[i] = ChanelConfInfo(
                name=self.genIMI_ASCII(self.data[16 + offset : 32 + offset]),
                wave=self.genIMI_Wave(self.data[32 + offset]),
                houhou=self.genIMI_Houhou(self.data[33 + offset]),
                unit=self.genIMI_ASCII(self.data[34 + offset : 50 + offset]),
                precision=self.data[50 + offset],
                sample_time=self.data[51 + offset : 53 + offset],
                sample_point=self.data[53 + offset],
                start_point=self.data[54 + offset],
                stop_point=self.data[55 + offset],
                calc_point=self.data[56 + offset],
                min=self.data[57 + offset : 61 + offset],
                max=self.data[61 + offset : 65 + offset],
                valid_date=self.data[65 + offset],
            )
        self.info = IDCardInfo(
            ver_len=self.ver_len,
            ver_inf=self.ver_inf,
            branch=self.branch,
            sample_type=self.sample_type,
            channel_confs=self.channel_confs,
            channel_standards=self.channel_standards,
            channel_animal_standards=self.channel_animal_standards,
        )

    def genIMI_ASCII(self, raw):
        try:
            imi = raw[1 : 1 + raw[0]].decode("ascii")
        except Exception as e:
            imi = repr(e)
        return BaseInfo(imi=imi, raw=raw)

    def genIMI_uint32(self, raw):
        imi = int.from_bytes(raw, byteorder="little")
        return BaseInfo(imi=imi, raw=raw)

    def genIMI_SampleType(self, raw):
        sample_types = ("血清", "血浆", "全血", "尿液")
        imi = []
        for i, s in enumerate(sample_types):
            if raw & (1 << i):
                imi.append(s)
        return BaseInfo(imi=imi, raw=raw)

    def genIMI_Wave(self, raw):
        waves = ("610", "550", "405")
        if raw < 1 or raw > 3:
            imi = "Unknow Wave"
        else:
            imi = waves[raw - 1]
        return BaseInfo(imi=imi, raw=raw)

    def genIMI_Houhou(self, raw):
        houhous = ("无项目", "速率法", "终点法", "两点终点法")
        if raw < 0 or raw > 3:
            imi = "Unknow Houhou"
        else:
            imi = houhous[raw]
        return BaseInfo(imi=imi, raw=raw)

        for i in range(6):
            if i == 0:
                pp = 3
                offset = 0
            else:
                pp = 2
                offset = 48
            for j in range(pp):
                logger.info(f"{'=' * 10} {i} {pp} {j} {'=' * 10}")
                for k in range(6):
                    wave = ("610", "550", "405")[j]
                    theo_start = 36 + k * 4 + j * 48 + i * 96 + offset
                    test_start = 60 + k * 4 + j * 48 + i * 96 + offset
                    logger.info(f"{wave} {k} | {theo_start} | {test_start}")
                logger.info(f"{'=' * 32}")


class DC201_ParamInfo:
    def __init__(self, data):
        self.data = data
        self.cc_temp_tops = []
        self.cc_temp_btms = []
        self.cc_temp_env = []
        self.cc_heaters = []
        self.cc_ts = []
        self.info = None
        self.decode()

    def decode(self):
        self.cc_temp_tops = [bytes2Float(self.data[i : i + 4]) for i in range(0, 24, 4)]
        self.cc_temp_btms = [bytes2Float(self.data[i : i + 4]) for i in range(24, 32, 4)]
        self.cc_temp_env = [bytes2Float(self.data[32 : 32 + 4])]
        self.cc_heaters = [bytes2Float(self.data[i : i + 4]) for i in range(36, 44, 4)]
        for i in range(6):
            ts_list = []
            channel = i + 1
            if i == 0:
                pp = 3
                offset = 0
            else:
                pp = 2
                offset = 48
            for j in range(pp):
                pairs = []
                wave = ("610", "550", "405")[j]
                for k in range(6):
                    theo_start = 44 + k * 4 + j * 48 + i * 96 + offset
                    theo = struct.unpack("I", self.data[theo_start : theo_start + 4])[0]
                    test_start = 68 + k * 4 + j * 48 + i * 96 + offset
                    test = struct.unpack("I", self.data[test_start : test_start + 4])[0]
                    pairs.append((theo, test))
                illum = IlluminateInfo(channel=channel, wave=wave, pairs=pairs)
                logger.debug(f"get illum | {illum}")
                ts_list.append(illum)
            self.cc_ts.append(ts_list)
        self.info = ParamInfo(
            cc_temp_tops=self.cc_temp_tops, cc_temp_btms=self.cc_temp_btms, cc_temp_env=self.cc_temp_env, cc_heaters=self.cc_heaters, cc_ts=self.cc_ts
        )

    def cc_temps_format(self, cc_temps):
        return [f"{t:.3f} ℃" for t in cc_temps]

    def cc_illuminate_format(self):
        result = []
        for ts in self.cc_ts:
            rr = []
            for t in ts:
                ps = [f"({p[0]},{p[1]})" for i, p in enumerate(t.pairs)]
                ps = f"[{' '.join(ps)}]"
                ks = []
                bs = []
                try:
                    for ikb, ida in enumerate(t.pairs[:-1]):
                        x = [ida[0], t.pairs[ikb + 1][0]]
                        if sum(x) == 0:
                            continue
                        y = [ida[1], t.pairs[ikb + 1][1]]
                        k, b = np.polyfit(x, y, 1)  # https://stackoverflow.com/a/21566184
                        ks.append(k)
                        bs.append(b)
                        logger.debug(f"line for {self.cc_ts.index(ts)} - {ts.index(t)} - {ikb} | y = {k:.4f} * x + {b:.4f}")
                except Exception:
                    logger.error(f"except in line fit\n{stackprinter.format()}")
                ks_text = f"[{', '.join(f'{k:.4f}' for k in ks)}]"
                bs_text = f"[{', '.join(f'{b:.4f}' for b in bs)}]"
                rr.append(f"通道 {t.channel} | 波长 {t.wave} | 数据对 {ps} | ks {ks_text} | bs {bs_text}")
            result.append("\n".join(rr))
        return f"\n{'=' * 96}\n".join(result)

    def __str__(self):
        return (
            f"上加热体校正系数 {self.cc_temps_format(self.cc_temp_tops)}\n"
            f"下加热体校正系数 {self.cc_temps_format(self.cc_temp_btms)}\n"
            f"环境温度校正系数 {self.cc_temps_format(self.cc_temp_env)}\n"
            f"目标温度偏差 {self.cc_temps_format(self.cc_heaters)}\n"
            f"OD校正配置\n{self.cc_illuminate_format()}"
        )


class DC201_PACK:
    def __init__(self):
        self.pack_head = struct.pack(">H", 0x69AA)
        self.pack_info_nt = TestPackInfo

    @lru_cache(maxsize=1024)
    def crc8(self, pack):
        return crc8(pack)

    def checkCRC(self, pack):
        return self.crc8(pack) == b"\x00"

    def checkTail(self, pack):
        if len(pack) < 7:
            return False
        pack_length = pack[2] + 4
        return len(pack) == pack_length

    def dealJunkPack(self, pack, start):
        if start > 0:
            junk_pack = pack[0:start]
            junk_text = junk_pack.decode("ascii", errors="ignore")
            logger.debug(f"discard junk pack | {bytesPuttyPrint(junk_pack)} | {junk_text}")

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
                yield self.pack_info_nt("M", pack.startswith(self.pack_head), False, self.checkTail(pack), pack, bytesPuttyPrint(pack))
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
                    yield self.pack_info_nt("M", True, False, self.checkTail(pack), pack, bytesPuttyPrint(pack))
                    continue
                self.dealJunkPack(pack, start)
                pack_length = pack[start + 2] + 4
                sub_pack = pack[start : start + pack_length]
                pack = pack[start + pack_length :]
                if len(pack) <= 7:
                    type_s = "O"
                else:
                    type_s = "M"
                yield self.pack_info_nt(
                    type_s, sub_pack.startswith(self.pack_head), self.checkCRC(sub_pack[4:]), self.checkTail(sub_pack), sub_pack, bytesPuttyPrint(sub_pack)
                )
            if pack:
                yield self.pack_info_nt("O", pack.startswith(self.pack_head), self.checkCRC(pack[4:]), self.checkTail(pack), pack, bytesPuttyPrint(pack))

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
