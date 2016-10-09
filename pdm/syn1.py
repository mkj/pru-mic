#!../venv/bin/python

import numpy as np
import scipy.signal, scipy, scipy.fftpack

import wiggle
import tracks
import decon1

def syn1():
    imp = [100,270,413,500]
    s1 = np.zeros(1000)

    imp = [3]
    s1 = np.zeros(10)

    for i in imp:
        s1[i] = 1

    c = np.array([0,1,-2,-1.5,-1,-0.2,0.3,1,0.5,0])

    cv = scipy.signal.fftconvolve(s1, c)
    cv = cv

    sp = decon1.spike(cv)

    tr = np.zeros((3,len(s1)))

    tr[0,:] = s1
    tr[1,:] = cv[:len(s1)]
    tr[2,:] = sp[:len(s1)]

    rms = np.abs(tr).max(1)
    #rms = ((tr*tr).sum(1) / tr.shape[1]) ** 0.5
    print(rms)

    tr = tr / rms[:,np.newaxis]
    tr = tr * 0.2

    print(tr)

    print(np.abs(tr).max(1))

    fr = tracks.Tracks(tr, 1000, 0)

    wiggle.wiggle(fr)

def main():
    syn1()

if __name__ == '__main__':
    main()
