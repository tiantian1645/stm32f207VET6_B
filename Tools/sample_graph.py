import loguru
import pyqtgraph as pg
from collections import namedtuple
import stackprinter
from bisect import bisect_left

logger = loguru.logger
DataConf = namedtuple("DataConf", "name data color plot")
DEFAULT_COLORS = (
    "BB86FC",
    "FF0266",
    "FF7597",
    "00a300",
    "2451c4",
    "F39C12",
    "1e7145",
    "ff0097",
    "03DAC5",
    "9f00a7",
    "00aba9",
    "2d89ef",
    "ee1111",
    "ffc40d",
    "99b433",
    "2b5797",
    "da532c",
    "b91d47",
)


def take_closest(myList, myNumber):
    """
    https://stackoverflow.com/a/12141511

    Assumes myList is sorted. Returns closest value to myNumber.

    If two numbers are equally close, return the smallest number.
    """
    pos = bisect_left(myList, myNumber)
    if pos == 0:
        return myList[0]
    if pos == len(myList):
        return myList[-1]
    before = myList[pos - 1]
    after = myList[pos]
    if after - myNumber < myNumber - before:
        return after
    else:
        return before


class SampleGraph:
    def __init__(self, *args, **kwargs):
        self.win = pg.GraphicsLayoutWidget(*args, **kwargs)
        # self.win.setBackground("121212")
        self.win.setWindowTitle("Test Title")
        self.label = pg.LabelItem(justify="left")
        self.win.addItem(self.label)
        self.vLine = pg.InfiniteLine(angle=90, movable=False)
        self.hLine = pg.InfiniteLine(angle=0, movable=False)
        self.plot = self.win.addPlot(row=1, col=0)
        # self.plot.addLegend()
        self.plot.showGrid(x=True, y=True, alpha=1.0)
        self.plot.addItem(self.vLine, ignoreBounds=True)
        self.plot.addItem(self.hLine, ignoreBounds=True)
        # pg.SignalProxy(self.plot.scene().sigMouseMoved, rateLimit=60, slot=self.mouseMoved)
        self.plot.scene().sigMouseMoved.connect(self.mouseMoved)
        self.plot_data_confs = []

    def take_closest_index(self, data_list, data):
        pos = bisect_left(data_list, data)
        if pos == 0:
            return pos
        if pos == len(data_list):
            return pos - 1
        before = data_list[pos - 1]
        after = data_list[pos]
        if after - data < data - before:
            return pos
        else:
            return pos - 1

    def plot_data_new(self, data=None, name=None, color=None):
        if data is None:
            logger.warning("data is None new as list")
            data = []
        dl = len(self.plot_data_confs)
        if color is None:
            color = DEFAULT_COLORS[dl % len(DEFAULT_COLORS)]
        if name is None:
            name = f"y{dl + 1}"
        # self.matplot_plots.append(self.plot_wg.plot([], name=f"B{k+1}", pen=mkPen(color=color), symbol=symbol, symbolSize=5, symbolBrush=(color)))
        p = self.plot.plot(data, name=name, pen=pg.mkPen(color=color), symbolSize=5, symbolBrush=(color))
        data_conf = DataConf(name=name, data=data, color=color, plot=p)
        self.plot_data_confs.append(data_conf)

    def plot_data_update(self, idx, data):
        if idx >= len(self.plot_data_confs):
            return False
        pdc = self.plot_data_confs[idx]
        pdc.data.append(data)
        pdc.plot.setData(pdc.data)

    def clear_plot(self):
        self.plot_data_confs.clear()
        self.plot.clear()
        self.plot.addItem(self.vLine, ignoreBounds=True)
        self.plot.addItem(self.hLine, ignoreBounds=True)

    def mouseMoved(self, evt):
        # logger.debug(f"get evt in mouseMoved | {evt}")
        pos = evt
        if self.plot.sceneBoundingRect().contains(pos):
            mousePoint = self.plot.vb.mapSceneToView(pos)
            x = mousePoint.x()
            y = mousePoint.y()
            index = round(x)
            label_point = f"<span style='font-size: 12pt'>{x:0.1f}, <span style='font-size: 12pt'>{y:0.1f}"
            lds = []
            for data_conf in self.plot_data_confs:
                if data_conf.data is not None and index >= 0 and index < len(data_conf.data):
                    value = data_conf.data[index]
                    if isinstance(value, int):
                        lds.append(f"<span style='font-size: 12pt; color: #{data_conf.color}'>{data_conf.name}={value:d}</span>")
                    elif isinstance(value, float):
                        lds.append(f"<span style='font-size: 12pt; color: #{data_conf.color}'>{data_conf.name}={value:.2f}</span>")
                else:
                    lds.append(f"<span style='font-size: 12pt; color: #{data_conf.color}'>{data_conf.name}=null</span>")
            label_data = ",   ".join(lds)
            label_text = f"{label_data}  ({label_point})"
            self.label.setText(label_text)
            # logger.debug(f"label text | {label_text}")
            self.vLine.setPos(x)
            self.hLine.setPos(y)


