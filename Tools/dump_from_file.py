import openpyxl
import simplejson
import pyperclip

book = openpyxl.load_workbook(r"C:\Users\Administrator\Downloads\循环定标2.xlsx")
# book.active = 1
sheet = book.active

chanel_d = dict()
for c in range(6):  # channel
    waves = 3 if c == 0 else 2
    chanel_d[f"CH-{c+1}"] = dict()
    for w in range(waves):  # 610 550 405?
        pairs = []
        for s in range(6):  # s1 ~ s6
            if w == 0:
                theo = 500 + 2000 * s
                cell = sheet[f'{chr(ord("B") + c)}{2 + s}']
                pairs.append((theo, round(cell.value)))
            elif w == 1:
                theo = 500 + 2000 * s
                cell = sheet[f'{chr(ord("I") + c)}{2 + s}']
                pairs.append((theo, round(cell.value)))
            elif w == 2:
                pairs.append((0, 0))
        chanel_d[f"CH-{c+1}"][("610", "550", "405")[w]] = pairs

records = simplejson.dumps(chanel_d)
pyperclip.copy(records)
