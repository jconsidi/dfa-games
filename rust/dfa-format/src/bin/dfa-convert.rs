//! Convert legacy DFA directories into the single-file format.

use std::path::{Path, PathBuf};

use anyhow::{bail, Context, Result};
use clap::Parser;
use dfa_format::{convert, resolve_source, LegacyDfa};

#[derive(Parser, Debug)]
#[command(
    about = "Convert legacy DFA directories to the FORMAT-DFA.md single-file format",
    long_about = None
)]
struct Args {
    /// Scratch directory holding dfas_by_hash/ and the per-game name directories
    #[arg(long, default_value = "scratch")]
    scratch: PathBuf,

    /// Where to write <digest>.dfa (defaults to <scratch>/dfas_by_hash)
    #[arg(long)]
    out_dir: Option<PathBuf>,

    /// Validate each converted file before publishing it
    #[arg(long)]
    verify: bool,

    /// Convert every directory under <scratch>/dfas_by_hash
    #[arg(long)]
    all: bool,

    /// DFA names, 64 character hashes, or directory paths
    names: Vec<String>,
}

fn main() -> Result<()> {
    let args = Args::parse();
    let out_dir = args
        .out_dir
        .clone()
        .unwrap_or_else(|| args.scratch.join("dfas_by_hash"));

    let sources = collect_sources(&args)?;
    if sources.is_empty() {
        bail!("nothing to convert: pass a DFA name, a hash, a path, or --all");
    }

    let mut failures = 0usize;
    let mut stale = 0usize;
    for source in &sources {
        match convert_one(source, &out_dir, args.verify) {
            Ok(was_stale) => {
                if was_stale {
                    stale += 1;
                }
            }
            Err(e) => {
                failures += 1;
                println!("{}", source.display());
                println!("  ERROR: {e:#}");
                println!();
            }
        }
    }

    if sources.len() > 1 {
        println!(
            "{} converted, {stale} with a stale directory name, {failures} failed",
            sources.len() - failures
        );
    }
    if failures > 0 {
        std::process::exit(1);
    }
    Ok(())
}

/// Returns true when the source directory's name disagrees with its contents.
fn convert_one(source: &Path, out_dir: &Path, verify: bool) -> Result<bool> {
    let legacy =
        LegacyDfa::open(source).with_context(|| format!("reading {}", source.display()))?;

    println!("{}", source.display());
    if legacy.resolved_dir() != source {
        println!("  resolves to: {}", legacy.resolved_dir().display());
    }
    println!(
        "  ndim: {}  shape: {}  states: {}",
        legacy.ndim(),
        summarize_shape(legacy.shape()),
        legacy.layer_size().iter().sum::<u64>()
    );

    let legacy_hash = legacy
        .legacy_hash()
        .context("recomputing the legacy hash")?;
    println!("  legacy hash: {legacy_hash}");
    let stale = match legacy.stored_hash() {
        Some(stored) if stored != legacy_hash => {
            println!("  WARNING: contents hash to {legacy_hash} but directory is named {stored}");
            true
        }
        _ => false,
    };

    let converted = convert(&legacy, out_dir, verify).context("converting")?;
    match &converted.canonical_break {
        None if converted.canonical => println!("  canonical: yes"),
        None => println!("  canonical: no"),
        Some(b) => println!("  canonical: no ({b})"),
    }
    println!("  digest: {}", converted.digest);
    if converted.already_existed {
        println!("  {} (already existed, skipped)", converted.path.display());
    } else {
        println!("  {}", converted.path.display());
    }
    println!();

    Ok(stale)
}

fn collect_sources(args: &Args) -> Result<Vec<PathBuf>> {
    let mut sources: Vec<PathBuf> = args
        .names
        .iter()
        .map(|name| resolve_source(&args.scratch, name))
        .collect();

    if args.all {
        let by_hash = args.scratch.join("dfas_by_hash");
        let entries = std::fs::read_dir(&by_hash)
            .with_context(|| format!("listing {}", by_hash.display()))?;
        let mut found = Vec::new();
        for entry in entries {
            let entry = entry?;
            if entry.file_type()?.is_dir() {
                found.push(entry.path());
            }
        }
        found.sort();
        sources.extend(found);
    }

    Ok(sources)
}

/// `3 x16` rather than sixteen threes; run-length encoded so a 65 layer chess
/// shape stays readable on one line.
fn summarize_shape(shape: &[u32]) -> String {
    let mut parts: Vec<String> = Vec::new();
    let mut iter = shape.iter().peekable();
    while let Some(&value) = iter.next() {
        let mut run = 1usize;
        while iter.peek() == Some(&&value) {
            iter.next();
            run += 1;
        }
        if run == 1 {
            parts.push(value.to_string());
        } else {
            parts.push(format!("{value}x{run}"));
        }
    }
    parts.join(",")
}
