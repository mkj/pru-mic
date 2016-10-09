import numpy as np
import copy
import pickle

class Tracks(object):
    VERSION = 3
    def __init__(self, tracks, srate, start = 0.0):
        """ Creates a frame holding tracks and metadata.

        tracks is an array shaped as [tracks, nsamps], or a 1d array. it may be modified. Can be None.
        srate is sample rate in hz
        start time is in seconds
        """
        if tracks is None:
            tracks = np.ndarray([])
        self.set_tracks(tracks)
        self.srate = srate
        self.start = start
        # for pickling
        self.version = self.VERSION

    def __repr__(self):
        return 'Tracks(nt %d, ns %d, srate %d start %f)' % (self.nt, self.ns, self.srate, self.start)

    @property
    def nt(self):
        if self.tracks.shape == ():
            return 0
        return self.tracks.shape[0]

    @property
    def ns(self):
        if self.tracks.shape == ():
            return 0
        return self.tracks.shape[1]

    @property
    def end(self):
        return self.start + (self.ns-1)/self.srate

    def set_tracks(self, tracks):
        self.tracks = tracks.copy()
        print(self.tracks.shape)
        if self.tracks.shape == ():
            # empty
            self.names = []
        else:
            if len(self.tracks.shape) == 1:
                # 1d array
                self.tracks.shape = (1, self.tracks.shape[0])
            self.names = ['t%d' % n for n in range(self.nt)]
        return self

    def add(self, track, name):
        if self.nt == 0:
            self.set_tracks(track)
            self.names[0] = name
        else:
            ex = np.zeros(self.ns)
            n = min(self.ns, len(track))
            ex[:n] = track[:n]
            ex.shape = (1, ex.shape[0])
            self.tracks = np.append(self.tracks, ex, 0)
            self.names.append(name)

    def copy(self):
        return copy.deepcopy(self)

    def each(self, fn):
        for t in range(self.nt):
            self.tracks[t,:] = fn(self.tracks[t,:])
        return self

    @classmethod
    def load(cls, fn):
        fr = pickle.load(open(fn, 'rb'))
        if type(fr) is not cls:
            raise ValueError("Wasn't a Frame object")
        if fr.version != cls.VERSION:
            raise ValueError("Frame object is a different version (%s, expect %d)" % 
                (fr.version, cls.VERSION))
        return fr

    def save(self, fn):
        with open(fn, 'wb') as f:
            pickle.dump(self, f, -1)


def main():
    t = Tracks(None, 1000, 0)

    n = np.zeros(5)
    n[:] = range(5)
    t.add(n, 'five')
    n = np.zeros(4)
    n[:] = range(4)
    t.add(n[:5], 'four')
    n = np.zeros(6)
    n[:] = range(6)
    t.add(n, 'six')
    print(t.tracks)

if __name__ == '__main__':
    main()




