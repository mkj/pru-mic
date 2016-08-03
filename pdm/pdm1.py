#!../venv/bin/python

import struct
import sys
import itertools
import wave
import io

import scipy.signal
import numpy as np
import numba
import matplotlib.pyplot as plt

import wiggle


NSTREAMS = 8

TESTMIC = 2 # data2

F = 1e6


np.set_printoptions(threshold=np.inf)

def demux(b):
	""" demultiplexes b """
	b.shape = (1,b.shape[0])
	bits = np.unpackbits(b, axis=0)
	return np.array(bits, dtype=np.int32)

def demuxstream(f, chunk):
	""" yields arrays of bits """
	while True:
		r = np.fromfile(f, count=chunk, dtype=np.uint8)
		if r.size != chunk:
			return
		yield demux(r)

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
	for a in demuxstream(sys.stdin, chunk):
		yield [decode(x) for (decode, x) in zip(decoders, a)]

def decodeone(f, chunk, decim):
	decoder = decimater(decim)
	for a in demuxstream(sys.stdin, chunk):
		yield decoder(a[NSTREAMS-1-TESTMIC])

def demuxone(f, chunk):
	for a in demuxstream(sys.stdin, chunk):
		yield a[NSTREAMS-1-TESTMIC]

#for n in decodeone(sys.stdin, 128000):
#	print(n)

def writewav(fn, sampbuf, rate):
	with wave.open(fn, 'wb') as w:
		w.setnchannels(1)
		w.setsampwidth(2)
		w.setframerate(rate)

		w.writeframes(sampbuf)

def frombytefile(f):
	return np.fromfile(f, dtype=np.uint8)

decim = 128

#insamps = next(demuxone(sys.stdin, 128000)) * 2 - 1
insamps = frombytefile(sys.stdin)
print("insamps mean %f rms %f" % (np.mean(insamps), np.mean(insamps**2)**0.5))
print("insamps min %f max %f" % (np.min(insamps), np.max(insamps)))

print(insamps[:200])
insamps.tofile('i1.dat', '')

decoder = decimater(decim)
samps = decoder(insamps)

rate = F/decim
print("sample rate %s" % rate)

#ls = np.sign(samps) * np.log(np.abs(samps))
ls = samps
#print(ls)

mean = np.mean(ls)
rms = np.sqrt(np.mean((ls-mean)**2))
scale = 32768 * 0.7 / rms
scaled = (ls - mean) * scale

print("rms %f, scale %f, mean %f" % (rms, scale, mean))

#ls = ls * scale

wavbuf = scaled.astype('int16').tobytes()

with open('test.dat', 'wb') as f:
	f.write(wavbuf)

writewav('out1.wav', wavbuf, rate)

fig = plt.figure(1)
plt.plot(ls)
plt.show()
