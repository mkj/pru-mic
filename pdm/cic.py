import numpy as np
import numba

spec = (
    ('int', numba.int64[:]),
    ('comb', numba.int64[:]),
    ('last_comb', numba.int64[:]),
    ('last2_comb', numba.int64[:]),
    ('last_int', numba.int64),
    ('last2_int', numba.int64),
    ('rate', numba.int32),
    )

@numba.jitclass(spec)
class cic_n4m2(object):
    def __init__(self, rate):
        self.int = np.zeros(4, np.int64)
        self.comb = np.zeros(3, np.int64)
        self.last_comb = np.zeros(3, np.int64)
        self.last2_comb = np.zeros(3, np.int64)
        self.last_int = 0
        self.last2_int = 0
        self.rate = rate

        # limited by bitsout = N*log2(RM) + 1
        # so R < (2**((64 - 1)/N))/M
        N = 4
        M = 2
        # about 27k??
        assert(rate < (2**((64 - 1)/N))/M)

    def go(self, s, out):
        # constant

        l = s.shape[0]
        assert out.shape[0] == l//self.rate

        for i, x in enumerate(s):
            self.int[0] += x
            self.int[1] += self.int[0]
            self.int[2] += self.int[1]
            self.int[3] += self.int[2]
            if i % self.rate == 0:
                self.comb[0] = self.int[3] - self.last2_int
                self.comb[1] = self.comb[0] - self.last2_comb[0]
                self.comb[2] = self.comb[1] - self.last2_comb[1]

                y = (self.comb[2] - self.last2_comb[2]) >> 1 # XXX is >> 1 required here?
                out[i//self.rate] = y

                self.last2_int = self.last_int
                self.last_int = self.int[3]
                self.last2_comb[:] = self.last_comb[:]
                self.last_comb[:] = self.comb[:]


        return out

