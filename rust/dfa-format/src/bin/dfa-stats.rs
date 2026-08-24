//! Report the size and shape of `.dfa` files.
//!
//! The `states` and `positions` lines are the two numbers src/stats.cpp
//! prints.  Everything else is what the self-describing file makes free:
//! src/stats.cpp has to be told which game a DFA belongs to before it can even
//! open one.

use std::path::{Path, PathBuf};

use anyhow::{bail, Context, Result};
use clap::Parser;
use dfa_format::stats::format_positions;
use dfa_format::{is_hash, Dfa, Stats};

#[derive(Parser, Debug)]
#[command(
    about = "Report states, accepted positions and storage layout of DFA files",
    long_about = None
)]
struct Args {
    /// Scratch directory holding dfas_by_hash/
    #[arg(long, default_value = "scratch")]
    scratch: PathBuf,

    /// Break the report down by layer
    #[arg(long)]
    per_layer: bool,

    /// One tab separated row per file, with a header line
    #[arg(long)]
    tsv: bool,

    /// Skip counting accepted positions, which is the only pass that costs
    /// memory proportional to the largest layer
    #[arg(long)]
    no_positions: bool,

    /// Report on every .dfa file under <scratch>/dfas_by_hash
    #[arg(long)]
    all: bool,

    /// Paths to .dfa files, or 64 character digests
    names: Vec<String>,
}

fn main() -> Result<()> {
    let args = Args::parse();
    let paths = collect_paths(&args)?;
    if paths.is_empty() {
        bail!("nothing to report on: pass a path, a digest, or --all");
    }

    if args.tsv {
        println!("path\tndim\tstates\ttransitions\tpositions\tbytes\tcanonical");
    }

    let mut failures = 0usize;
    for path in &paths {
        match report(path, &args) {
            Ok(()) => {}
            Err(e) => {
                failures += 1;
                eprintln!("{}: {e:#}", path.display());
            }
        }
    }

    if failures > 0 {
        std::process::exit(1);
    }
    Ok(())
}

fn report(path: &Path, args: &Args) -> Result<()> {
    let dfa = Dfa::open(path).with_context(|| format!("opening {}", path.display()))?;
    let stats = Stats::collect(&dfa, !args.no_positions)?;
    let positions = stats
        .positions
        .map(format_positions)
        .unwrap_or_else(|| "-".to_string());

    if args.tsv {
        println!(
            "{}\t{}\t{}\t{}\t{}\t{}\t{}",
            path.display(),
            stats.ndim,
            stats.states,
            stats.transitions,
            positions,
            stats.file_len,
            stats.canonical
        );
        return Ok(());
    }

    println!("{}", path.display());
    println!("  ndim: {}  shape: {}", stats.ndim, stats.shape_summary());
    println!("  states: {}", stats.states);
    println!("  positions: {positions}");
    println!("  transitions: {}", stats.transitions);
    println!(
        "  bytes: {} = {} header and tables + {} transitions + {} padding",
        stats.file_len, stats.header_and_tables, stats.transition_bytes, stats.padding_bytes
    );
    println!("  entry widths: {}", stats.width_summary());
    println!(
        "  initial_state: {}  canonical: {}",
        stats.initial_state,
        if stats.canonical { "yes" } else { "no" }
    );

    if args.per_layer {
        println!();
        println!("  layer\tshape\tstates\twidth\ttransitions\tbytes");
        for l in &stats.layers {
            println!(
                "  {}\t{}\t{}\t{}\t{}\t{}",
                l.layer, l.shape, l.states, l.width, l.transitions, l.bytes
            );
        }
    }
    println!();

    Ok(())
}

fn collect_paths(args: &Args) -> Result<Vec<PathBuf>> {
    let by_hash = args.scratch.join("dfas_by_hash");

    let mut paths: Vec<PathBuf> = args
        .names
        .iter()
        .map(|name| {
            if is_hash(name) {
                by_hash.join(format!("{name}.dfa"))
            } else {
                PathBuf::from(name)
            }
        })
        .collect();

    if args.all {
        let entries = std::fs::read_dir(&by_hash)
            .with_context(|| format!("listing {}", by_hash.display()))?;
        let mut found = Vec::new();
        for entry in entries {
            let path = entry?.path();
            if path.extension().is_some_and(|e| e == "dfa") {
                found.push(path);
            }
        }
        found.sort();
        paths.extend(found);
    }

    Ok(paths)
}
