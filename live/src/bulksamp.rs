#[allow(unused_imports)]
use {
    log::{debug, error, info, warn},
    anyhow::{Result,Context,bail,anyhow},
};
use std::io::{Read,BufReader};
use dasp::{signal::Signal,frame::Frame};

// TODO: const generics
// Samples per channel per chunk. bytes of input, bits per channel of output
const CHUNK: usize = 1024;
pub type Sample = i32;
const SAMPLE_SIZE: usize = std::mem::size_of::<usize>() * 8;

/// Reads a single channel from bulksamp
pub struct BulkSamp {
    inf: std::io::BufReader<std::fs::File>,
    want_chan: usize,
    actual_chunk: usize,
    finished: bool,
    err: Result<()>,

}

impl BulkSamp {
    /// will read in multiples of r
    pub fn new(infile: &str, want_chan: usize, r: usize) -> Result<Self> {

        let f = std::fs::File::open(infile)
        .with_context(|| format!("Opening {}", infile))?;

        let b = BulkSamp {
            // TODO: perhaps with_capacity() for lower latency?
            // Default 8kB is 2ms.
            inf: BufReader::new(f),
            want_chan,
            actual_chunk: (CHUNK / r) * r,
            finished: false,
            err: Ok(()),
        };
        debug!("actual_chunk {}", b.actual_chunk);
        Ok(b)
    }

    pub fn is_error(&self) -> &Result<()> {
        &self.err
    }
}

impl Signal for BulkSamp {
    type Frame = Sample;

    fn next(&mut self) -> Self::Frame {
        // XXX allocation
        let mut buf = vec![0u8; 1];

        if self.finished {
            return Self::Frame::EQUILIBRIUM;
        }
        if let Err(e) = self.inf.read_exact(&mut buf) {
            self.finished = true;
            if e.kind() != std::io::ErrorKind::UnexpectedEof {
                self.err = Err(e).context("Error reading input");
            }
            // let it return the 0 samples
        }

        ((((buf[0] >> self.want_chan) & 1) as i32) << 16) - 32768
    }

    fn is_exhausted(&self) -> bool {
        self.finished
    }
}




