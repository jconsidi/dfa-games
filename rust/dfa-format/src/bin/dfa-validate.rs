//! Validate `.dfa` files against FORMAT-DFA.md.

use std::path::{Path, PathBuf};

use anyhow::{bail, Context, Result};
use clap::Parser;
use dfa_format::{is_hash, validate, Dfa, ValidateOptions};

#[derive(Parser, Debug)]
#[command(
    about = "Validate DFA single-file format files against FORMAT-DFA.md",
    long_about = None
)]
struct Args {
    /// Scratch directory holding dfas_by_hash/
    #[arg(long, default_value = "scratch")]
    scratch: PathBuf,

    /// Skip verifying the SHA-256 digest in the header
    #[arg(long)]
    no_digest: bool,

    /// Skip verifying that rows 0 and 1 hold their reserved values
    #[arg(long)]
    no_reserved_rows: bool,

    /// Skip verifying that every entry is within the next layer
    #[arg(long)]
    no_entry_bounds: bool,

    /// Skip verifying canonical numbering when flags bit 0 is set
    #[arg(long)]
    no_canonical: bool,

    /// Skip verifying that a <digest>.dfa file is named after its own digest
    #[arg(long)]
    no_filename: bool,

    /// Validate every .dfa file under <scratch>/dfas_by_hash
    #[arg(long)]
    all: bool,

    /// Report whether this comma separated string is accepted, e.g. 0,1,2,0
    #[arg(long, value_name = "CHARS")]
    accepts: Option<String>,

    /// Paths to .dfa files, or 64 character digests
    names: Vec<String>,
}

fn main() -> Result<()> {
    let args = Args::parse();
    let opts = ValidateOptions {
        digest: !args.no_digest,
        reserved_rows: !args.no_reserved_rows,
        entry_bounds: !args.no_entry_bounds,
        canonical: !args.no_canonical,
        filename: !args.no_filename,
    };

    let paths = collect_paths(&args)?;
    if paths.is_empty() {
        bail!("nothing to validate: pass a path, a digest, or --all");
    }

    let mut bad = 0usize;
    for path in &paths {
        if !validate_one(path, &opts, args.accepts.as_deref())? {
            bad += 1;
        }
    }

    if paths.len() > 1 {
        println!("{} valid, {bad} invalid", paths.len() - bad);
    }
    if bad > 0 {
        std::process::exit(1);
    }
    Ok(())
}

fn validate_one(path: &Path, opts: &ValidateOptions, accepts: Option<&str>) -> Result<bool> {
    let report = validate(path, opts).with_context(|| format!("validating {}", path.display()))?;

    println!("{}", path.display());
    println!("  size: {} bytes", report.file_len);
    if let (Some(header), Some(layout)) = (&report.header, &report.layout) {
        println!(
            "  version: {}.{}  ndim: {}  states: {}",
            header.version_major,
            header.version_minor,
            layout.ndim(),
            layout.total_states()
        );
        println!(
            "  initial_state: {}  canonical: {}",
            header.initial_state,
            if header.canonical() { "yes" } else { "no" }
        );
    }

    if report.ok() {
        println!("  VALID");
    } else {
        for violation in &report.violations {
            println!("  INVALID: {violation}");
        }
    }

    if let Some(spec) = accepts {
        let string = parse_string(spec)?;
        // Only meaningful on a file that passed, so say why when it did not.
        match Dfa::open(path) {
            Ok(dfa) => {
                let verdict = dfa.accepts(&string)?;
                println!("  accepts {spec}: {verdict}");
            }
            Err(e) => println!("  accepts {spec}: cannot evaluate ({e})"),
        }
    }

    println!();
    Ok(report.ok())
}

fn parse_string(spec: &str) -> Result<Vec<u32>> {
    spec.split(',')
        .map(|part| {
            part.trim()
                .parse::<u32>()
                .with_context(|| format!("parsing character {part:?}"))
        })
        .collect()
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
            let entry = entry?;
            let path = entry.path();
            if path.extension().is_some_and(|e| e == "dfa") {
                found.push(path);
            }
        }
        found.sort();
        paths.extend(found);
    }

    Ok(paths)
}
