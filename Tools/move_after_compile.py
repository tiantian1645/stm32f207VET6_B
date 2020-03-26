import os
import pathlib
import re
import shutil
from datetime import datetime
from pathlib import Path

import stackprinter
from git import Repo
from intelhex import IntelHex
from loguru import logger

TARGET_DIR = "E:\\WebServer\\DC201\\程序\\控制板\\Application\\"
REPO = Repo(search_parent_directories=True)
VERSION_RE = re.compile(r"#define APP_VERSION \(\(float\)([\d\.]+)\)")
SHA_RE = re.compile(r"\d{8}-\d{6}-(\w{40})")


def get_version_str():

    with open("../Inc/main.h", "r", encoding="utf-8") as f:
        for line in f.readlines():
            version_match = VERSION_RE.match(line)
            if version_match:
                version_str = version_match.group(1)
                logger.debug(f"hit line | {line[:-1]} | {version_str}")
                return version_str
    return ""


def check_by_diff(diff):
    dps = tuple(d.a_path for d in diff)
    if any((os.path.splitext(dp)[1].lower() not in (".py", ".json") for dp in dps)):
        logger.debug(f"find valid change in dps {dps}")
        return True
    logger.debug(f"do not find valid change in dps {dps}")
    return False


def check_archive():
    if REPO.is_dirty():
        return False
    archive_parent_dir = os.path.join(TARGET_DIR, f"V{get_version_str()}")
    head_hash = REPO.head.object.hexsha
    if not os.path.isdir(archive_parent_dir):
        logger.info(f"new hash for new version | {head_hash}")
        return True

    slib_dirs = sorted(os.listdir(archive_parent_dir))
    if len(slib_dirs) == 0:
        logger.warning(f"new hash for no sub dir | {head_hash}")
        return True

    last_archive_commit = REPO.commit(SHA_RE.match(slib_dirs[-1]).group(1))
    diff = REPO.commit().diff(last_archive_commit)
    return check_by_diff(diff)


def merge_hex_by_intelhex(bl_hex_path, app_hex_path, output_path):
    origin = IntelHex(bl_hex_path)
    new = IntelHex(app_hex_path)
    origin.merge(new, overlap="ignore")
    file_name = os.path.splitext(output_path)[0]
    origin.tofile(f"{file_name}.hex", format="hex")
    origin.tofile(f"{file_name}.bin", format="bin")
    # https://python-intelhex.readthedocs.io/en/latest/part2-6.html
    # origin = IntelHex(bl_hex_path)
    # origin.merge(new, overlap="replace")
    # origin.tofile(f"{file_name}_r.hex", format="hex")
    # origin.tofile(f"{file_name}_r.bin", format="bin")


def main():
    logger.debug(f"current path | {os.path.abspath('./')}")
    version_str = get_version_str()
    now = datetime.now()
    datetime_str = now.strftime(r"%Y%m%d%H%M%S")
    if not os.path.isdir(TARGET_DIR):
        os.makedirs(TARGET_DIR)
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
        archive_dir = os.path.join(TARGET_DIR, f"V{version_str}", f"{now.strftime('%Y%m%d-%H%M%S')}-{REPO.head.object.hexsha}")
        name = f"stm32f207VET6_B-{datetime_str}-v{version_str}"
        pathlib.Path(archive_dir).mkdir(parents=True, exist_ok=True)
        shutil.copyfile("../Debug/stm32f207VET6_B.bin", os.path.join(archive_dir, f"{name}.bin"))
        shutil.copyfile("../Debug/stm32f207VET6_B.hex", os.path.join(archive_dir, f"{name}.hex"))
    # 合并 hex 文件
    bl_hex_dir = os.path.join(Path(TARGET_DIR).parent, "Bootloader")
    bl_hex_path = None
    for dp, dns, fns in os.walk(bl_hex_dir, topdown=False):
        for fn in fns:
            file_path = os.path.join(dp, fn)
            if file_path.endswith("hex"):
                bl_hex_path = file_path
                break
    if bl_hex_path:
        logger.info(f"find bootload hex path | {bl_hex_path}")
        app_hex_path = "../Debug/stm32f207VET6_B.hex"
        output_path = os.path.join(Path(TARGET_DIR).parent, "dc201_control_board_whole.hex")
        merge_hex_by_intelhex(bl_hex_path, app_hex_path, output_path)


if __name__ == "__main__":
    try:
        main()
    except Exception:
        logger.error(f"error in main loog \n{stackprinter.format()}")
