import pickle

class Frame(object):
    def __init__(self, tracks, srate, start = 0.0):
        """ Creates a frame holding tracks and metadata.

        tracks is an array shaped as [tracks, nsamps].
        srate is sample rate in hz
        start time is in floating point microsecs 
        """
        self.tracks = tracks
        self.nt = tracks.shape[0]
        self.ns = tracks.shape[1]
        self.srate = srate
        # for pickling
        self.version = 1

    @classmethod
    def load(cls, fn):
        fr = pickle.load(open(fn, 'rb'))
        if type(fr) is not cls:
            raise ValueError("Wasn't a Frame object")
        return fr

    def save(self, fn):
        with open(fn, 'wb') as f:
            pickle.dump(self, f, -1)




