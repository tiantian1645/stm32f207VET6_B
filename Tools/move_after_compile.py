import os
import re
import shutil
from datetime import datetime
from pathlib import Path

import stackprinter
from git import Repo
from intelhex import IntelHex
from loguru import logger

# bootloader
BL_VERSION_RES = (
    re.compile(r"#define BOOTLOADER_YEAR \((\d+)\)"),
    re.compile(r"#define BOOTLOADER_MONTH \((\d+)\)"),
    re.compile(r"#define BOOTLOADER_DAY \((\d+)\)"),
    re.compile(r"#define BOOTLOADER_VERSION \((\d+)\)"),
)


def get_bootloader_version_str():
    version_ms = [None] * len(BL_VERSION_RES)
    with open("../../stm32f207VET6_Bootloader/Core/Inc/main.h", "r", encoding="utf-8") as f:
        for line in f.readlines():
            for rm in BL_VERSION_RES:
                rr = rm.match(line)
                if rr is None:
                    continue
                version_ms[BL_VERSION_RES.index(rm)] = int(rr.group(1))
                break
    if all(version_ms):
        return f"{version_ms[3]}-20{version_ms[0]:02d}-{version_ms[1]:02d}-{version_ms[2]:02d}"
    return ""


TARGET_DIR = Path("E:\\WebServer\\DC201\\程序\\控制板\\Application\\")
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
    archive_parent_dir = TARGET_DIR / f"V{get_version_str()}"
    head_hash = REPO.head.object.hexsha
    if not archive_parent_dir.is_dir():
        logger.info(f"new hash for new version | {head_hash}")
        return True

    slib_dirs = [i for i in sorted(archive_parent_dir.iterdir()) if i.is_dir()]
    if len(slib_dirs) == 0:
        logger.warning(f"new hash for no sub dir | {head_hash}")
        return True

    last_archive_commit = REPO.commit(SHA_RE.match(slib_dirs[-1].name).group(1))
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
    if not TARGET_DIR.is_dir():
        TARGET_DIR.mkdir(exist_ok=True)
    #  复制到根目录
    for f in TARGET_DIR.glob("stm32f207VET6_B-*"):
        logger.debug(f"remove old file | {f}")
        os.remove(f)
    shutil.copyfile("../Debug/stm32f207VET6_B.bin", TARGET_DIR / f"stm32f207VET6_B-{datetime_str}-v{version_str}.bin")
    shutil.copyfile("../Debug/stm32f207VET6_B.hex", TARGET_DIR / f"stm32f207VET6_B-{datetime_str}-v{version_str}.hex")
    #  归档
    if check_archive():
        archive_dir = TARGET_DIR / f"V{version_str}" / f"{now.strftime('%Y%m%d-%H%M%S')}-{REPO.head.object.hexsha}"
        name = f"stm32f207VET6_B-{datetime_str}-v{version_str}"
        archive_dir.mkdir(parents=True, exist_ok=True)
        shutil.copyfile("../Debug/stm32f207VET6_B.bin", archive_dir / f"{name}.bin")
        shutil.copyfile("../Debug/stm32f207VET6_B.hex", archive_dir / f"{name}.hex")
    # 合并 hex 文件
    bl_hex_dir = TARGET_DIR.parent / "Bootloader"
    bl_hex_path_list = list(bl_hex_dir.rglob("*.hex"))

    if not bl_hex_path_list:
        logger.error(f"could not find bootloader hex file in | {bl_hex_dir}")
    else:
        for i in TARGET_DIR.parent.glob("dc201_control_board_whole*"):
            logger.debug(f"remove | {i}")
            os.remove(i)
        bl_hex_path = bl_hex_path_list[0]
        logger.info(f"find bootload hex path | {bl_hex_path}")
        app_hex_path = "../Debug/stm32f207VET6_B.hex"
        whole_name = f"dc201_control_board_whole_{datetime_str}_v{get_version_str()}_bv{get_bootloader_version_str()}.hex"
        output_path = TARGET_DIR.parent / whole_name
        merge_hex_by_intelhex(bl_hex_path.as_posix(), app_hex_path, output_path.as_posix())


if __name__ == "__main__":
    try:
        main()
    except Exception:
        logger.error(f"error in main loog \n{stackprinter.format()}")
