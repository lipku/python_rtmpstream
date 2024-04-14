import numpy as np
import cv2
import time
from rtmp_streaming import StreamerConfig, Streamer

from queue import Queue

import soundfile as sf
import resampy

video_file_path = "./test.mp4"
audio_file_path = "./test_16k.wav"
rtmp_server = "rtmp://localhost:1935/live/livestream"

queue = Queue()
fps = 25
sample_rate = 16000
chunk = 320  # 320 samples per chunk (20ms * 16000 / 1000)


def create_bytes_stream():
    stream, sr = sf.read(audio_file_path)
    print(f'[INFO]tts audio stream {sr}: {stream.shape}')
    stream = stream.astype(np.float32)

    if stream.ndim > 1:
        print(f'[WARN] audio has {stream.shape[1]} channels, only use the first.')
        stream = stream[:, 0]

    if sr != sample_rate and stream.shape[0] > 0:
        print(f'[WARN] audio sample rate is {sr}, resampling into {sample_rate}.')
        stream = resampy.resample(x=stream, sr_orig=sr, sr_new=sample_rate)

    return stream


def push_audio():
    wav = create_bytes_stream()
    start_idx = 0
    while start_idx < wav.shape[0]:
        if start_idx + chunk > wav.shape[0]:
            af = wav[start_idx:wav.shape[0]]
            start_idx = 0
        else:
            af = wav[start_idx:start_idx + chunk]
            start_idx = start_idx + chunk
        queue.put(af)


cap = cv2.VideoCapture(video_file_path)
ret, frame = cap.read()

sc = StreamerConfig()
sc.source_width = frame.shape[1]
sc.source_height = frame.shape[0]
sc.stream_width = frame.shape[1]
sc.stream_height = frame.shape[0]
sc.stream_fps = fps
sc.stream_bitrate = 4000000
sc.stream_profile = 'main'
sc.audio_channel = 1
sc.sample_rate = sample_rate
sc.stream_server = rtmp_server

streamer = Streamer()
streamer.init(sc)
streamer.enable_av_debug_log()

push_audio()

while True:
    ret, frame = cap.read()
    if not ret:
        break

    t = time.time()

    streamer.stream_frame(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB))
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
