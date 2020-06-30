# https://stackoverflow.com/questions/3694918/how-to-extract-frequency-associated-with-fft-values-in-python
# https://stackoverflow.com/questions/51298604/calculating-amplitude-from-np-fft
import numpy as np


def get_freq_amp_from_data(data, frate=31250):
    da = (max(data) + min(data)) // 2

    x = np.array([i - da for i in data])
    w = np.fft.fft(x)

    # Find the peak in the coefficients
    idx = np.argmax(np.abs(w))
    # calculate frep
    freqs = np.fft.fftfreq(len(x))
    freq = freqs[idx]
    freq_in_hertz = abs(freq * frate)
    # # calculate amplitude
    amplitude = 2 / len(data) * np.abs(w[idx])
    return freq_in_hertz, amplitude
