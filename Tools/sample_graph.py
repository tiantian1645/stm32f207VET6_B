import loguru
import pyqtgraph as pg
from collections import namedtuple

logger = loguru.logger
DataConf = namedtuple("DataConf", "name data color")
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

    def plot_data(self, data=None, *args, **kwargs):
        if data is None:
            logger.error("data is None could not be added")
            return
        dl = len(self.plot_data_confs)
        color = kwargs.get("color", DEFAULT_COLORS[dl % len(DEFAULT_COLORS)])
        data_conf = DataConf(name=kwargs.get("name", f"y{dl + 1}"), data=data, color=color)
        self.plot_data_confs.append(data_conf)
        # self.matplot_plots.append(self.plot_wg.plot([], name=f"B{k+1}", pen=mkPen(color=color), symbol=symbol, symbolSize=5, symbolBrush=(color)))
        self.plot.plot(data, pen=pg.mkPen(color=color), symbolSize=5, symbolBrush=(color))

    def clear_plot(self):
        self.plot.clear()
        self.plot_data_confs.clear()
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
            logger.debug(f"hit index {index} by x {x}")
            label_text_x = f"[<span style='font-size: 12pt'>x={x:0.1f}, <span style='font-size: 12pt'>y={y:0.1f}]"
            tys = []
            for data_conf in self.plot_data_confs:
                if data_conf.data is not None and index >= 0 and index < len(data_conf.data):
                    tys.append(f"<span style='color: #{data_conf.color}'>{data_conf.name}={data_conf.data[index]:d}</span>")
                else:
                    tys.append(f"<span style='color: #{data_conf.color}'>{data_conf.name}=overflow</span>")
            label_text_y = ",   ".join(tys)
            label_text = f"{label_text_x} {label_text_y}"
            self.label.setText(label_text)
            # logger.debug(f"label text | {label_text}")
            self.vLine.setPos(x)
            self.hLine.setPos(y)
