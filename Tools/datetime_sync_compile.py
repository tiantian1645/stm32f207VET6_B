import os

import stackprinter
from loguru import logger

TARGET_FILE = "../Src/protocol.c"


if __name__ == "__main__":
    try:
        if os.path.isfile(TARGET_FILE):
            os.utime(TARGET_FILE)
        else:
            logger.error(f"file path not exist {os.path.abspath(TARGET_FILE)}")
    except Exception:
        logger.exception(f"exception append\n{stackprinter.format()}")
