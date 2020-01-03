import os
import pathlib
import re
import shutil
from datetime import datetime

import stackprinter
from git import Repo
from loguru import logger


TARGET_DIR = "E:\\WebServer\\DC201\\程序\\控制板\\"
REPO = Repo(search_parent_directories=True)


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


def check_by_git():
    if REPO.is_dirty():
        return False
    return True


def check_file_change():
    archive_parent_dir = os.path.join(TARGET_DIR, f"V{get_version_str()}")
    if not os.path.isdir(archive_parent_dir):
        return True
    head_hash = REPO.head.object.hexsha
    for fn in os.listdir(archive_parent_dir):
        if head_hash in fn:
            logger.info(f"hit same hash | {fn}")
            return False
    logger.info(f"new hash | {head_hash}")
    return True


def check_archive():
    return check_by_git() and check_file_change()


def main():
    logger.debug(f"current path | {os.path.abspath('./')}")
    version_str = get_version_str()
    now = datetime.now()
    datetime_str = now.strftime(r"%Y%m%d%H%M%S")
    #  复制到根目录
    for fn in os.listdir(TARGET_DIR):
        if fn.startswith("stm32f207VET6_B-"):
            file_path = os.path.join(TARGET_DIR, fn)
            logger.debug(f"remove old file | {file_path}")
            os.remove(file_path)
    shutil.copyfile("../Debug/stm32f207VET6_B.bin", f"{TARGET_DIR}stm32f207VET6_B-{datetime_str}-v{version_str}.bin")
    shutil.copyfile("../Debug/stm32f207VET6_B.hex", f"{TARGET_DIR}stm32f207VET6_B-{datetime_str}-v{version_str}.hex")
    #  归档
    if check_archive():
        archive_dir = os.path.join(TARGET_DIR, f"V{version_str}", f"{now.strftime('%Y%m%d_%H%M%S')}-{REPO.head.object.hexsha}")
        name = f"stm32f207VET6_B-{datetime_str}-v{version_str}"
        pathlib.Path(archive_dir).mkdir(parents=True, exist_ok=True)
        shutil.copyfile("../Debug/stm32f207VET6_B.bin", os.path.join(archive_dir, f"{name}.bin"))
        shutil.copyfile("../Debug/stm32f207VET6_B.hex", os.path.join(archive_dir, f"{name}.hex"))


if __name__ == "__main__":
    try:
        main()
    except Exception:
        logger.error(f"error in main loog \n{stackprinter.format()}")
