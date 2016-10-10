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
                yorigin = t+1
                vals[0] = yorigin
                vals[-1] = yorigin
                vals[1:-1] = yorigin + -0.5*frame.tracks[t,:]

                # clip fill to positive values only
                clip_points = np.array([frame.start,yorigin-eps,
                                        frame.end,yorigin-eps,
                                        frame.end,-10,
                                        frame.start,-10])
                clip_points.shape = (4,2)
                patch = matplotlib.patches.Polygon(clip_points, closed=True, transform=ax.transData)
                ax.fill(times,vals,color='k',clip_path=patch)

                ax.plot(times, vals, fmt)

        ax.set_ylim([frame.nt+1,0])
        plt.yticks(range(1,frame.nt+1), frame.names)
        plt.tight_layout()
        plt.show()
