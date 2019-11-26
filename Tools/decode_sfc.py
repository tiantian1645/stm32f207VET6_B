import loguru
import pyperclip
from lxml import etree
from enum import Enum

logger = loguru.logger


class dSPIN_CONF_PARAM(Enum):
    dSPIN_CONF_PARAM_ABS_POS = 0x01
    dSPIN_CONF_PARAM_EL_POS = 0x02
    dSPIN_CONF_PARAM_MARK = 0x03
    dSPIN_CONF_PARAM_SPEED = 0x04
    dSPIN_CONF_PARAM_ACC = 0x05
    dSPIN_CONF_PARAM_DEC = 0x06
    dSPIN_CONF_PARAM_MAX_SPEED = 0x07
    dSPIN_CONF_PARAM_MIN_SPEED = 0x08
    dSPIN_CONF_PARAM_KVAL_HOLD = 0x09
    dSPIN_CONF_PARAM_KVAL_RUN = 0x0A
    dSPIN_CONF_PARAM_KVAL_ACC = 0x0B
    dSPIN_CONF_PARAM_KVAL_DEC = 0x0C
    dSPIN_CONF_PARAM_INT_SPD = 0x0D
    dSPIN_CONF_PARAM_ST_SLP = 0x0E
    dSPIN_CONF_PARAM_FN_SLP_ACC = 0x0F
    dSPIN_CONF_PARAM_FN_SLP_DEC = 0x10
    dSPIN_CONF_PARAM_K_THERM = 0x11
    dSPIN_CONF_PARAM_ADC_OUT = 0x12
    dSPIN_CONF_PARAM_OCD_TH = 0x13
    dSPIN_CONF_PARAM_STALL_TH = 0x14
    dSPIN_CONF_PARAM_FS_SPD = 0x15
    dSPIN_CONF_PARAM_STEP_MODE = 0x16
    dSPIN_CONF_PARAM_ALARM_EN = 0x17
    dSPIN_CONF_PARAM_CONFIG = 0x18


tree = etree.parse(r"C:\Users\Administrator\Documents\tray_0.3A.sfc")
root = tree.getroot()

records = []
for reg in root.findall("Registers")[1:]:
    addr = int(reg.find("Addr").text)
    value = int(reg.find("Hex").text)
    logger.info(f"get Addr | {dSPIN_CONF_PARAM(addr).name} | {value}")
    records.append(f"#define {dSPIN_CONF_PARAM(addr).name} ({value})")
pyperclip.copy("\n".join(records))
