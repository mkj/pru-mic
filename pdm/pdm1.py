#!../venv/bin/python

import struct
import sys
import itertools
import wave
import io
import argparse

import scipy.signal
import numpy as np
import numba
import matplotlib.pyplot as plt

import wiggle
import cic
import player


NSTREAMS = 8
MEG = 1024*1024

F = 4e6

INBLOCK_MS = 5

DEFAULTBOOST=2

np.set_printoptions(threshold=np.inf)

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

def decodestream(f, chunk):
    decoders = [decimater() for _ in range(NSTREAMS)]
    for a in demuxstream(f, chunk):
        yield [decode(x) for (decode, x) in zip(decoders, a)]

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
    inchunk = int(rate * INBLOCK_MS / 1000.0)*decim

    w = openwav(wavfn, newrate) if wavfn else None

    if args.live:
        block = inchunk // decim
        play = player.Player(newrate, inchunk//decim, args.stream)
    else:
        play = None

    ns = len(args.channel)
    if args.bitex:
        ins = bitexstream(inf, inchunk)
    else:
        ins = demuxstream(inf, inchunk)
        ns = NSTREAMS

    outsamps = np.ndarray([ns, inchunk // decim])
    decoders = [cic.cic_n4m2(decim) for _ in args.channel]

    plotsamps = np.ndarray([ns, 0])

    for chunk, insamps in enumerate(ins):
        first = (chunk == 0)

        if first and args.stats:
            print("insamps mean %f rms %f" % (np.mean(insamps), np.mean(insamps**2)**0.5))
            print("insamps min %f max %f" % (np.min(insamps), np.max(insamps)))
            maxamp = decoder.getmaxamp()
            maxbits = np.log2(maxamp)
            print("wav rate %f, cic rate %f, maxbits %.1f, maxamp %f DECIM %d, WAVDIV %f chunk %d" % (newrate, rate, maxbits, maxamp, decim, wavdiv, inchunk))


        n = insamps.shape[0]
        l = insamps.shape[1]

        for i, ch in enumerate(args.channel):
            decoders[i].go(insamps[ch,:], outsamps[i,:])

        ls = outsamps.sum(0) / float(ns)

        #ls = np.sign(outsamps) * np.log(np.abs(outsamps))

        maxabs = minabs = rms = mean = 1
        if args.stats:
            maxabs = np.max(np.abs(ls))
            minabs = np.min(np.abs(ls))
            rms = np.sqrt(np.mean((ls-mean)**2))
            mean = np.mean(ls)

        scale = 2**15
        scale *= scaleboost*DEFAULTBOOST
        scaled = ls * scale

        if first and args.stats:
            print("rms %f, scale %f, mean %f maxabs %f minabs %f" % (rms, scale, mean, maxabs, minabs))
        if w or play:
            wavbuf = scaled.astype('int16').tobytes()
        if w:
            w.writeframes(wavbuf)
        if play:
            play.push(wavbuf)
        if doplot:
            plotsamps = np.append(plotsamps, outsamps, 1)

    if play:
        play.flush()

    if doplot:
        wiggle.wiggle(wiggle.Frame(plotsamps, rate))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('-d', '--decim', type=int, default=128)
    parser.add_argument('-w', '--wavdiv', type=float, default=1)
    parser.add_argument('-s', '--scaleboost', type=float, default=1)
    parser.add_argument('-o', '--output', type=str, metavar='wavfile', nargs='?')
    parser.add_argument('-i', '--input', type=str, metavar='infile', nargs='?')
    parser.add_argument('-p', '--plot', action='store_true')
    parser.add_argument('-l', '--live', action='store_true')
    parser.add_argument('--stream', help="Low latency streaming, input must be realtime speed", action='store_true')
    parser.add_argument('-c', '--channel')
    parser.add_argument('--bitex', help="Single bit input stream", action='store_true')
    parser.add_argument('--stats', action="store_true")
    args = parser.parse_args()

    if args.input:
        f = open(args.input, 'rb')
    else:
        f = sys.stdin.buffer

    if args.output and not args.output.endswith('.wav'):
        args.output += '.wav'

    if args.wavdiv != 1 and args.live:
        print("--wavdiv doesn't make sense with --live")
        #sys.exit(1)

    if args.channel:
        if args.channel == '.':
            args.channel = list(range(NSTREAMS))
        else:
            args.channel = [int(x.strip()) for x in args.channel.split(',')]
    else:
        args.channel = [0]

    # fix up channel numbers - unpackbits() has big endian bytes ????
    args.channel = [NSTREAMS-1-c for c in args.channel]


    run_cic1(args, f, args.decim, args.wavdiv, args.scaleboost, args.plot, args.output)


if __name__ == '__main__':
    main()

