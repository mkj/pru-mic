#!../venv/bin/python
import numpy as np
import scipy.signal, scipy, scipy.fftpack

import wiggle
import tracks
import decon1

def real1():
    f = tracks.Tracks.load('f4.out')
    def d(t):
        return np.abs(scipy.fftpack.fft(t))
    f.each(d)
    #f.each(decon1.spike)
    f.set_tracks(f.tracks[:,:f.ns/6])
    f.tracks *= 0.1

    wiggle.wiggle(f)

def main():
    real1()

if __name__ == '__main__':
    main()
