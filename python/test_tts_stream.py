import asyncio
from io import BytesIO
import numpy as np
import cv2
import time
from rtmp_streaming import StreamerConfig, Streamer

from queue import Queue
import edge_tts

import soundfile as sf
import resampy

video_file_path = "./test.mp4"
rtmp_server = "rtmp://localhost:1935/live/livestream"

queue = Queue()
fps = 25
sample_rate = 16000
chunk = 320  # 320 samples per chunk (20ms * 16000 / 1000)

input_stream = BytesIO()
voice_name = "zh-CN-YunxiaNeural"
text = "掌握模式识别中涉及的相关概念、算法很重要！真的很重要！"


def create_bytes_stream(byte_stream):
    stream, sr = sf.read(byte_stream)
    print(f'[INFO]tts audio stream {sr}: {stream.shape}')
    stream = stream.astype(np.float32)

    if stream.ndim > 1:
        print(f'[WARN] audio has {stream.shape[1]} channels, only use the first.')
        stream = stream[:, 0]

    if sr != sample_rate and stream.shape[0] > 0:
        print(f'[WARN] audio sample rate is {sr}, resampling into {sample_rate}.')
        stream = resampy.resample(x=stream, sr_orig=sr, sr_new=sample_rate)

    return stream


def push_audio(buffer):
    input_stream.write(buffer)
    if len(buffer) > 0:
        input_stream.seek(0)
        stream = create_bytes_stream(input_stream)
        streamlen = stream.shape[0]
        idx = 0
        while streamlen >= chunk:
            queue.put(stream[idx:idx + chunk])
            streamlen -= chunk
            idx += chunk

        input_stream.seek(0)
        input_stream.truncate()


async def edge(_text):
    communicate = edge_tts.Communicate(_text, voice_name)

    res = None
    async for chunk in communicate.stream():
        if chunk["type"] == "audio":
            if res is None:
                res = chunk["data"]
            else:
                res = res + chunk["data"]
        elif chunk["type"] == "WordBoundary":
            pass
    push_audio(res)


def run_tts(_text):
    asyncio.get_event_loop().run_until_complete(edge(_text))


cap = cv2.VideoCapture(video_file_path)
ret, frame = cap.read()

sc = StreamerConfig()
sc.source_width = frame.shape[1]
sc.source_height = frame.shape[0]
sc.stream_width = frame.shape[1]
sc.stream_height = frame.shape[0]
sc.stream_fps = fps
sc.stream_bitrate = 4000000
sc.stream_profile = 'baseline'
sc.audio_channel = 1
sc.sample_rate = sample_rate
sc.stream_server = rtmp_server

streamer = Streamer()
streamer.init(sc)
streamer.enable_av_debug_log()

run_tts(text)

while True:
    ret, frame = cap.read()
    if not ret:
        break

    t = time.time()

    streamer.stream_frame(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))  # frame.flatten()
    if not queue.empty():
        for i in range(2):
            audio_frame = queue.get()
            streamer.stream_frame_audio(audio_frame)
    else:
        for i in range(2):
            audio_frame = np.zeros(chunk, dtype=np.float32)
            streamer.stream_frame_audio(audio_frame)

    delay = 0.04 - (time.time() - t)
    if delay > 0:
        time.sleep(delay)

cap.release()
cv2.destroyAllWindows()