//! Cap rayon's global thread pool at the job's `NSLOTS` core grant.
//!
//! Rust counterpart to the TBB cap in `src/parallel.h`. On a shared cluster
//! (e.g. BU's SCC under SGE/UGE) rayon's default pool sizes itself to every
//! core on the node, ignoring the job's actual core grant unless something
//! caps it explicitly.

use anyhow::{bail, Context, Result};

/// Reads `NSLOTS` and, if set, installs it as rayon's global thread pool
/// size. Call this once, before any `rayon::prelude` use.
///
/// Unset leaves rayon's own default (`std::thread::available_parallelism`)
/// in place. Set but not a positive integer is an error, not a silent
/// fallback onto that default.
pub fn cap_thread_pool_from_environment() -> Result<()> {
    let nslots = match std::env::var("NSLOTS") {
        Ok(value) => value,
        Err(std::env::VarError::NotPresent) => return Ok(()),
        Err(std::env::VarError::NotUnicode(_)) => {
            bail!("NSLOTS environment variable is not valid UTF-8");
        }
    };

    let threads: usize = nslots.parse().ok().filter(|&n| n != 0).ok_or_else(|| {
        anyhow::anyhow!("NSLOTS environment variable must be a positive integer, got {nslots:?}")
    })?;

    rayon::ThreadPoolBuilder::new()
        .num_threads(threads)
        .build_global()
        .context("failed to install rayon thread pool sized from NSLOTS")?;

    Ok(())
}
