#[allow(unused_imports)]
use {
    log::{debug, error, info, warn},
    anyhow::{Result,Context,bail,anyhow},
};
use std::io::{Read,BufReader};

// TODO: const generics
// Samples per channel per chunk. bytes of input, bits per channel of output
const CHUNK: usize = 1024;
pub type Sample = i32;

pub struct BulkSamp {
    inf: std::io::BufReader<std::fs::File>,
    want_chan: usize,
    read_buf: Vec<u8>,
}

impl BulkSamp {
    pub fn new(infile: &str, want_chan: usize, block_size: usize) -> Result<Self> {

        let f = std::fs::File::open(infile)
        .with_context(|| format!("Opening {}", infile))?;

        Ok(BulkSamp {
            // TODO: perhaps with_capacity() for lower latency?
            // Default 8kB is 2ms.
            inf: BufReader::new(f),
            want_chan,
            read_buf: vec![0u8; (CHUNK/block_size)*block_size],
        })
    }
}

impl Iterator for BulkSamp {
    type Item = Result<Vec<Sample>>;

    fn next(&mut self) -> Option<Self::Item> {
        if let Err(e) = self.inf.read_exact(&mut self.read_buf) {
            if e.kind() != std::io::ErrorKind::UnexpectedEof {
                return Some(Err(e).context("Error reading input"));
            }
            // XXX: this usually drops the last few samples
            return None;
        }

        // demultiplex
        let res = self.read_buf.iter()
        .map(|r| ((((r >> self.want_chan) & 1) as i32) << 16) - 32768)
        .collect();
        Some(Ok(res))
    }
}




