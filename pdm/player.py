import sounddevice
import queue

class Player(object):
    def __init__(self, rate, chunk):
        self.stream = sounddevice.RawOutputStream(samplerate = int(rate), channels = 1,
            callback = self._callback, dtype='int16', blocksize=chunk)
        self.queue = queue.Queue(20)
        self.started = False

    def push(self, indata):
        try:
            self.queue.put_nowait(indata)
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
        print("flushing")
        with self.stream:
            while not self.queue.empty():
                print("sleep. queue %s" % self.queue.qsize())
                sounddevice.sleep(1000)
            print("flushed")


