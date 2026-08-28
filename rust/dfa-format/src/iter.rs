//! Enumeration of the strings an automaton accepts, in lexicographic order.
//!
//! Port of `DFAIterator` in `src/DFA.cpp`.  The state chain is kept across
//! `next()` so advancing costs the carry depth rather than a fresh walk from
//! the initial state, which is what makes enumerating 10^7 positions
//! practical.
//!
//! The walk assumes the automaton is *trim*: every state it reaches other than
//! `STATE_REJECT` has at least one accepting continuation.  Every DFA this repo
//! produces is, and the C++ iterator asserts it.  Here a dead end is reported
//! as an error instead, which is why the item type is a `Result`: a file can be
//! structurally valid and still not trim, and a verifier that panicked on one
//! would be reporting the wrong thing.

use crate::error::{FormatError, Result};
use crate::layout::{STATE_ACCEPT, STATE_REJECT};
use crate::read::Dfa;

pub struct Positions<'a> {
    dfa: &'a Dfa,

    /// The current string, or empty once the walk is finished.
    characters: Vec<u32>,

    /// `states[layer]` is the state reached before consuming
    /// `characters[layer]`, with `states[0]` the initial state, so a live walk
    /// holds `ndim + 1` of them and `states[ndim]` is `STATE_ACCEPT`.
    states: Vec<u64>,

    done: bool,

    /// A dead end found while seeding or advancing.  Held rather than returned
    /// immediately so the string already in hand is still yielded first.
    error: Option<FormatError>,
}

impl<'a> Positions<'a> {
    pub(crate) fn new(dfa: &'a Dfa) -> Positions<'a> {
        let mut out = Positions {
            dfa,
            characters: Vec::with_capacity(dfa.layout().ndim()),
            states: Vec::with_capacity(dfa.layout().ndim() + 1),
            done: false,
            error: None,
        };

        if let Err(e) = out.seed() {
            out.error = Some(e);
        }

        out
    }

    fn dead_end(&self, layer: usize, state: u64) -> FormatError {
        FormatError::Other(format!(
            "automaton is not trim: state {state} in layer {layer} has no accepting continuation"
        ))
    }

    /// First character at `layer` out of `state` that is not a rejection,
    /// starting the scan at `from`.
    fn next_live(&self, layer: usize, state: u64, from: u32) -> Option<(u32, u64)> {
        let shape = self.dfa.layout().shape()[layer];
        for c in from..shape {
            let next = self.dfa.entry(layer, state, c);
            if next != u64::from(STATE_REJECT) {
                return Some((c, next));
            }
        }
        None
    }

    /// Walk to the lexicographically first accepted string.
    fn seed(&mut self) -> Result<()> {
        let ndim = self.dfa.layout().ndim();

        let mut state = self.dfa.header().initial_state;
        if state == u64::from(STATE_REJECT) {
            // accepts nothing, which is a normal DFA here: won,side_to_move=N
            // is the reject DFA for a normal play game.
            self.done = true;
            return Ok(());
        }

        self.states.push(state);
        for layer in 0..ndim {
            let (c, next) = self
                .next_live(layer, state, 0)
                .ok_or_else(|| self.dead_end(layer, state))?;
            self.characters.push(c);
            self.states.push(next);
            state = next;
        }

        if state != u64::from(STATE_ACCEPT) {
            return Err(FormatError::Other(format!(
                "walk of the first accepted string ended in state {state}, not {STATE_ACCEPT}"
            )));
        }

        Ok(())
    }

    /// Advance to the next accepted string, like incrementing a number with
    /// carrying except that characters leading to `STATE_REJECT` are skipped.
    fn advance(&mut self) -> Result<()> {
        let ndim = self.dfa.layout().ndim();

        // drop the accepting state the current string ended in
        self.states.pop();

        while !self.states.is_empty() {
            let layer = self.states.len() - 1;
            let state = self.states[layer];

            match self.next_live(layer, state, self.characters[layer] + 1) {
                Some((c, next)) => {
                    self.characters[layer] = c;
                    self.states.push(next);
                    break;
                }
                None => {
                    // no character left at this layer, so carry
                    self.characters.pop();
                    self.states.pop();
                }
            }
        }

        if self.states.is_empty() {
            self.done = true;
            self.characters.clear();
            return Ok(());
        }

        // fill forward from the character that was advanced
        for layer in self.characters.len()..ndim {
            let state = self.states[layer];
            let (c, next) = self
                .next_live(layer, state, 0)
                .ok_or_else(|| self.dead_end(layer, state))?;
            self.characters.push(c);
            self.states.push(next);
        }

        Ok(())
    }
}

impl Iterator for Positions<'_> {
    type Item = Result<Vec<u32>>;

    fn next(&mut self) -> Option<Result<Vec<u32>>> {
        if let Some(e) = self.error.take() {
            self.done = true;
            return Some(Err(e));
        }

        if self.done {
            return None;
        }

        let out = self.characters.clone();

        if let Err(e) = self.advance() {
            self.error = Some(e);
        }

        Some(Ok(out))
    }
}
