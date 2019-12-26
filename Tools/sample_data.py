from sqlalchemy import create_engine
from sqlalchemy import Column, Integer, String, DATETIME, BLOB, Enum
from sqlalchemy.ext.declarative import declarative_base
from bytes_helper import bytesPuttyPrint
from datetime import datetime
from sqlalchemy.orm import sessionmaker
from sqlalchemy import ForeignKey
from sqlalchemy.orm import relationship
import loguru
import enum

logger = loguru.logger
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
