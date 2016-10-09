"""
# (c) 2016 Matt Johnston matt@ucc.asn.au
"""
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.transforms
import matplotlib.patches

def wiggle(frame, scale=1.0):
        fig,ax = plt.subplots()

        # add an extra zero start/end sample so that fill works properly
        times = np.zeros(frame.ns+2)
        vals = np.zeros(frame.ns+2)
        times[1:-1] = np.linspace(frame.start, frame.end, frame.ns)
        times[0] = times[1]
        times[-1] = times[-2]
        fmt = 'k-' # black solid line
        eps = 0.01 # bit of a bodge

        for t in range(frame.nt):
                vals[0] = t
                vals[-1] = t
                vals[1:-1] = t + 0.5*frame.tracks[t,:]

                # clip fill to positive values only
                clip_points = np.array([frame.start,t+eps,
                                        frame.end,t+eps,
                                        frame.end,9999,
                                        frame.start,9999])
                clip_points.shape = (4,2)
                patch = matplotlib.patches.Polygon(clip_points, closed=True, transform=ax.transData)
                ax.fill(times,vals,color='k',clip_path=patch)

                ax.plot(times, vals, fmt)

        plt.yticks(range(frame.nt), frame.names)
        plt.tight_layout()
        plt.show()
