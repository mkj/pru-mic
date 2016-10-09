#!../venv/bin/python

import numpy as np
import scipy.signal, scipy, scipy.fftpack

import wiggle
import tracks
import decon1

def syn1():
    imp = [100,270,413,500]
    s1 = np.zeros(1000)

    imp = [3, 7]
    s1 = np.zeros(13)

    for i in imp:
        s1[i] = 1

    c = np.array([0,1,-2,-1.5,-1,-0.2,0.3,1,0.5,0])

    cv = scipy.signal.fftconvolve(s1, c)
    cv = cv

    db = {}
    sp = decon1.spike(cv, db)


    # division
    zp = np.zeros(len(cv))
    zp[:len(c)] = c
    div = scipy.fftpack.fft(cv) / scipy.fftpack.fft(zp)
    div = scipy.ifft(div).real

    auc1 = scipy.signal.correlate(cv, cv)

    t = tracks.Tracks(None, 1000, 0)
    t.add(auc1, 'auc1')
    t.add(s1, 's1')
    t.add(c, 'real wavelet')
    t.add(cv, 'convolved')
    t.add(db['auc'], 'autoc')
    t.add(db['wl'].real, 'decon wavelet')
    t.add(sp.real, 'spike')
    t.add(div.real, 'freqdiv')

    # normalise
    t.each(lambda tr: tr / np.abs(tr).max())
    #t.tracks /= np.max(np.abs(t.tracks))

    print("\n".join("%d %s" % x for x in enumerate(t.names, 1)))
    wiggle.wiggle(t)

def main():
    syn1()

if __name__ == '__main__':
    main()
