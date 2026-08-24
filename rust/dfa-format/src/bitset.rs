//! A bit per state.
//!
//! Reachability passes need one flag per state of a layer, and layers here run
//! to tens of millions of states, so a byte per state would be wasteful and a
//! `Vec<bool>` on the largest layers would run to gigabytes.

pub(crate) struct Bitset {
    words: Vec<u64>,
    len: u64,
}

impl Bitset {
    pub(crate) fn new(len: u64) -> Bitset {
        let words = usize::try_from(len.div_ceil(64)).unwrap_or(usize::MAX);
        Bitset {
            words: vec![0; words],
            len,
        }
    }

    /// Out of range indices are ignored: callers are validating a file that
    /// may well be wrong, and a bad entry is reported by the bounds check
    /// rather than by panicking here.
    pub(crate) fn set(&mut self, index: u64) {
        if index >= self.len {
            return;
        }
        self.words[(index / 64) as usize] |= 1u64 << (index % 64);
    }

    pub(crate) fn get(&self, index: u64) -> bool {
        if index >= self.len {
            return false;
        }
        self.words[(index / 64) as usize] & (1u64 << (index % 64)) != 0
    }
}
