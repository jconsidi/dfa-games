//! Reader, writer and validator for the DFA single-file format.
//!
//! See `FORMAT-DFA.md` at the repository root for the specification.  Section
//! references in the source refer to it.
//!
//! The binaries in this crate are thin wrappers: [`layout`] is the single
//! authority on where bytes go, [`write`] produces files and [`read`]
//! validates them.

mod bitset;
pub mod error;
pub mod iter;
pub mod layout;
pub mod read;
pub mod sample;
pub mod stats;
pub mod union;
pub mod write;

pub use error::{FormatError, Result, Violation};
pub use iter::Positions;
pub use layout::Layout;
pub use read::{validate, Dfa, Report, ValidateOptions};
pub use sample::{Rng, Sampler};
pub use stats::Stats;
pub use union::{sample_for_witness, verify_dfa_union, Caveat, UnionFailure, UnionStats};
pub use write::{write_automaton, Converted};

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
/// Tests build one of these and hand it to [`write_automaton`], so the file
/// under test comes from this crate's one writer rather than from a second,
/// weaker opinion about what conformance means.
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
}
