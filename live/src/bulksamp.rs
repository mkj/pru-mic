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
}

impl BulkSamp {
    pub fn new(infile: &str) -> Result<Self> {

        Ok(BulkSamp {
            // TODO: perhaps with_capacity() for lower latency?
            // Default 8kB is 2ms.
            inf: BufReader::new(std::fs::File::open(infile)?),
        })
    }
}

impl Iterator for BulkSamp {
    type Item = Result<[[Sample; CHUNK]; 8]>;

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
        let mut res = [[0 as Sample; CHUNK]; 8];
        for i in 0..CHUNK {
            for c in 0..8 {
                res[c][i] = ((((buf[i] >> c) & 1) as i32) << 16) - 32768;
            }
        }
        Some(Ok(res))
    }
}




