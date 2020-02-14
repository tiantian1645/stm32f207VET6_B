from collections import namedtuple

import stackprinter
from loguru import logger

from openpyxl import Workbook, load_workbook
from openpyxl.cell import WriteOnlyCell

TEMP_CC_DataInfo = namedtuple("TEMP_CC_DataInfo", "tops btms env heaters")
ILLU_CC_DataInfo = namedtuple("ILLU_CC_DataInfo", "wave standard_points channel_pointses")


def dump_CC(data, file_path):
    wb = Workbook(write_only=True)
    sheets = [wb.create_sheet(f"{title}") for title in (610, 550, 405, "温度")]
    # build frame
    for idx, sheet in enumerate(sheets[:3]):
        # https://www.johndcook.com/wavelength_to_RGB.html
        sheet.sheet_properties.tabColor = ("ff9b00", "a3ff00", "8200c8")[idx]
        cells = [WriteOnlyCell(sheet, value=(sheet.title, "测试点-1", "测试点-2", "测试点-3", "测试点-4", "测试点-5", "测试点-6")[i]) for i in range(7)]
        sheet.append(cells)
    sheet = sheets[-1]
    cells = [WriteOnlyCell(sheet, value=f"探头-{i + 1}") for i in range(9)] + [WriteOnlyCell(sheet, value=["目标温度偏移-上", "目标温度偏移-下"][i]) for i in range(2)]
    sheet.append(cells)
    # insert data
    try:
        for d in data:
            if isinstance(d, ILLU_CC_DataInfo):
                sheet = wb[f"{d.wave}"]
                cells = [WriteOnlyCell(sheet, value="理论值")] + [WriteOnlyCell(sheet, value=sp) for sp in d.standard_points]
                sheet.append(cells)
                for row_idx, cps in enumerate(d.channel_pointses):
                    cells = [WriteOnlyCell(sheet, value=f"通道-{row_idx + 1}")] + [WriteOnlyCell(sheet, value=cp) for cp in cps]
                    sheet.append(cells)
            elif isinstance(d, TEMP_CC_DataInfo):
                sheet = wb["温度"]
                cells = (
                    [WriteOnlyCell(sheet, value=i) for i in d.tops]
                    + [WriteOnlyCell(sheet, value=i) for i in d.btms]
                    + [WriteOnlyCell(sheet, value=d.env)]
                    + [WriteOnlyCell(sheet, value=i) for i in d.heaters]
                )
                sheet.append(cells)
        wb.save(file_path)
        return True
    except Exception:
        logger.error(f"save data to xlsx failed\n{stackprinter.format()}")


def load_CC(file_path):
    try:
        wb = load_workbook(file_path, read_only=True)
        data = []
        for t_idx, title in enumerate((610, 550, 405)):
            sheet = wb[f"{title}"]
            standard_points = tuple(sheet.cell(column=2 + i, row=2).value for i in range(6))
            channel_pointses = []
            cn = 6 if t_idx != 2 else 1
            for row_idx in range(cn):
                channel_pointses.append(tuple(sheet.cell(column=column_idx + 2, row=3 + row_idx).value for column_idx in range(6)))
            data.append(ILLU_CC_DataInfo(wave=title, standard_points=standard_points, channel_pointses=tuple(channel_pointses)))
        sheet = wb["温度"]
        data.append(
            TEMP_CC_DataInfo(
                tops=tuple(sheet.cell(column=column_idx, row=2).value for column_idx in range(1, 7)),
                btms=tuple(sheet.cell(column=column_idx, row=2).value for column_idx in range(7, 9)),
                env=sheet.cell(column=9, row=2).value,
                heaters=tuple(sheet.cell(column=column_idx, row=2).value for column_idx in range(10, 12)),
            )
        )
        return tuple(data)
    except Exception:
        logger.error(f"load data from xlsx failed\n{stackprinter.format()}")


if __name__ == "__main__":
    file_path = r"C:\Users\Administrator\Desktop\test.xlsx"
    data = (
        ILLU_CC_DataInfo(
            wave=610,
            standard_points=(30, 31, 32, 33, 34, 35),
            channel_pointses=(
                (1100, 2200, 3300, 4400, 5500, 6600),
                (1101, 2201, 3301, 4401, 5501, 6601),
                (1102, 2202, 3302, 4402, 5502, 6602),
                (1103, 2203, 3303, 4403, 5503, 6603),
                (1104, 2204, 3304, 4404, 5504, 6604),
                (1105, 2205, 3305, 4405, 5505, 6605),
            ),
        ),
        ILLU_CC_DataInfo(
            wave=550,
            standard_points=(26, 27, 28, 29, 10, 11),
            channel_pointses=(
                (7100, 8200, 9300, 10400, 11500, 12600),
                (7101, 8201, 9301, 10401, 11501, 12601),
                (7102, 8202, 9302, 10402, 11502, 12602),
                (7103, 8203, 9303, 10403, 11503, 12603),
                (7104, 8204, 9304, 10404, 11504, 12604),
                (7105, 8205, 9305, 10405, 11505, 12605),
            ),
        ),
        ILLU_CC_DataInfo(wave=405, standard_points=(13, 14, 15, 16, 17, 18,), channel_pointses=((18100, 17200, 16300, 15400, 14500, 13600),),),
        TEMP_CC_DataInfo(tops=(1.0, 1.5, 2.0, 2.5, 3.0, 3.6), btms=(4.0, 5.0), env=6, heaters=(7.0, 8.0)),
    )
    dump_CC(data, file_path)
    rdata = load_CC(file_path)
    assert rdata == data
