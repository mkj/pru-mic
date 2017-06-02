This project is 8 MEMS microphones input directly to a Beaglebone Black.

They each provide a 4mhz 1-bit PDM input stream, across 4 wires multiplexed on rising/falling clock signal. 
The PDM microphone is a Knowles SPH0641LU4H

The project consists of 

- pru/ - the bit-banging GPIO driver for the PDM input, which runs on the realtime PRU units of the beaglebone
- driver/ - a Linux kernel driver which presents /dev/bulksamp. Inputs are multiplexed bitwise, the 8 microphones each contribute a bit to a byte of input.
- pdm/ - a Python program to decode PDM bitstreams to audio data. This uses Numba to perform fast PDM decoding with a Cascaded integrator–comb (CIC) filter. It also has some waveform plotting code. Tested with Python 3.5.
- bitex/ - a program to extract a single bitstream from /dev/bulksamp. This can be used to pipe a single microphone into the pdm program.
- circuit/ - basic pinout

The code works as a prototype, though there are likely some specifics of my personal setup that I've neglected to mention.

The pdm/ kernel driver is GPL licensed, the rest is MIT. See LICENSE.

Matt Johnston 2017 matt@ucc.asn.au

There are a few similar looking projects I've come across:

- http://bela.io/
- https://github.com/LCAV/CompactSix
- https://www.seeedstudio.com/ReSpeaker-Mic-Array-Far-field-w%2F-7-PDM-Microphones-p-2719.html


