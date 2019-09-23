import math


def get_temp_by_adc(c, b=3380):
    return (1 / (1 / (25 + 273.15) - ((math.log(c / (4095 - c))) / b))) - 273.15


def get_adc_by_resust(r):
    return (10 / (10 + r)) * 4095
