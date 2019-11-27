import loguru
import os

logger = loguru.logger

if __name__ == "__main__":
    try:
        with open("../Src/protocol.c", "a") as f:
            f.write(" ")
        with open("../Src/protocol.c", "rb+") as f:
            f.seek(-1, os.SEEK_END)
            f.truncate()
    except Exception:
        logger.exception(f"exception append\n")
