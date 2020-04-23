import re
from urllib.parse import urljoin

import requests
import stackprinter
from loguru import logger

from datetime import datetime

VERSION_RE = re.compile(r"V\d\.\d\d\d/")
FLODER_RE = re.compile(r"\d{8}-\d{6}-[0-9a-f]{40}/")
APP_BIN_RE = re.compile(r"stm32f207VET6_B-(\d{14})-(v\d\.\d\d\d).bin")


def get_latest_app_bin():
    url = "http://la.mengyyy.bid:8081/DC201/%E7%A8%8B%E5%BA%8F/%E6%8E%A7%E5%88%B6%E6%9D%BF/Application/"
    result = (None, None, None, None)
    try:
        resp = requests.get(url, auth=("wondfo", "wondfo+!S"), timeout=10)
        if resp.status_code != 200:
            logger.error(f"status code error | {resp.status_code}")
            return result

        floder_url = urljoin(url, VERSION_RE.findall(resp.text)[-1])
        resp = requests.get(floder_url, auth=("wondfo", "wondfo+!S"), timeout=10)
        if resp.status_code != 200:
            logger.error(f"status code error | {resp.status_code}")
            return result

        sub_floder_url = urljoin(floder_url, FLODER_RE.findall(resp.text)[-1])
        resp = requests.get(sub_floder_url, auth=("wondfo", "wondfo+!S"), timeout=10)
        if resp.status_code != 200:
            logger.error(f"status code error | {resp.status_code}")
            return result

        m = APP_BIN_RE.search(resp.text)
        filename = m.group(0)
        datetime_obj = datetime.strptime(m.group(1), r"%Y%m%d%H%M%S")
        version = m.group(2)
        resp = requests.get(urljoin(url, filename), auth=("wondfo", "wondfo+!S"), timeout=10)
        if resp.status_code != 200:
            logger.error(f"status code error | {resp.status_code}")
            return result
        result = (resp.content, filename, datetime_obj, version)
    except Exception:
        logger.error(f"could not get app bin\n{stackprinter.format()}")
    finally:
        return result
