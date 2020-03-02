import random

from loguru import logger
from PIL import Image
from pylibdmtx.pylibdmtx import encode


def gen_code_img(content="hello world".encode("utf8"), file_path="test.jpg"):
    encoded = encode(content)
    img = Image.frombytes("RGB", (encoded.width, encoded.height), encoded.pixels)
    logger.debug(f"size | {(encoded.width, encoded.height)}")
    img.save(file_path)


def gen_test_barcode():
    # 批号 4位
    branch = random.randint(0, 1e4 - 1)
    logger.debug(f"branch | {branch}")
    # 日期 6位
    dt = random.randint(0, 1e6 - 1)
    logger.debug(f"dt | {dt}")
    # 标段索引 6位
    stages = [random.randint(0, 5) for _ in range(6)]
    logger.debug(f"stages | {stages}")
    # 标准值 4 * 13 = 52 位
    i_values = [random.randint(0, 65536) for _ in range(13)]
    logger.debug(f"i_values | {i_values}")
    # 校验码 2 位
    crc = random.randint(0, 1e2 - 1)
    logger.debug(f"crc | {crc}")

    stages_str = "".join(f"{i:d}" for i in stages)
    i_values_str = "".join(f"{i:04x}" for i in i_values)
    logger.debug(f"{branch:04d} | {dt:06d} | {stages_str} | {i_values_str} | {crc:02x}")
    return f"{branch:04d}{dt:06d}{stages_str}{i_values_str}{crc:02x}"


if __name__ == "__main__":
    content = gen_test_barcode().encode("utf-8")
    gen_code_img(content, "E:/Download/test.bmp")
