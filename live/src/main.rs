#[allow(unused_imports)]
use {
    log::{debug, error, info, warn},
    anyhow::{Result,Context,bail,anyhow},
};
use simplelog::{CombinedLogger,LevelFilter,TermLogger,TerminalMode};

use std::io::BufWriter;
use std::fs::File;

mod bulksamp;

#[derive(argh::FromArgs)]
/** AnyPitch
Matt Johnston 2021 matt@ucc.asn.au */
struct Args {
    #[argh(switch, short='v')]
    /// verbose debug logging
    debug: bool,

    /// input device, default /dev/bulk_samp31
    #[argh(option, short = 'd', default = "\"/dev/bulk_samp31\".to_string()")]
    input: String,

    /// channel 0-7, default 2
    #[argh(option, short = 'c', default = "2")]
    channel: usize,

    /// decim - the downsampling rate, default 64
    #[argh(option, short = 'r', default = "64")]
    decim: usize,

    /// output wav file (optional)
    #[argh(option, short = 'w')]
    outwav: Option<String>,

    /// play output
    #[argh(switch, short = 'w')]
    play: bool,

    /// CIC order (optional), default 4
    #[argh(option, default = "4")]
    order: usize,

    /// apply FIR compensation filter (off by default)
    #[argh(switch)]
    fir: bool,
}

fn setup_log(debug: bool) -> Result<()> {
    let level = match debug {
        true => LevelFilter::Debug,
        false => LevelFilter::Info,
    };
    let logconf = simplelog::ConfigBuilder::new()
    .set_time_format_str("%Y-%m-%d %H:%M:%S%.3f")
    .set_time_to_local(true)
    .build();
    CombinedLogger::init(
        vec![
            TermLogger::new(level, logconf.clone(), TerminalMode::Mixed),
            //WriteLogger::new(level, logconf, open_logfile()?),
        ]
    ).context("logging setup failed")
}

fn open_wav_out(wav_filename: &str) -> Result<hound::WavWriter<BufWriter<File>>> {
    let spec = hound::WavSpec {
        channels: 1,
        sample_rate: 44100,
        bits_per_sample: 16,
        sample_format: hound::SampleFormat::Int,
    };
    hound::WavWriter::create(wav_filename, spec).context("Error opening output wavfile")
}

fn main() -> Result<()> {
    let args: Args = argh::from_env();
    setup_log(args.debug)?;

    // graceful exit
    let term = std::sync::Arc::new(core::sync::atomic::AtomicBool::new(false));
    signal_hook::flag::register(signal_hook::consts::SIGTERM, term.clone())?;
    signal_hook::flag::register(signal_hook::consts::SIGINT, term.clone())?;

    let mut wav = if let Some(f) = args.outwav {
        match open_wav_out(&f) {
            Ok(w) => Some(w),
            // faffing around because hound's Error is odd?
            // https://github.com/dtolnay/anyhow/issues/35#issuecomment-547986739
            Err(e) => bail!(e), 
        }
    } else {
        None
    };

    let stream = bulksamp::BulkSamp::new(&args.input, &[args.channel])?;
    let mut cic = sdr::CIC::<bulksamp::Sample>::new(args.order, args.decim, 1);
    let mut fir = sdr::FIR::<i32>::cic_compensator(63, args.order, args.decim, 1);

    for s in stream {
        if term.load(core::sync::atomic::Ordering::Relaxed) {
            info!("Exiting");
            break;
        }
        let s = s?;
        let c = &s[0];
        let wout = cic.process(c);
        let wout = if args.fir {
            fir.process(&wout)
        } else {
            wout
        };
        //println!("c {:?}", c);
        //println!("wout {:?}", wout);
        if let Some(ref mut w) = wav {
            for s in wout {
                w.write_sample(s)?;
            }
        }
    }

    Ok(())
}
