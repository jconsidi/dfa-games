//! Uniform sampling of the strings a DFA accepts.
//!
//! Used as a cheap pre-filter in front of exact checks: a sampled string that
//! behaves wrongly is a refutation witness that can be re-checked in O(n) by
//! anything, including a reader that does not trust this crate.
//!
//! Sampling is uniform over the language, not over paths: the choice at each
//! layer is weighted by how many accepted suffixes each successor leads to.

use crate::error::{FormatError, Result};
use crate::layout::STATE_ACCEPT;
use crate::read::Dfa;

/// SplitMix64.  Deterministic on purpose — a witness that only shows up on
/// some runs is nearly useless, so the seed is an input and the same seed
/// gives the same strings.
pub struct Rng(u64);

impl Rng {
    pub fn new(seed: u64) -> Rng {
        Rng(seed)
    }

    fn next_u64(&mut self) -> u64 {
        self.0 = self.0.wrapping_add(0x9E37_79B9_7F4A_7C15);
        let mut z = self.0;
        z = (z ^ (z >> 30)).wrapping_mul(0xBF58_476D_1CE4_E5B9);
        z = (z ^ (z >> 27)).wrapping_mul(0x94D0_49BB_1331_11EB);
        z ^ (z >> 31)
    }

    /// Uniform in [0, 1).
    fn next_f64(&mut self) -> f64 {
        (self.next_u64() >> 11) as f64 * (1.0 / (1u64 << 53) as f64)
    }
}

/// The per-state suffix counts a uniform sample needs.
///
/// This is `stats::count_accepted`'s dynamic program with every layer kept
/// instead of only the running one, so it costs memory proportional to the
/// automaton's total states rather than its largest layer.
pub struct Sampler<'a> {
    dfa: &'a Dfa,
    /// `counts[layer][state]` — accepted suffixes from that state onward.
    /// `counts[ndim]` is the terminal pseudo-layer, `[0.0, 1.0]`.
    counts: Vec<Vec<f64>>,
}

impl<'a> Sampler<'a> {
    pub fn new(dfa: &'a Dfa) -> Result<Sampler<'a>> {
        let layout = dfa.layout();
        let ndim = layout.ndim();

        let mut counts: Vec<Vec<f64>> = vec![Vec::new(); ndim + 1];
        counts[ndim] = vec![0.0, 1.0];

        for layer in (0..ndim).rev() {
            let states = usize::try_from(layout.layer_size()[layer]).map_err(|_| {
                FormatError::Other(format!(
                    "layer {layer} has {} states, too many to sample from in memory",
                    layout.layer_size()[layer]
                ))
            })?;
            let shape = layout.shape()[layer];

            let mut current = vec![0.0f64; states];
            for (row, slot) in current.iter_mut().enumerate() {
                let mut total = 0.0;
                for c in 0..shape {
                    total += counts[layer + 1][dfa.entry(layer, row as u64, c) as usize];
                }
                *slot = total;
            }
            counts[layer] = current;
        }

        Ok(Sampler { dfa, counts })
    }

    /// How many strings the automaton accepts.
    pub fn total(&self) -> f64 {
        self.counts[0][self.dfa.header().initial_state as usize]
    }

    /// One string drawn uniformly from the language, or `None` when the
    /// language is empty.
    pub fn sample(&self, rng: &mut Rng) -> Option<Vec<u32>> {
        let layout = self.dfa.layout();
        let ndim = layout.ndim();

        let mut state = self.dfa.header().initial_state;
        if self.counts[0][state as usize] <= 0.0 {
            return None;
        }

        let mut out = Vec::with_capacity(ndim);
        for layer in 0..ndim {
            let shape = layout.shape()[layer];
            let mut target = rng.next_f64() * self.counts[layer][state as usize];

            // Walk the characters, subtracting each one's share. The fallback
            // matters: floating point drift can leave `target` just past the
            // end, and dropping out of the loop with no character chosen would
            // produce a string the automaton does not accept.
            let mut chosen = None;
            for c in 0..shape {
                let next = self.dfa.entry(layer, state, c);
                let weight = self.counts[layer + 1][next as usize];
                if weight <= 0.0 {
                    continue;
                }
                target -= weight;
                if target < 0.0 {
                    chosen = Some((c, next));
                    break;
                }
                chosen = Some((c, next));
            }

            let (c, next) = chosen?;
            out.push(c);
            state = next;
        }

        debug_assert_eq!(state, u64::from(STATE_ACCEPT));
        Some(out)
    }
}
