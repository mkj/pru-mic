import sounddevice
import queue

class Player(object):
    def __init__(self, rate, chunk, streaming):
        self.stream = sounddevice.RawOutputStream(samplerate = int(rate), channels = 1,
            callback = self._callback, dtype='int16', blocksize=chunk)
        self.streaming = streaming
        if streaming:
            # short queue
            self.queue = queue.Queue(20)
        else:
            self.queue = queue.Queue(200)

        self.started = False

    def push(self, indata):
        try:
            if self.streaming:
                self.queue.put_nowait(indata)
            else:
                self.queue.put(indata)
        except queue.Full:
            print("queue full")
            try:
                while True:
                    self.queue.get_nowait()
            except queue.Empty:
                pass
            return

        if not self.started:
            self.stream.start()
            self.started = True

    def _callback(self, outdata, frames, time, status):
        if status:
            print(status, flush=True)
        try:
            outdata[:] = self.queue.get_nowait()
        except queue.Empty:
            outdata[:] = bytearray(len(outdata))

    def flush(self):
        with self.stream:
            while not self.queue.empty():
                sounddevice.sleep(1000)


