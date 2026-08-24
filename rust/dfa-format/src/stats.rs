//! Counting and size statistics for a `.dfa` file.
//!
//! The position count mirrors `DFA::size()` (src/DFA.cpp:874): a backward pass
//! that gives each state the number of accepted continuations from it, summed
//! over the alphabet at each step.
//!
//! The reserved rows need no special case.  Row 0 is all zeros, so state 0
//! accumulates nothing; row 1 is all ones, so state 1 accumulates the product
//! of the remaining alphabet sizes, which is exactly what "accept every
//! continuation" means.  Counting through them therefore agrees with the early
//! returns of spec section 5.
//!
//! Counts are `f64`, as in the C++.  A DFA over 64 ternary characters can
//! accept more strings than any fixed width integer holds, so these are
//! approximate above 2^53 and are printed accordingly.

use crate::error::{FormatError, Result};
use crate::layout::Layout;
use crate::read::Dfa;

#[derive(Debug, Clone)]
pub struct LayerStats {
    pub layer: usize,
    pub shape: u32,
    pub states: u64,
    /// Bytes per stored entry, derived from the next layer's size.
    pub width: u8,
    pub transitions: u64,
    pub bytes: u64,
}

#[derive(Debug, Clone)]
pub struct Stats {
    pub ndim: usize,
    /// Sum of the layer sizes, reserved states included, as `DFA::states()`.
    pub states: u64,
    pub transitions: u64,
    pub file_len: u64,
    pub header_and_tables: u64,
    pub transition_bytes: u64,
    pub padding_bytes: u64,
    /// Accepted strings, or `None` when the count was not requested.
    pub positions: Option<f64>,
    pub canonical: bool,
    pub initial_state: u64,
    pub layers: Vec<LayerStats>,
}

impl Stats {
    pub fn collect(dfa: &Dfa, count_positions: bool) -> Result<Stats> {
        let lay = dfa.layout();
        let ndim = lay.ndim();

        let mut layers = Vec::with_capacity(ndim);
        let mut transitions = 0u64;
        let mut transition_bytes = 0u64;
        for layer in 0..ndim {
            let states = lay.layer_size()[layer];
            let shape = lay.shape()[layer];
            let count = states * u64::from(shape);
            transitions += count;
            transition_bytes += lay.block_bytes()[layer];
            layers.push(LayerStats {
                layer,
                shape,
                states,
                width: lay.width()[layer],
                transitions: count,
                bytes: lay.block_bytes()[layer],
            });
        }

        let header_and_tables = lay.tables_end();
        let file_len = lay.file_len();

        Ok(Stats {
            ndim,
            states: lay.total_states(),
            transitions,
            file_len,
            header_and_tables,
            transition_bytes,
            padding_bytes: file_len - header_and_tables - transition_bytes,
            positions: count_positions.then(|| count_accepted(dfa)).transpose()?,
            canonical: dfa.header().canonical(),
            initial_state: dfa.header().initial_state,
            layers,
        })
    }

    /// Run-length summary of the shape, so a 65 layer chess shape stays on one
    /// line.
    pub fn shape_summary(&self) -> String {
        run_length(self.layers.iter().map(|l| l.shape))
    }

    pub fn width_summary(&self) -> String {
        run_length(self.layers.iter().map(|l| u32::from(l.width)))
    }
}

fn run_length(values: impl Iterator<Item = u32>) -> String {
    let mut parts: Vec<String> = Vec::new();
    let mut iter = values.peekable();
    while let Some(value) = iter.next() {
        let mut run = 1usize;
        while iter.peek() == Some(&value) {
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

/// Number of strings the automaton accepts.
pub fn count_accepted(dfa: &Dfa) -> Result<f64> {
    let lay: &Layout = dfa.layout();

    // The terminal pseudo-layer: state 0 accepts nothing, state 1 accepts the
    // empty continuation.
    let mut next: Vec<f64> = vec![0.0, 1.0];

    for layer in (0..lay.ndim()).rev() {
        let states = usize::try_from(lay.layer_size()[layer]).map_err(|_| {
            FormatError::Other(format!(
                "layer {layer} has {} states, too many to count in memory",
                lay.layer_size()[layer]
            ))
        })?;
        let shape = lay.shape()[layer];

        let mut current = vec![0.0f64; states];
        for (row, slot) in current.iter_mut().enumerate() {
            let mut total = 0.0;
            for c in 0..shape {
                total += next[dfa.entry(layer, row as u64, c) as usize];
            }
            *slot = total;
        }
        next = current;
    }

    Ok(next[dfa.header().initial_state as usize])
}

/// Print a count the way a reader can use it: exactly while it is exact, and
/// in scientific notation once `f64` has stopped being able to be.
pub fn format_positions(value: f64) -> String {
    const EXACT_LIMIT: f64 = 9007199254740992.0; // 2^53
    if value < EXACT_LIMIT && value.fract() == 0.0 {
        format!("{}", value as u64)
    } else {
        format!("{value:.6e}")
    }
}
