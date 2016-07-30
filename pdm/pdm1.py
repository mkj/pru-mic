#!../venv/bin/python

import struct
import sys
import itertools
import numpy as np
import numba

NSTREAMS = 8

np.set_printoptions(threshold=np.inf)

def demux(b):
	""" demultiplexes b """
	b.shape = (1,b.shape[0])
	return np.unpackbits(b, axis=0)

def demuxstream(f, chunk):
	""" yields arrays of bits """
	while True:
		r = np.fromfile(f, count=chunk, dtype=np.uint8, )
		if r.size != chunk:
			return
		yield demux(r)

class decoder(object):
	def __call__(self, s):
		return np.sum(s)

def decodestream(f, chunk):
	decoders = [decoder() for _ in range(NSTREAMS)]
	for a in demuxstream(sys.stdin, chunk):
		yield [decode(x) for (decode, x) in zip(decoders, a)]

for n in decodestream(sys.stdin, 64):
	print(n)
