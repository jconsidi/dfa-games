//! Reader, writer and validator for the DFA single-file format.
//!
//! See `FORMAT-DFA.md` at the repository root for the specification.  Section
//! references in the source refer to it.
//!
//! The two binaries in this crate are thin wrappers: [`layout`] is the single
//! authority on where bytes go, [`write`] produces files and [`read`]
//! validates them, and [`legacy`] knows how to read the directory-per-DFA
//! layout that `src/DFA.cpp` writes.

mod bitset;
pub mod error;
pub mod layout;
pub mod legacy;
pub mod read;
pub mod write;

pub use error::{FormatError, Result, Violation};
pub use layout::Layout;
pub use legacy::LegacyDfa;
pub use read::{validate, Dfa, Report, ValidateOptions};
pub use write::{convert, Converted};

use std::path::{Path, PathBuf};

/// Resolve a command line argument to a source directory.
///
/// Accepts a bare 64 character hash, a path that already exists, or a DFA name
/// such as `breakthrough_4x4/forward,ply=001`, which lives in the scratch
/// directory as a symlink into `dfas_by_hash/`.
pub fn resolve_source(scratch: &Path, arg: &str) -> PathBuf {
    if is_hash(arg) {
        return scratch.join("dfas_by_hash").join(arg);
    }
    let direct = Path::new(arg);
    if direct.exists() {
        return direct.to_path_buf();
    }
    scratch.join(arg)
}

/// Lower case hex, the spelling used for every digest this crate prints.
pub fn hex(bytes: &[u8]) -> String {
    use std::fmt::Write;
    let mut out = String::with_capacity(bytes.len() * 2);
    for b in bytes {
        let _ = write!(out, "{b:02x}");
    }
    out
}

pub fn is_hash(s: &str) -> bool {
    s.len() == 64
        && s.bytes()
            .all(|b| b.is_ascii_hexdigit() && !b.is_ascii_uppercase())
}

/// An automaton held in memory, used to build test vectors.
///
/// Tests write one of these out as a legacy directory and then run the real
/// converter over it, so the code under test is the same code that runs in
/// production rather than a parallel implementation.
#[derive(Debug, Clone)]
pub struct Automaton {
    shape: Vec<u32>,
    initial_state: u32,
    /// `layers[layer][row][character]`
    layers: Vec<Vec<Vec<u32>>>,
}

impl Automaton {
    /// A new automaton whose every layer holds just the two reserved states.
    pub fn new(shape: Vec<u32>) -> Automaton {
        let layers = shape
            .iter()
            .map(|&s| vec![vec![0u32; s as usize], vec![1u32; s as usize]])
            .collect();
        Automaton {
            shape,
            initial_state: 0,
            layers,
        }
    }

    pub fn ndim(&self) -> usize {
        self.shape.len()
    }

    pub fn shape(&self) -> &[u32] {
        &self.shape
    }

    pub fn layer_size(&self, layer: usize) -> u64 {
        self.layers[layer].len() as u64
    }

    pub fn initial_state(&self) -> u32 {
        self.initial_state
    }

    pub fn set_initial_state(&mut self, state: u32) {
        self.initial_state = state;
    }

    /// Append an ordinary state to `layer`, returning its index.
    pub fn add_state(&mut self, layer: usize, transitions: Vec<u32>) -> u32 {
        assert_eq!(transitions.len(), self.shape[layer] as usize);
        let index = self.layers[layer].len() as u32;
        self.layers[layer].push(transitions);
        index
    }

    pub fn row(&self, layer: usize, row: u64) -> &[u32] {
        &self.layers[layer][row as usize]
    }

    pub fn set_row(&mut self, layer: usize, row: u64, transitions: Vec<u32>) {
        assert_eq!(transitions.len(), self.shape[layer] as usize);
        self.layers[layer][row as usize] = transitions;
    }

    /// Reference implementation of spec section 5, for cross-checking.
    pub fn accepts(&self, s: &[u32]) -> bool {
        let mut state = self.initial_state;
        for (i, &c) in s.iter().enumerate() {
            if state == 0 {
                return false;
            }
            if state == 1 {
                return true;
            }
            state = self.layers[i][state as usize][c as usize];
        }
        state == 1
    }

    /// Every string over this automaton's shape, in odometer order.
    pub fn all_strings(&self) -> Vec<Vec<u32>> {
        let mut out = vec![Vec::new()];
        for &s in &self.shape {
            let mut next = Vec::new();
            for prefix in &out {
                for c in 0..s {
                    let mut extended = prefix.clone();
                    extended.push(c);
                    next.push(extended);
                }
            }
            out = next;
        }
        out
    }

    /// Write this automaton in the legacy directory layout of `src/DFA.cpp`.
    pub fn write_legacy_dir(&self, dir: &Path) -> std::io::Result<()> {
        std::fs::create_dir_all(dir)?;
        std::fs::write(dir.join("initial_state"), self.initial_state.to_le_bytes())?;
        for (layer, rows) in self.layers.iter().enumerate() {
            let mut bytes = Vec::with_capacity(rows.len() * self.shape[layer] as usize * 4);
            for row in rows {
                for &v in row {
                    bytes.extend_from_slice(&v.to_le_bytes());
                }
            }
            std::fs::write(dir.join(format!("layer={layer}")), &bytes)?;
        }
        Ok(())
    }
}