class TemperatureGraph(SampleGraph):
    def plot_data_new(self, data_x=None, data_y=None, name=None, color=None):
        if data_x is None:
            logger.warning("data_x is None new as list")
            data_x = []
        if data_y is None:
            logger.warning("data_y is None new as list")
            data_y = []
        dl = len(self.plot_data_confs)
        if color is None:
            color = DEFAULT_COLORS[dl % len(DEFAULT_COLORS)]
        if name is None:
            name = f"y{dl + 1}"
        # self.matplot_plots.append(self.plot_wg.plot([], name=f"B{k+1}", pen=mkPen(color=color), symbol=symbol, symbolSize=5, symbolBrush=(color)))
        p = self.plot.plot(data_x, data_y, name=name, pen=pg.mkPen(color=color))
        data_conf = DataConf(name=name, data=[data_x, data_y], color=color, plot=p)
        self.plot_data_confs.append(data_conf)

    def plot_data_update(self, idx, data_x, data_y):
        if idx >= len(self.plot_data_confs):
            logger.error(f"index error {idx} | {self.plot_data_confs}")
            return False
        pdc = self.plot_data_confs[idx]
        pdc.data[0].append(data_x)
        pdc.data[1].append(data_y)
        try:
            pdc.plot.setData(pdc.data[0], pdc.data[1])
        except Exception:
            logger.error(f"set data exception | {pdc} \n{stackprinter.format()}")

    def mouseMoved(self, evt):
        # logger.debug(f"get evt in mouseMoved | {evt}")
        pos = evt
        if self.plot.sceneBoundingRect().contains(pos):
            mousePoint = self.plot.vb.mapSceneToView(pos)
            x = mousePoint.x()
            y = mousePoint.y()
            label_point = f"<span style='font-size: 12pt'>{x:0.2f}, <span style='font-size: 12pt'>{y:0.2f}"
            lds = []
            for data_conf in self.plot_data_confs:
                index = self.take_closest_index(data_conf.data[0], x)
                if index >= 0 and index < len(data_conf.data[0]):
                    value = data_conf.data[1][index]
                    if isinstance(value, int):
                        lds.append(f"<span style='font-size: 12pt; color: #{data_conf.color}'>{data_conf.name}={value:d}</span>")
                    elif isinstance(value, float):
                        lds.append(f"<span style='font-size: 12pt; color: #{data_conf.color}'>{data_conf.name}={value:.2f}</span>")
                else:
                    lds.append(f"<span style='font-size: 12pt; color: #{data_conf.color}'>{data_conf.name}=null</span>")
            label_data = ",   ".join(lds)
            label_text = f"{label_data}  ({label_point})"
            self.label.setText(label_text)
            # logger.debug(f"label text | {label_text}")
            self.vLine.setPos(x)
            self.hLine.setPos(y)
