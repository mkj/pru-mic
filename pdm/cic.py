import numpy as np
import numba

spec = (
    ('int', numba.int64[:]),
    ('comb', numba.int64[:]),
    ('last_comb', numba.int64[:]),
    ('last2_comb', numba.int64[:]),
    ('last_int', numba.int64),
    ('last2_int', numba.int64),
    )

@numba.jitclass(spec)
class cic128(object):
    def __init__(self):
        self.int = np.zeros(4, np.int64)
        self.comb = np.zeros(3, np.int64)
        self.last_comb = np.zeros(3, np.int64)
        self.last2_comb = np.zeros(3, np.int64)
        self.last_int = 0
        self.last2_int = 0

    def go(self, s, out):
        # constant
        R = 128

        l = s.shape[0]
        assert l % 8 == 0
        assert out.shape[0] == l//R

        for i, x in enumerate(s):
            self.int[0] += x
            self.int[1] += self.int[0]
            self.int[2] += self.int[1]
            self.int[3] += self.int[2]
            if i % R == 0:
                self.comb[0] = self.int[3] - self.last2_int
                self.comb[1] = self.comb[0] - self.last2_comb[0]
                self.comb[2] = self.comb[1] - self.last2_comb[1]

                y = (self.comb[2] - self.last2_comb[2]) >> 1 # XXX is >> 1 required here?
                out[i//R] = y

                self.last2_int = self.last_int
                self.last_int = self.int[3]
                self.last2_comb[:] = self.last_comb[:]
                self.last_comb[:] = self.comb[:]


        return out

