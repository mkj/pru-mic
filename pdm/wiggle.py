"""
Based on plotting code in PySeis:
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

Modified 2016 Matt Johnston matt@ucc.asn.au to plot horizontally an work
with different frame object.
"""
import numpy as np
import pylab

def wiggle(frame, scale=1.0):
        tracks = frame.tracks.copy()
        fig = pylab.figure()
        ax = fig.add_subplot(111)        
        ns = frame.ns
        nt = frame.nt
        scalar = scale*frame.nt/(frame.nt*0.2) #scales the trace amplitudes relative to the number of traces
        tracks *= -1 # invert so that positive is up
        tracks[:,-1] = np.nan #set the very last value to nan. this is a lazy way to prevent wrapping
        vals = tracks.ravel() #flat view of the 2d array.
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

        srate_scale = 1000 / frame.srate
        y *= srate_scale
        y += frame.start*1000


        ax.plot(y, x[order] *scalar + x_shift + 1, 'k')
        x[x>0] = np.nan
        x = x[order] *scalar + x_shift + 1
        ax.fill(y, x, 'k', aa=True) 
        ax.set_ylim([nt+1,0])
        ax.set_xlim([1000*frame.start, 1000*frame.start + ns * srate_scale])

        pylab.tight_layout()
        pylab.show()
        


    
    
