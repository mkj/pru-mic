"""
Copyright (c) 2013 Stewart Fletcher

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the 'Software'), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED 'AS IS', WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
"""
import numpy as np
import pylab

class Frame(dict):
        def __init__(self, tracks, srate):
                """ takes an array shaped as [tracks, nsamps] """
                self.tracks = tracks
                self.nt = tracks.shape[0]
                self.ns = tracks.shape[1]
                self.srate = srate

def wiggle(frame, scale=1.0):
        fig = pylab.figure()
        ax = fig.add_subplot(111)        
        ns = frame.ns
        nt = frame.nt
        scalar = scale*frame.nt/(frame.nt*0.2) #scales the trace amplitudes relative to the number of traces
        frame.tracks[:,-1] = np.nan #set the very last value to nan. this is a lazy way to prevent wrapping
        vals = frame.tracks.ravel() #flat view of the 2d array.
        vect = np.arange(vals.size).astype(np.float) #flat index array, for correctly locating zero crossings in the flat view
        crossing = np.where(np.diff(np.signbit(vals)))[0] #index before zero crossing
        #use linear interpolation to find the zero crossing, i.e. y = mx + c. 
        x1=  vals[crossing]
        x2 =  vals[crossing+1]
        y1 = vect[crossing]
        y2 = vect[crossing+1]
        m = (y2 - y1)/(x2-x1)
        c = y1 - m*x1       
        #tack these values onto the end of the existing data
        x = np.hstack([vals, np.zeros_like(c)])
        y = np.hstack([vect, c])
        #resort the data
        order = np.argsort(y) 
        #shift from amplitudes to plotting coordinates
        x_shift, y = y[order].__divmod__(ns)

        # main x axis milliseconds
        srate_scale = 1000 / frame.srate
        y *= srate_scale

        ax.plot(y, x[order] *scalar + x_shift + 1, 'k')
        x[x>0] = np.nan
        x = x[order] *scalar + x_shift + 1
        ax.fill(y, x, 'k', aa=True) 
        ax.set_ylim([nt,0])
        ax.set_xlim([0,ns * srate_scale])

        pylab.tight_layout()
        pylab.show()
        


    
    
