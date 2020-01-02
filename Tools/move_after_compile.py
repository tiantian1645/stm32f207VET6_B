import os
import re
from loguru import logger
import shutil
from datetime import datetime


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
    datetime_str = datetime.now().strftime(r"%Y%m%d%H%M%S")
    target_dir = "E:\\WebServer\\DC201\\程序\\控制板\\"
    for fn in os.listdir(target_dir):
        if fn.startswith("stm32f207VET6_B-"):
            file_path = os.path.join(target_dir, fn)
            logger.debug(f"remove old file | {file_path}")
            os.remove(file_path)
    shutil.move("../Debug/stm32f207VET6_B.bin", f"{target_dir}stm32f207VET6_B-{datetime_str}-v{version_str}.bin")
    shutil.move("../Debug/stm32f207VET6_B.hex", f"{target_dir}stm32f207VET6_B-{datetime_str}-v{version_str}.hex")


if __name__ == "__main__":
    main()
