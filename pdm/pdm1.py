#!../venv/bin/python

import struct
import sys
import itertools
import wave
import io
import argparse
import queue

import scipy.signal
import numpy as np
import numba
import matplotlib.pyplot as plt
import sounddevice

import wiggle
import cic


NSTREAMS = 8
MEG = 1024*1024

TESTMIC = 2 # data2

F = 4e6

INBLOCK_MS = 10
INCHUNK = int(F * INBLOCK_MS / 1000.0)

np.set_printoptions(threshold=np.inf)

class Player(object):
    def __init__(self, rate, chunk, chans=1):
        self.stream = sounddevice.RawStream(samplerate = int(rate), channels = chans,
            callback = self._callback, dtype='int16', blocksize=chunk)
        self.queue = queue.Queue()

    def push(self, indata):
        self.queue.put(indata)

    def _callback(self, indata, outdata, frames, time, status):
        print("cb")
        if status:
            print(status, flush=True)
        outdata[:] = self.queue.get_nowait()

    def flush(self):
        print("flushing")
        while not self.queue.empty():
            sounddevice.sleep(1000)
        print("flushed")


def demux(b):
    """ demultiplexes b """
    b.shape = (1,b.shape[0])
    bits = np.unpackbits(b, axis=0)
    return np.array(bits, dtype=np.int32)

def demuxstream(f, chunk):
    """ yields arrays of bits """
    while True:
        bl = f.read(chunk)
        if len(bl) != chunk:
            return
        r = np.fromstring(bl, count=chunk, dtype=np.uint8)
        yield demux(r)

def bitexstream(f, chunk):
    """ yields arrays of bits """
    assert(chunk % 8 == 0)
    bytechunk = chunk // 8
    while True:
        bl = f.read(bytechunk)
        if len(bl) != bytechunk:
            return
        r = np.fromstring(bl, count=bytechunk, dtype=np.uint8)
        yield np.unpackbits(r)

class sumdecoder(object):
    def __call__(self, s):
        return np.sum(s)

class decimater(object):
    def __init__(self, decim=128):
        self.decim = decim
    def __call__(self, s):
        newl = s.shape[0] / self.decim
        #return scipy.signal.resample(s, newl)
        k = s
        k = scipy.signal.decimate(k, 4, 30, 'fir', zero_phase=False)
        k = scipy.signal.decimate(k, 4, 30, 'fir', zero_phase=False)
        k = scipy.signal.decimate(k, 8, 30, 'fir', zero_phase=False)
        #k = scipy.signal.decimate(k, 8, 50, 'fir', zero_phase=True)
        return k

def decodestream(f, chunk):
    decoders = [decimater() for _ in range(NSTREAMS)]
    for a in demuxstream(f, chunk):
        yield [decode(x) for (decode, x) in zip(decoders, a)]

def decodeone(f, chunk, decim):
    decoder = decimater(decim)
    for a in demuxstream(f, chunk):
        yield decoder(a[NSTREAMS-1-TESTMIC])

def demuxone(f, chunk, mic):
    for a in demuxstream(f, chunk):
        yield a[NSTREAMS-1-mic]

def openwav(fn, rate):
    w = wave.open(fn, 'wb')
    w.setnchannels(1)
    w.setsampwidth(2)
    w.setframerate(rate)
    return w

def frombytefile(f):
    return np.fromfile(f, dtype=np.uint8)

def run_cic1(args, inf, decim, wavdiv, scaleboost, doplot, wavfn = None):

    rate = F/decim
    newrate = rate / wavdiv
    w = openwav(wavfn, newrate) if wavfn else None

    play = Player(newrate, INCHUNK)

    decoder = cic.cic_n4m2(decim)

    if args.bitex:
        ins = bitexstream(inf, INCHUNK)
    else:
        ins = demuxone(inf, INCHUNK, TESTMIC)

    for chunk, insamps in enumerate(ins):
        first = (chunk == 0)

        if first:
            print("insamps mean %f rms %f" % (np.mean(insamps), np.mean(insamps**2)**0.5))
            print("insamps min %f max %f" % (np.min(insamps), np.max(insamps)))
            maxamp = decoder.getmaxamp()
            maxbits = np.log2(maxamp)
            print("wav rate %f, cic rate %f, maxbits %.1f, maxamp %f DECIM %d, WAVDIV %f chunk %d" % (newrate, rate, maxbits, maxamp, decim, wavdiv, INCHUNK))

        l = insamps.shape[0]
        outsamps = np.ndarray(l // decim)
        decoder.go(insamps, outsamps)

        ls = outsamps
        #ls = np.sign(outsamps) * np.log(np.abs(outsamps))

        mean = np.mean(ls)
        rms = np.sqrt(np.mean((ls-mean)**2))
        scale = 2**15
        scale *= scaleboost
        scaled = ls * scale
        maxabs = np.max(np.abs(ls))
        minabs = np.min(np.abs(ls))

        if first:
            print("rms %f, scale %f, mean %f maxabs %f minabs %f" % (rms, scale, mean, maxabs, minabs))
            if doplot:
                fig = plt.figure(1)
                plt.plot(ls)
                plt.show()

        if w or play:
            wavbuf = scaled.astype('int16').tobytes()
        if w:
            w.writeframes(wavbuf)
        if play:
            play.push(wavbuf)

    if play:
        play.flush()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--decim', type=int, default=128)
    parser.add_argument('-w', '--wavdiv', type=float, default=1)
    parser.add_argument('-s', '--scaleboost', type=int, default=2)
    parser.add_argument('-o', '--output', type=str, metavar='wavfile', nargs='?')
    parser.add_argument('-i', '--input', type=str, metavar='infile', nargs='?')
    parser.add_argument('-p', '--plot', action='store_true')
    parser.add_argument('-l', '--live', action='store_true')
    parser.add_argument('--bitex', help="Single bit input stream", action='store_true')
    args = parser.parse_args()

    if args.input:
        f = open(args.input, 'rb')
    else:
        f = sys.stdin.buffer

    if args.output and not args.output.endswith('.wav'):
        args.output += '.wav'


    run_cic1(args, f, args.decim, args.wavdiv, args.scaleboost, args.plot, args.output)


if __name__ == '__main__':
    main()

