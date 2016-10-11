"""
# (c) 2016 Matt Johnston matt@ucc.asn.au
"""
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.transforms
import matplotlib.patches

class wiggle(object):

    def __init__(self, tracks, posfill=True):
        self.current_scale = 1.0
        self.tracks = tracks
        self.posfill = posfill

        self.first_plot()

    def get_vals(self, index):
        vals = np.zeros(self.tracks.ns+2)
        yorigin = index+1
        vals[0] = yorigin
        vals[-1] = yorigin
        vals[1:-1] = yorigin + -0.5*self.tracks.tracks[index,:]*self.current_scale
        return vals

    def on_key(self, event):
        if event.key == 'g':
            scale = 2
        elif event.key == 'G':
            scale = 0.5
        else:
            return

        self.current_scale *= scale
        for index, l in enumerate(self.lines):
            l.set_ydata(self.get_vals(index))
        plt.draw()

    def first_plot(self):
        tracks = self.tracks
        fig,ax = plt.subplots()


        # add an extra zero start/end sample so that fill works properly
        times = np.zeros(tracks.ns+2)
        vals = np.zeros(tracks.ns+2)
        times[1:-1] = np.linspace(tracks.start, tracks.end, tracks.ns)
        times[0] = times[1]
        times[-1] = times[-2]
        fmt = 'k-' # black solid line
        eps = 0.01 # bit of a bodge

        self.lines = []

        for index in range(tracks.nt):
            yorigin = index+1
            vals = self.get_vals(index)

            # clip fill to positive values only
            if self.posfill:
                clip_points = np.array([tracks.start,yorigin-eps,
                                        tracks.end,yorigin-eps,
                                        tracks.end,-10,
                                        tracks.start,-10])
                clip_points.shape = (4,2)
                patch = matplotlib.patches.Polygon(clip_points, closed=True, transform=ax.transData)
                ax.fill(times,vals,color='k',clip_path=patch)

            l = plt.plot(times, vals, fmt)
            self.lines.append(l[0])

        fig.canvas.mpl_connect('key_press_event', self.on_key)

        ax.set_ylim([tracks.nt+1,0])
        plt.yticks(range(1,tracks.nt+1), tracks.names)
        plt.tight_layout()
        plt.show()
