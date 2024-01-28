import numpy as np
import cv2
import time
from rtmp_streaming import StreamerConfig, Streamer

import pyaudio
from threading import Thread, Event
from queue import Queue

def _read_frame(stream,  queue, chunk):
    while True:
        frame = stream.read(chunk, exception_on_overflow=False)
        frame = np.frombuffer(frame, dtype=np.int16).astype(np.float32) / 32767 # [chunk]
        queue.put(frame)


def main():

    cap = cv2.VideoCapture(0)
    #cap.set(cv2.CAP_PROP_CONVERT_RGB,0)
    ret, frame = cap.read()
    print(frame.shape)

    sc = StreamerConfig()
    sc.source_width = frame.shape[1]
    sc.source_height = frame.shape[0]
    sc.stream_width = 640
    sc.stream_height = 480
    sc.stream_fps = 25
    sc.stream_bitrate = 1000000
    sc.stream_profile = 'main' #'high444' # 'main'
    sc.audio_channel = 1
    sc.sample_rate = 16000
    sc.stream_server = 'rtmp://localhost/live/livestream'  #'test.mp4'


    streamer = Streamer()
    streamer.init(sc)
    streamer.enable_av_debug_log()


    fps = 50
    sample_rate=16000
    chunk = sample_rate // fps # 320 samples per chunk (20ms * 16000 / 1000)
    audio_instance = pyaudio.PyAudio()
    input_stream = audio_instance.open(format=pyaudio.paInt16, channels=1, rate=sample_rate, input=True, output=False, frames_per_buffer=chunk)
    queue = Queue()
    process_read_frame = Thread(target=_read_frame, args=(input_stream, queue, chunk))
    process_read_frame.start()
        
    prev = time.time()

    show_cap = True

    while(True):
        ret, frame = cap.read()
        now = time.time()
        duration = now - prev
        streamer.stream_frame(cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)) #frame.flatten()
        while(not queue.empty()):
            audio_frame = queue.get()
            streamer.stream_frame_audio(audio_frame)
        prev = now
        if show_cap:
            cv2.imshow('frame', frame)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break


    cap.release()
    if show_cap:
        cv2.destroyAllWindows()



if __name__ == "__main__":
    main()
    



