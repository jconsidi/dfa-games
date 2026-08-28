//! Check that `L(A) = L(B) ∪ L(C)` for three `.dfa` files.
//!
//! Independent of the solver and of any game: the three files only have to
//! agree on ndim and shape.  See `union.rs` for the walk and for why `A` has to
//! carry the canonical flag.

use std::path::{Path, PathBuf};

use anyhow::Result;
use clap::Parser;
use dfa_format::union::{sample_for_witness, verify_dfa_union};
use dfa_format::{is_hash, Dfa};

#[derive(Parser, Debug)]
#[command(
    about = "Verify that one DFA is the union of two others",
    long_about = None
)]
struct Args {
    /// Scratch directory holding dfas_by_hash/ and the per game directories
    #[arg(long, default_value = "scratch")]
    scratch: PathBuf,

    /// Resolve bare DFA names under <scratch>/<game>/. Pure path joining: this
    /// binary knows nothing about games, and a name that is already a path or
    /// a digest is unaffected.
    #[arg(long)]
    game: Option<String>,

    /// Strings to draw from each of the three languages as a pre-filter, 0 to
    /// skip it. Cheap, and the only thing here that produces a witness.
    #[arg(long, default_value_t = 1000)]
    samples: u32,

    /// Seed for the pre-filter, so a witness can be reproduced
    #[arg(long, default_value_t = 1)]
    seed: u64,

    /// Skip the pre-filter and go straight to the exact walk
    #[arg(long)]
    exact_only: bool,

    /// The claimed union: a DFA name when --game is given, otherwise a path to
    /// a .dfa file. A 64 character digest works either way.
    a: String,

    /// Left operand
    b: String,

    /// Right operand
    c: String,
}

fn main() -> std::process::ExitCode {
    let args = Args::parse();
    match run(&args) {
        Ok(()) => std::process::ExitCode::SUCCESS,
        Err(e) => {
            // A refutation arrives here like any other failure, which is the
            // point: there is no path where it can be passed over. The message
            // already says the relation does not hold, so there is nothing to
            // add before it.
            eprintln!("{e:#}");
            std::process::ExitCode::FAILURE
        }
    }
}

fn resolve(scratch: &Path, game: Option<&str>, name: &str) -> PathBuf {
    // A digest addresses the content store directly, whatever else was asked
    // for, which is how the C++ get_file_name reads a name too.
    if is_hash(name) {
        return scratch.join("dfas_by_hash").join(format!("{name}.dfa"));
    }
    match game {
        Some(game) => scratch.join(game).join(name),
        None => PathBuf::from(name),
    }
}

fn open(scratch: &Path, game: Option<&str>, name: &str, role: &str) -> Result<Dfa> {
    let path = resolve(scratch, game, name);
    // Not `with_context`: FormatError::Io already names the path and its cause,
    // and anyhow's chain would then print the cause a second time.
    Dfa::open(&path).map_err(|e| anyhow::anyhow!("opening {role}: {e}"))
}

fn run(args: &Args) -> Result<()> {
    let game = args.game.as_deref();
    let a = open(&args.scratch, game, &args.a, "A")?;
    let b = open(&args.scratch, game, &args.b, "B")?;
    let c = open(&args.scratch, game, &args.c, "C")?;

    // Resolved, so both the log and any failure name the file that was read
    // rather than what was typed.
    let a_name = resolve(&args.scratch, game, &args.a).display().to_string();
    let b_name = resolve(&args.scratch, game, &args.b).display().to_string();
    let c_name = resolve(&args.scratch, game, &args.c).display().to_string();

    let samples = if args.exact_only { 0 } else { args.samples };
    if samples > 0 {
        sample_for_witness(&a, &b, &c, samples, args.seed)?;
        println!(
            "prefilter: {samples} samples per language, seed {} -- no witness",
            args.seed
        );
    }

    let stats = verify_dfa_union(&a, &a_name, &b, &b_name, &c, &c_name)?;

    // The breakdown, which the library's one line summarises: which of the
    // memos carried the work is the thing worth knowing about a triple.
    println!(
        "reachable pairs: {} both non-trivial, {} b reject-all (keyed on c), {} c reject-all (keyed on b)",
        stats.pairs_both, stats.pairs_b_reject, stats.pairs_c_reject
    );
    println!(
        "short circuits:  {} accept-all, {} reject-all",
        stats.stops_accept, stats.stops_reject
    );

    Ok(())
}
