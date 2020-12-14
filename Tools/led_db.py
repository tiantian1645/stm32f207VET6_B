from pony.orm import Database, PrimaryKey, Required, IntArray, db_session, desc
from datetime import datetime
import csv

db = Database()


class SampleRecord(db.Entity):
    id = PrimaryKey(int, auto=True)
    label = Required(str)
    occur = Required(datetime, default=lambda: datetime.utcnow())
    led = Required(str)
    channel = Required(int)
    white_pd = Required(IntArray)
    gray_pd = Required(IntArray)
    od = Required(IntArray)


@db_session
def creat_new_record(label, occur, led, channel, white_pd, gray_pd, od):
    sr = SampleRecord(label=label, occur=occur, led=led, channel=channel, white_pd=white_pd, gray_pd=gray_pd, od=od)
    return sr.to_dict()


@db_session
def fetch_sample_record(start, size, reverse=False):
    results = []
    if not reverse:
        for sr in SampleRecord.select().order_by(desc(SampleRecord.id))[:size]:
            results.append(sr.to_dict())
    else:
        for sr in SampleRecord.select().order_by(desc(SampleRecord.id))[:size]:
            results.append(sr.to_dict())
    return results


@db_session
def dump_all_record(file_path):
    try:
        with open(file_path, mode="w", newline="") as csv_file:
            fieldnames = ["id", "label", "occur", "led", "channel", "white_pd", "gray_pd", "od"]
            writer = csv.DictWriter(csv_file, fieldnames=fieldnames)

            writer.writeheader()
            for sr in SampleRecord.select():
                writer.writerow(sr.to_dict())
            return True
    except Exception:
        return False


@db_session
def update_label(sr_id, new_label):
    sr = SampleRecord.get(id=sr_id)
    if sr:
        sr.label = new_label
        return sr.to_dict()
    return None


def led_db_init(dp_path):
    db.bind(provider="sqlite", filename=dp_path, create_db=True)
    db.generate_mapping(create_tables=True)
