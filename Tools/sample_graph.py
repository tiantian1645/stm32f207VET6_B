from bisect import bisect_left
from collections import namedtuple
from itertools import chain

import loguru
import pyqtgraph as pg
import stackprinter
from numpy import ones, vstack
from numpy.linalg import lstsq
from PyQt5 import QtCore

logger = loguru.logger
DataConf = namedtuple("DataConf", "name data color plot")
DEFAULT_COLORS = (
    "BB86FC",
    "FF0266",
    "FF7597",
    "00a300",
    "2451c4",
    "F39C12",
    "ff0097",
    "1e7145",
    "03DAC5",
    "9f00a7",
    "e3eb10",
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


def line_equation_from_two_points(p1, p2):
    x_coords, y_coords = zip(p1, p2)
    A = vstack([x_coords, ones(len(x_coords))]).T
    m, c = lstsq(A, y_coords, rcond=None)[0]
    # logger.debug(f"Line Solution is y = {m}x + {c}")
    return m, c


def point_line_equation_map(channel_points, standard_points, origin_data):
    if origin_data <= channel_points[1]:
        p1 = (channel_points[0], standard_points[0])
        p2 = (channel_points[1], standard_points[1])
    elif origin_data > channel_points[-2]:
        p1 = (channel_points[-2], standard_points[-2])
        p2 = (channel_points[-1], standard_points[-1])
    else:
        for idx, c in enumerate(channel_points):
            if origin_data < c:
                p1 = (channel_points[idx - 1], standard_points[idx - 1])
                p2 = (channel_points[idx], standard_points[idx])
                break
        else:
            logger.error(f"could not find p1 p2 | origin_data {origin_data} | channel_points {channel_points}")
            return origin_data
    m, c = line_equation_from_two_points(p1, p2)
    result = m * origin_data + c
    return result


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

    def _check_duplicated_name(self, name):
        result = 0
        color = None
        for data_conf in self.plot_data_confs:
            if data_conf.name == name:
                result += 1
                if color is None:
                    color = data_conf.color
        return result, color

    def plot_data_new(self, data=None, name=None, color=None):
        if data is None:
            # logger.warning("data is None new as list")
            data = []
        dl = len(self.plot_data_confs)
        if color is None:
            color = DEFAULT_COLORS[dl % len(DEFAULT_COLORS)]
        if name is None:
            name = f"y{dl + 1}"
        duplicated, duplicated_color = self._check_duplicated_name(name)
        # logger.debug(f"dl {dl} | color {color} | duplicated_color {duplicated_color}")
        if duplicated_color:
            color = duplicated_color
        if duplicated == 0:
            pen = pg.mkPen(color=color)
        elif duplicated == 1:
            pen = pg.mkPen(color=color, style=QtCore.Qt.DashLine)
        elif duplicated == 2:
            pen = pg.mkPen(color=color, style=QtCore.Qt.DashDotLine)
        else:
            pen = pg.mkPen(color=color, style=QtCore.Qt.DashDotDotLine)
        # self.matplot_plots.append(self.plot_wg.plot([], name=f"B{k+1}", pen=mkPen(color=color), symbol=symbol, symbolSize=5, symbolBrush=(color)))
        p = self.plot.plot(data, name=name, pen=pen, symbolSize=5, symbolBrush=(color))
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
            for data_conf in self.plot_data_confs[-24:]:
                if data_conf.data is not None and index >= 0 and index < len(data_conf.data):
                    value = data_conf.data[index]
                    if isinstance(value, int):
                        lds.append(f"<span style='font-size: 12pt; color: #{data_conf.color}'>{data_conf.name}={value:d}</span>,   ")
                    elif isinstance(value, float):
                        lds.append(f"<span style='font-size: 12pt; color: #{data_conf.color}'>{data_conf.name}={value:.2f}</span>,   ")
                else:
                    lds.append(f"<span style='font-size: 12pt; color: #{data_conf.color}'>{data_conf.name}=null</span>,   ")
            lds = list(chain(*[lds[i : i + 12] + [" <br> "] if len(lds[i : i + 12]) == 12 else lds[i : i + 12] for i in range(0, len(lds), 12)]))
            label_data = "".join(lds)
            label_text = f"{label_data}  ({label_point})"
            self.label.setText(label_text)
            # logger.debug(f"label text | {label_text}")
            self.vLine.setPos(x)
            self.hLine.setPos(y)


class TemperatureGraph(SampleGraph):
    def plot_data_new(self, data_x=None, data_y=None, name=None, color=None):
        if data_x is None:
            # logger.warning("data_x is None new as list")
            data_x = []
        if data_y is None:
            # logger.warning("data_y is None new as list")
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


class CC_Graph(SampleGraph):
    def color_tuple_position(self, data_conf, sample):
        if sample <= data_conf.data[1]:
            p = 1
        elif sample > data_conf.data[-2]:
            p = len(data_conf.data) - 1
        else:
            for i in range(len(data_conf.data) - 1, 0, -1):
                if sample > data_conf.data[i]:
                    p = i + 1
                    break
        tt = []
        for po, d in enumerate(data_conf.data):
            if po == p:
                tt.append(f"<span style='font-size: 12pt; color: #FFFFFF'>{sample:7.1f}</span>")
            tt.append(f"<span style='font-size: 12pt; color: #{data_conf.color}'>{d:05d}</span>")
        return ", ".join(tt)

    def mouseMoved(self, evt):
        # logger.debug(f"get evt in mouseMoved | {evt}")
        pos = evt
        if self.plot.sceneBoundingRect().contains(pos):
            mousePoint = self.plot.vb.mapSceneToView(pos)
            x = mousePoint.x()
            y = mousePoint.y()
            index = round(x)
            lds = []
            for data_conf in self.plot_data_confs:
                if data_conf.data is not None and index >= 0 and index < len(data_conf.data):
                    value = data_conf.data[index]
                    lds.append(f"<span style='font-size: 12pt; color: #{data_conf.color}'>{data_conf.name}={value:d}</span>")
                else:
                    lds.append(f"<span style='font-size: 12pt; color: #{data_conf.color}'>{data_conf.name}=null</span>")
            label_data = ",   ".join(lds)

            if x < 1:
                head = 0
            elif x > len(self.plot_data_confs[0].data) - 2:
                head = len(self.plot_data_confs[0].data) - 2
            else:
                head = int(x)
            if not self.plot_data_confs[0].data:
                return
            c0, c1 = self.plot_data_confs[0].data[head : head + 2]
            dxs = []
            cxs = []
            for i in range(6):
                d0, d1 = self.plot_data_confs[i + 1].data[head : head + 2]
                k, b = line_equation_from_two_points((d0, c0), (d1, c1))
                dx = d0 + (x - head) * (d1 - d0)
                cx = k * dx + b
                dxs.append(dx)
                cxs.append(cx)
                # cxf = c0 + (x - head) * (c1 - c0)

            label_point = (
                f"<span style='font-size: 12pt'>({x:.1f}, {y:.1f}) | </span>"
                f"(<span style='font-size: 12pt;'>{cx:.1f} -> </span>"
                f"<span style='font-size: 12pt; color: #{self.plot_data_confs[1].color}'>{dxs[1 - 1]:.1f}, </span>"
                f"<span style='font-size: 12pt; color: #{self.plot_data_confs[2].color}'>{dxs[2 - 1]:.1f}, </span>"
                f"<span style='font-size: 12pt; color: #{self.plot_data_confs[3].color}'>{dxs[3 - 1]:.1f}, </span>"
                f"<span style='font-size: 12pt; color: #{self.plot_data_confs[4].color}'>{dxs[4 - 1]:.1f}, </span>"
                f"<span style='font-size: 12pt; color: #{self.plot_data_confs[5].color}'>{dxs[5 - 1]:.1f}, </span>"
                f"<span style='font-size: 12pt; color: #{self.plot_data_confs[6].color}'>{dxs[6 - 1]:.1f}) </span>"
            )
            label_raw = (
                f"<br>{self.color_tuple_position(self.plot_data_confs[0], cx)}"
                f"<br>{self.color_tuple_position(self.plot_data_confs[1], dxs[1 - 1])}"
                f"<br>{self.color_tuple_position(self.plot_data_confs[2], dxs[2 - 1])}"
                f"<br>{self.color_tuple_position(self.plot_data_confs[3], dxs[3 - 1])}"
                f"<br>{self.color_tuple_position(self.plot_data_confs[4], dxs[4 - 1])}"
                f"<br>{self.color_tuple_position(self.plot_data_confs[5], dxs[5 - 1])}"
                f"<br>{self.color_tuple_position(self.plot_data_confs[6], dxs[6 - 1])}"
            )
            label_text = f"{label_data}  {label_point} \n{label_raw}"
            self.label.setText(label_text)
            # logger.debug(f"label text | {label_text}")
            self.vLine.setPos(x)
            self.hLine.setPos(y)
