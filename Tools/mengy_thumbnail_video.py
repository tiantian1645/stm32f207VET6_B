import ffmpeg
import os
from loguru import logger


def gen_thumbnail(file_path, ss, op):
    out, err = ffmpeg.input(file_path, ss=ss).output(op, vframes=1).overwrite_output().run(capture_stdout=True, capture_stderr=True)
    if err:
        logger.error(f"error in generate thumbnail in | {file_path} | {ss} | {err}")
    else:
        logger.success(f"generate thumbnail success | {op}")
    return len(err) == 0


def gen_thumbnail_from_video(file_path, start=0, duration=3, interval=10, fps=60):
    if not os.path.isfile(file_path):
        logger.error(f"invalid file path | {file_path}")
        return False
    probe = ffmpeg.probe(file_path)
    fps = float(probe.get("format", {}).get("tags", {}).get("com.android.capture.fps", fps))
    parent_dir = os.path.split(file_path)[0]
    dir_name = os.path.splitext(file_path)[0]
    dir_path = os.path.join(parent_dir, dir_name)
    os.makedirs(dir_path, exist_ok=True)
    for i in range(6):
        rs = start + i * interval
        for n in range(duration * int(fps)):
            rrs = rs + n * 1 / fps
            gen_thumbnail(file_path, rrs, os.path.join(dir_path, f"{rrs:02.3f}.jpg"))

