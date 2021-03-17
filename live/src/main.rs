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
/** PDM to wav file.
Matt Johnston 2021 matt@ucc.asn.au */
struct Args {
    /// verbose debug logging
    #[argh(switch, short='v')]
    debug: bool,

    /// input device, default /dev/bulk_samp31
    #[argh(option, short = 'i', default = "\"/dev/bulk_samp31\".to_string()")]
    input: String,

    /// channel 0-7, default 2
    #[argh(option, short = 'c', default = "2")]
    channel: usize,

    /// decim - the downsampling rate, default 91 (approx realtime = 4e6/44100)
    /// eg "-r 9" will be ~10x slowdown
    #[argh(option, short = 'r', default = "91")]
    decim: usize,

    /// output wav file (optional)
    #[argh(option, short = 'w')]
    outwav: Option<String>,

    /// scale output amplitude
    #[argh(option, short = 's', default = "1f32")]
    scaleamp: f32,

    // /// play output
    // #[argh(switch, short = 'w')]
    // play: bool,

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
    let terminate = std::sync::Arc::new(core::sync::atomic::AtomicBool::new(false));
    signal_hook::flag::register(signal_hook::consts::SIGTERM, terminate.clone())?;
    signal_hook::flag::register(signal_hook::consts::SIGINT, terminate.clone())?;

    let mut wav = if let Some(ref wfn) = args.outwav {
        match open_wav_out(wfn) {
            Ok(w) => Some(w),
            // faffing around because hound's Error is odd?
            // https://github.com/dtolnay/anyhow/issues/35#issuecomment-547986739
            Err(e) => bail!(e), 
        }
    } else {
        None
    };

    let stream = bulksamp::BulkSamp::new(&args.input, args.channel, args.decim)?;
    let mut cic = sdr::CIC::<bulksamp::Sample>::new(args.order, args.decim, 1);
    let mut fir = sdr::FIR::<i32>::cic_compensator(63, args.order, args.decim, 1);

    info!("Running, input from {}", args.input);

    for s in stream {
        if terminate.load(core::sync::atomic::Ordering::Relaxed) {
            break;
        }
        let s = s?;
        let wout = cic.process(&s);
        let wout = if args.fir {
            fir.process(&wout)
        } else {
            wout
        };
        //println!("c {:?}", c);
        //println!("wout {:?}", wout);
        if let Some(ref mut w) = wav {
            if args.scaleamp == 1f32 {
                for s in wout {
                    let scals = s.clamp(i16::MIN as i32, i16::MAX as i32) as i16;
                    w.write_sample(scals)?
                }
            } else {
                for s in wout {
                    let scals = ((s as f32) * args.scaleamp)
                    .clamp(i16::MIN as f32, i16::MAX as f32) as i16;
                    w.write_sample(scals)?;
                }

            }
        }
    }
    if let Some(ref wfn) = args.outwav {
        wav.unwrap().finalize()?;
        info!("Wrote wav file {}", wfn);
    }
    else {
        info!("Finished.");
    }

    Ok(())
}