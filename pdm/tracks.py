import copy
import pickle

class Tracks(object):
    VERSION = 2
    def __init__(self, tracks, srate, start = 0.0):
        """ Creates a frame holding tracks and metadata.

        tracks is an array shaped as [tracks, nsamps], or a 1d array. it may be modified.
        srate is sample rate in hz
        start time is in seconds
        """
        self.set_tracks(tracks)
        self.srate = srate
        self.start = start
        # for pickling
        self.version = self.VERSION

    def __repr__(self):
        return 'Tracks(nt %d, ns %d, srate %d start %f)' % (self.nt, self.ns, self.srate, self.start)

    def set_tracks(self, tracks):
        self.tracks = tracks
        if len(tracks.shape) == 1:
            # 1d array
            tracks.shape = (1, tracks.shape[0])
        self.nt = tracks.shape[0]
        self.ns = tracks.shape[1]
        return self

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




