import os
import pathlib
import re
import shutil
from datetime import datetime

import stackprinter
from loguru import logger


def get_version_str():
    version_re = re.compile(r"#define APP_VERSION \(\(float\)([\d\.]+)\)")
    with open("../Inc/main.h", "r", encoding="utf-8") as f:
        for line in f.readlines():
            version_match = version_re.match(line)
            if version_match:
                version_str = version_match.group(1)
                logger.debug(f"hit line | {line[:-1]} | {version_str}")
                return version_str
    return ""


def main():
    logger.debug(f"current path | {os.path.abspath('./')}")
    version_str = get_version_str()
    now = datetime.now()
    datetime_str = now.strftime(r"%Y%m%d%H%M%S")
    #  复制到根目录
    target_dir = "E:\\WebServer\\DC201\\程序\\控制板\\"
    for fn in os.listdir(target_dir):
        if fn.startswith("stm32f207VET6_B-"):
            file_path = os.path.join(target_dir, fn)
            logger.debug(f"remove old file | {file_path}")
            os.remove(file_path)
    shutil.copyfile("../Debug/stm32f207VET6_B.bin", f"{target_dir}stm32f207VET6_B-{datetime_str}-v{version_str}.bin")
    shutil.copyfile("../Debug/stm32f207VET6_B.hex", f"{target_dir}stm32f207VET6_B-{datetime_str}-v{version_str}.hex")
    #  归档
    target_dir = f"E:\\WebServer\\DC201\\程序\\控制板\\V{version_str}\\{now.strftime('%Y%m%d_%H%M%S')}"
    name = f"stm32f207VET6_B-{datetime_str}-v{version_str}"
    pathlib.Path(target_dir).mkdir(parents=True, exist_ok=True)
    shutil.copyfile("../Debug/stm32f207VET6_B.bin", os.path.join(target_dir, f"{name}.bin"))
    shutil.copyfile("../Debug/stm32f207VET6_B.hex", os.path.join(target_dir, f"{name}.hex"))


if __name__ == "__main__":
    try:
        main()
    except Exception:
        logger.error(f"error in main loog \n{stackprinter.format()}")
