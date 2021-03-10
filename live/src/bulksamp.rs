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
    want_chans: Vec<usize>,
}

impl BulkSamp {
    pub fn new(infile: &str, want_chans: &[usize]) -> Result<Self> {

        let f = std::fs::File::open(infile)
        .with_context(|| format!("Opening {}", infile))?;

        Ok(BulkSamp {
            // TODO: perhaps with_capacity() for lower latency?
            // Default 8kB is 2ms.
            inf: BufReader::new(f),
            want_chans: want_chans.to_vec(),
        })
    }
}

impl Iterator for BulkSamp {
    type Item = Result<Vec<[Sample; CHUNK]>>;

    fn next(&mut self) -> Option<Self::Item> {
        let mut buf = [0u8; CHUNK];
        match self.inf.read(&mut buf) {
            Ok(nread) => {
                if nread < CHUNK {
                    // discard the last chunk if it's not a full multiple!
                    return None
                }
            }
            Err(e) => {
                return Some(Err(e).context("Error reading input"))
            }
        };

        // demultiplex
        let mut res = vec!();
        for _ in &self.want_chans {
            res.push([0 as Sample; CHUNK]);
        }
        for i in 0..CHUNK {
            for (n, c) in self.want_chans.iter().enumerate() {
                res[n][i] = ((((buf[i] >> c) & 1) as i32) << 16) - 32768;
            }
        }
        Some(Ok(res))
    }
}




