import enum
from collections import namedtuple
from datetime import datetime
from math import log10
from struct import unpack

from loguru import logger
from more_itertools import divide
from sqlalchemy import BLOB, DATETIME, Column, Enum, ForeignKey, Integer, String, create_engine, desc
from sqlalchemy.ext.declarative import declarative_base
from sqlalchemy.orm import relationship, sessionmaker

from bytes_helper import bytesPuttyPrint

SapmleInfo = namedtuple("SapmleInfo", "label_id label_datetimne label_name label_version label_device_id sample_channel sample_method sample_wave sample_datas")
METHOD_NAMES = ("无项目", "速率法", "终点法", "两点终点法")
WAVES = (610, 550, 405)


Base = declarative_base()


class Label(Base):
    __tablename__ = "labels"

    id = Column(Integer, primary_key=True)
    datetime = Column(DATETIME)
    name = Column(String)
    version = Column(String)
    device_id = Column(String)

    def __repr__(self):
        return f"<Label(id={self.id}, datetime='{self.datetime}', name='{self.name}', version='{self.version}', device_id='{self.device_id}')>"


class WaveEnum(enum.Enum):
    wave_610 = 1
    wave_550 = 2
    wave_405 = 3


class MethodEnum(enum.Enum):
    none = 0
    rate = 1
    end_point = 2
    two_point = 3


SampleSetInfo = namedtuple("SampleSetInfo", "short_name full_name wave method points_num")
SAMPLE_SET_INFOS = (
    SampleSetInfo(short_name="None", full_name="Default", wave=WaveEnum.wave_610, method=MethodEnum.rate, points_num=6),
    SampleSetInfo(short_name="α-AMY", full_name="α-淀粉酶", wave=WaveEnum.wave_405, method=MethodEnum.rate, points_num=12),
    SampleSetInfo(short_name="UREA", full_name="尿素", wave=WaveEnum.wave_610, method=MethodEnum.end_point, points_num=12),
    SampleSetInfo(short_name="UA", full_name="尿酸", wave=WaveEnum.wave_610, method=MethodEnum.end_point, points_num=12),
    SampleSetInfo(short_name="ALB", full_name="白蛋白", wave=WaveEnum.wave_610, method=MethodEnum.end_point, points_num=6),
    SampleSetInfo(short_name="HB", full_name="血红蛋白", wave=WaveEnum.wave_550, method=MethodEnum.end_point, points_num=6),
    SampleSetInfo(short_name="ALT", full_name="丙氨酸氨基转移酶", wave=WaveEnum.wave_610, method=MethodEnum.rate, points_num=15),
    SampleSetInfo(short_name="CHE", full_name="胆碱酯酶", wave=WaveEnum.wave_610, method=MethodEnum.rate, points_num=12),
    SampleSetInfo(short_name="CREA", full_name="肌酐", wave=WaveEnum.wave_610, method=MethodEnum.two_point, points_num=30),
    SampleSetInfo(short_name="LDH", full_name="乳酸脱氢酶", wave=WaveEnum.wave_550, method=MethodEnum.rate, points_num=12),
    SampleSetInfo(short_name="AST", full_name="天门冬氨酸氨基转移酶", wave=WaveEnum.wave_610, method=MethodEnum.rate, points_num=12),
    SampleSetInfo(short_name="ALC", full_name="乙醇", wave=WaveEnum.wave_610, method=MethodEnum.end_point, points_num=12),
    SampleSetInfo(short_name="CK", full_name="肌酸激酶", wave=WaveEnum.wave_550, method=MethodEnum.rate, points_num=18),
    SampleSetInfo(short_name="CKMB", full_name="肌酸激酶同工酶MB", wave=WaveEnum.wave_550, method=MethodEnum.rate, points_num=24),
    SampleSetInfo(short_name="LPS", full_name="脂肪酶", wave=WaveEnum.wave_610, method=MethodEnum.two_point, points_num=18),
    SampleSetInfo(short_name="GLU", full_name="葡萄糖", wave=WaveEnum.wave_610, method=MethodEnum.end_point, points_num=12),
    SampleSetInfo(short_name="α-HBDH", full_name="α-羟丁酸脱氢酶", wave=WaveEnum.wave_550, method=MethodEnum.rate, points_num=12),
    SampleSetInfo(short_name="DAO", full_name="二胺氧化酶", wave=WaveEnum.wave_610, method=MethodEnum.two_point, points_num=30),
    SampleSetInfo(short_name="D-LAC", full_name="D-乳酸", wave=WaveEnum.wave_550, method=MethodEnum.end_point, points_num=12),
    SampleSetInfo(short_name="TP", full_name="总蛋白", wave=WaveEnum.wave_550, method=MethodEnum.end_point, points_num=12),
    SampleSetInfo(short_name="TBA", full_name="总胆汁酸", wave=WaveEnum.wave_550, method=MethodEnum.end_point, points_num=18),
    SampleSetInfo(short_name="TBIL", full_name="总胆红素", wave=WaveEnum.wave_550, method=MethodEnum.end_point, points_num=18),
    SampleSetInfo(short_name="DBIL", full_name="直接胆红素", wave=WaveEnum.wave_550, method=MethodEnum.end_point, points_num=18),
    SampleSetInfo(short_name="G6PD", full_name="葡萄糖-6-磷酸脱氢酶", wave=WaveEnum.wave_550, method=MethodEnum.rate, points_num=12),
)


