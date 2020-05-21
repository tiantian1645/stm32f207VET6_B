from bytes_helper import crc8
from collections import namedtuple

Criterion = namedtuple("Criterion", "batch date stage list_610 list_550 list_405")


def make_barcode(c):
    text_610 = "".join(f"{i:04X}" for i in c.list_610)
    text_550 = "".join(f"{i:04X}" for i in c.list_550)
    text_405 = "".join(f"{i:04X}" for i in c.list_405)
    text_payload = f"{c.batch:04d}{c.date:06d}{c.stage:d}{text_610}{text_550}{text_405}"
    crc = crc8(text_payload.encode("ascii")).hex().upper()
    text_all = f"{text_payload}{crc}"
    return text_all


c = Criterion(1234, 201205, 0, [0x1234, 0x4321, 0x3456, 0x6543, 0x5678, 0x8765], [0x789A, 0xA987, 0x9517, 0x1739, 0x9371, 0x2580], [0x7410])
r = "12342012050123443213456654356788765789AA987951717399371258074105B"
assert make_barcode(c) == r


c0 = Criterion(1, 200521, 0, [1057, 1019, 1046, 1047, 1033, 1093], [2878, 2824, 2826, 2906, 2798, 2838], [0])
c2 = Criterion(1, 200521, 1, [2692, 2689, 2681, 2688, 2664, 2682], [4548, 4544, 4561, 4563, 4569, 4555], [0])
c4 = Criterion(1, 200521, 2, [4660, 4728, 4661, 4689, 4633, 4656], [6588, 6600, 6548, 6579, 6565, 6548], [0])
c6 = Criterion(1, 200521, 3, [6647, 6690, 6728, 6681, 6634, 6692], [8736, 8695, 8680, 8681, 8688, 8674], [0])
c8 = Criterion(1, 200521, 4, [8537, 8527, 8582, 8542, 8561, 8476], [10660, 10729, 10716, 10714, 10569, 10716], [0])
c10 = Criterion(1, 200521, 5, [10256, 10207, 10315, 10289, 10094, 10258], [12684, 12729, 12781, 12699, 12721, 12649], [0])


r0 = make_barcode(c0)
r2 = make_barcode(c2)
r4 = make_barcode(c4)
r6 = make_barcode(c6)
r8 = make_barcode(c8)
r10 = make_barcode(c10)
