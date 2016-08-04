import numpy as np
import numba

class cic128(object):
    def __init__(self):
        pass

    @numba.jit
    def __call__(self, s):
        l = s.shape[0]
        assert l % 8 == 0

        # constant
        R = 128

        # integrators
        int1 = 0
        int2 = 0
        int3 = 0
        int4 = 0

        last_comb1 = 0
        last_comb2 = 0
        last_comb3 = 0
        last2_comb1 = 0
        last2_comb2 = 0
        last2_comb3 = 0
        last_int4 = 0
        last2_int4 = 0

        out = np.ndarray(l // R)

        for i, x in enumerate(s):
            int1 += x
            int2 += int1
            int3 += int2
            int4 += int3
            if i % R == 0:
                comb1 = int4 - last2_int4
                comb2 = comb1 - last2_comb1
                comb3 = comb2 - last2_comb2
                y = (comb3 - last2_comb3) >> 1 # XXX is >> 1 required here?
                last2_int4 = last_int4
                last_int4 = int4
                last2_comb1 = last_comb1
                last_comb1 = comb1
                last2_comb2 = last_comb2
                last_comb2 = comb2
                last2_comb3 = last_comb3
                last_comb3 = comb3

                out[i//R] = y / 2.0**28

        return out