class SampleData(Base):
    __tablename__ = "sample_datas"

    id = Column(Integer, primary_key=True)
    datetime = Column(DATETIME)
    channel = Column(Integer)
    method = Column(Enum(MethodEnum))
    wave = Column(Enum(WaveEnum))
    total = Column(Integer)
    raw_data = Column(BLOB)
    label_id = Column(Integer, ForeignKey("labels.id"))
    label = relationship("Label", back_populates="sample_datas")

    def __repr__(self):
        return (
            f"<Sample Data(id={self.id},  datetime='{self.datetime}', "
            f"channel={self.channel}, total={self.total}, method={self.method}, wave={self.wave}, raw_data='{bytesPuttyPrint(self.raw_data)}')>"
        )


Label.sample_datas = relationship("SampleData", order_by=SampleData.id, back_populates="label")


class SampleDB:
    def __init__(self, db_url="sqlite:///data/db.sqlite3", echo=False):
        self.engine = create_engine(db_url, echo=echo)
        Base.metadata.create_all(self.engine)
        Session = sessionmaker(bind=self.engine)
        self.session = Session()
        logger.success(f"init db over | {db_url}")
        self._i32 = None

    def build_label(self, dt=None, name="no name", version="no version", device_id="no device id"):
        if dt is None:
            dt = datetime.now()
        label = Label(datetime=dt, name=name, version=version, device_id=device_id)
        logger.debug(f"insert new label | {label}")
        return label

    def _save_data(self, data):
        self.session.add(data)
        self.session.commit()

    def build_sample_data(self, dt, channel, method, wave, total, raw_data):
        sample_data = SampleData(datetime=dt, channel=channel, method=method, wave=wave, total=total, raw_data=raw_data)
        logger.debug(f"insert new sample_data | {sample_data}")
        return sample_data

    def bind_label_sample_data(self, label, sample_data):
        ol = len(label.sample_datas)
        label.sample_datas.append(sample_data)
        self._save_data(label)
        nl = len(label.sample_datas)
        logger.debug(f"bind label and sample_data | {ol} --> {nl} | {label} | {sample_data}")

    def bind_label_sample_datas(self, label, sample_datas):
        ol = len(label.sample_datas)
        for sd in sample_datas:
            label.sample_datas.append(sd)
        self._save_data(label)
        nl = len(label.sample_datas)
        logger.debug(f"bind label and sample_datas | {ol} --> {nl} | {label} | {sample_datas}")

    def get_label_cnt(self):
        return self.session.query(Label).count()

    def get_label_by_index(self, index):
        try:
            return self.session.query(Label).order_by(Label.id)[index]
        except IndexError:
            return None

    def _decode_raw_data(self, total, raw_data):
        if total == 0 or len(raw_data) == 0:
            return []
        elif len(raw_data) / total == 2:
            return [unpack("H", bytes((i)))[0] for i in divide(total, raw_data)]
        elif len(raw_data) / total == 4:
            result = [unpack("I", bytes((i)))[0] for i in divide(total, raw_data)]
            if self._i32 is None:
                self._i32 = result[:]
            else:
                new = result[: len(self._i32)]
                for i, j in zip(self._i32, new):
                    if j != 0:
                        result.append(log10(i / j) * 10000)
                    else:
                        result.append(0)
                self._i32 = None
            return result
        else:
            return []

    def iter_all_data(self, start=0, num=2 ** 32):
        for label in self.session.query(Label).filter(Label.sample_datas.__ne__(None)).order_by(desc(Label.id))[start : start + num]:
            self._i32 = None
            for sample_data in label.sample_datas:
                # no data
                if sample_data.total == 0 or len(sample_data.raw_data) == 0:
                    continue
                else:
                    sample_datas = self._decode_raw_data(sample_data.total, sample_data.raw_data)
                if 0 < sample_data.method.value < len(METHOD_NAMES):
                    sample_method = METHOD_NAMES[sample_data.method.value]
                else:
                    sample_method = f"error-method-{sample_data.method.value}"
                if 0 < sample_data.wave.value <= len(WAVES):
                    sample_wave = WAVES[sample_data.wave.value - 1]
                else:
                    sample_wave = f"error-wave-{sample_data.wave.value}"

                yield SapmleInfo(
                    label_id=label.id,
                    label_datetimne=str(label.datetime),
                    label_name=label.name,
                    label_version=label.version,
                    label_device_id=label.device_id,
                    sample_channel=sample_data.channel,
                    sample_method=sample_method,
                    sample_wave=sample_wave,
                    sample_datas=sample_datas,
                )


if __name__ == "__main__":
    self = SampleDB("sqlite:///data/db.sqlite3")
    lb = self.build_label(name="test", version="0.1", device_id="200-200-300")
    sds = [
        self.build_sample_data(datetime.now(), channel=i, method=MethodEnum(1 + i), wave=WaveEnum(1 + i), total=i * 4, raw_data=b"\x01\x02" * i)
        for i in range(3)
    ]
    self.bind_label_sample_datas(lb, sds)

    # engine = create_engine("sqlite:///:memory:", echo=True)
    engine = create_engine("sqlite:///data/db.sqlite3", echo=True)
    Base.metadata.create_all(engine)
    Session = sessionmaker(bind=engine)
    session = Session()

    new_label = Label(datetime=datetime.now(), name="test", version="0.1")
    new_sample_data = SampleData(datetime=datetime.now(), channel=0, method=0, wave=610, total=2, raw_data=b"\x01\x08")
    new_sample_data_2 = SampleData(datetime=datetime.now(), channel=1, method=0, wave=610, total=2, raw_data=b"\x01\x08")
    new_label.sample_datas = [new_sample_data]
    new_sample_data_2.label = new_label

    session.add(new_label)
    session.new
    session.commit()
