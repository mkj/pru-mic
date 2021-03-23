#[allow(unused_imports)]
use {
    log::{debug, error, info, warn},
    anyhow::{Result,Context,bail,anyhow},
};
use simplelog::{CombinedLogger,LevelFilter,TermLogger,TerminalMode};

use std::io::BufWriter;
use std::fs::File;
use std::sync::Arc;
use core::sync::atomic::AtomicBool;
use std::time::Duration;

use dasp::{Signal,ring_buffer::Bounded};

mod bulksamp;

#[derive(argh::FromArgs)]
/** AnyPitch
Matt Johnston 2021 matt@ucc.asn.au */
struct Args {
    #[argh(switch, short='v')]
    /// verbose debug logging
    debug: bool,

    /// input PDM file. If not given will use mic device /dev/bulk_samp31
    #[argh(option, short = 'i')]
    input: Option<String>,

    /// save PDM file
    #[argh(option, short = 's')]
    save: Option<String>,

    /// channel 0-7, default 2
    #[argh(option, short = 'c', default = "2")]
    channel: usize,

    /// decim - the downsampling rate, default 91
    #[argh(option, short = 'r', default = "91")]
    decim: usize,

    /// output wav file (optional)
    #[argh(option, short = 'w')]
    outwav: Option<String>,

    /*
    /// play output
    #[argh(switch, short = 'w')]
    play: bool,
    */

    /// CIC order (optional), default 4
    #[argh(option, default = "4")]
    order: usize,

    /// apply FIR compensation filter (off by default)
    #[argh(switch)]
    fir: bool,

    #[argh(subcommand)]
    command: Option<CommandEnum>,
}

/// run benchmark
#[derive(argh::FromArgs, PartialEq, Debug)]
#[argh(subcommand)]
enum CommandEnum {
    Bench(BenchArgs),
}

#[derive(argh::FromArgs, PartialEq, Debug)]
#[argh(subcommand, name = "bench")]
/// run a benchmark
struct BenchArgs {
    /// run time, default 3 seconds
    #[argh(option, default = "3.0")]
    time: f32,
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

const BULK_SAMP_DEVICE: &str = "/dev/bulk_samp31";

fn main() -> Result<()> {
    let args: Args = argh::from_env();
    setup_log(args.debug)?;

    match args.command {
        Some(CommandEnum::Bench(_)) => {
            return bench(args);
        },
        None => (),
    };
    // graceful exit on signal.
    let terminate = Arc::new(AtomicBool::new(false));
    signal_hook::flag::register(signal_hook::consts::SIGTERM, terminate.clone())?;
    signal_hook::flag::register(signal_hook::consts::SIGINT, terminate.clone())?;
    run_loop(args, terminate)
}

fn bench(args: Args) -> Result<()> {
    let terminate = Arc::new(AtomicBool::new(false));
    let t = terminate.clone();
    std::thread::spawn(move || {
        std::thread::sleep(Duration::from_secs(3));
        t.store(true, core::sync::atomic::Ordering::Relaxed);
    });

    run_loop(args, terminate)?;

    Ok(())
}

fn run_loop(args: Args, terminate: Arc<AtomicBool>) -> Result<()> {

    let inpdm = if let Some(f) = args.input {
        info!("Reading PDM from input file {}", f);
        f
    } else {
        info!("PDM input from mic");
        BULK_SAMP_DEVICE.into()
    };

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

    let mut stream = bulksamp::BulkSamp::new(&inpdm, args.channel, args.decim)?;
    let mut cic = sdr::CIC::<bulksamp::Sample>::new(args.order, args.decim, 1);
    let mut fir = sdr::FIR::<i32>::cic_compensator(63, args.order, args.decim, 1);

    // let liveaudio = if args.play {
    //     Some(())
    // } else {
    //     None
    // };

    // let ring_buffer = Bounded::from(vec![0 as bulksamp::Sample; args.decim]);
    // let mut buffered_stream = stream.buffered(ring_buffer);

    loop {
        if terminate.load(core::sync::atomic::Ordering::Relaxed) {
            info!("Exiting");
            break;
        }

        // TODO: how can we handle exhaustion of stream?
        let inp = stream.by_ref().take(args.decim).collect::<Vec<_>>();
        let wout = cic.process(&inp);
        let wout = if args.fir {
            fir.process(&wout)
        } else {
            wout
        };

        if let Some(ref mut w) = wav {
            for s in wout {
                w.write_sample(s)?;
            }
        }

        if stream.is_exhausted() {
            debug!("exhausted");
            if let Err(e) = stream.is_error() {
                warn!("Error from input {}", e);
            }
            break;
        }
    }

    Ok(())
}
