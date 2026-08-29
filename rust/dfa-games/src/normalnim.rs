//! Nim, normal play.
//!
//! Rules from `GAMES.md`, not from the C++: `NormalNimGame` has no
//! `validate_moves` to port, so this is the only implementation of the rules
//! anywhere.
//!
//! A move takes any positive number of tokens from exactly one heap. Both
//! sides always have the same moves — nothing on the board belongs to either
//! of them — so `side_to_move` is unused. Out of moves is a loss, which is
//! what "normal" distinguishes from misère nim, so the `validate_result`
//! default is the whole result rule.
//!
//! Encoding, which is shared with the C++ and cannot be derived:
//! `num_heaps` layers of `heap_max + 1` characters, one layer per heap, the
//! character being the number of tokens left in it.

use crate::game::{Game, Position, Side};

pub struct NormalNimGame {
    name: String,
    shape: Vec<u32>,
}

impl NormalNimGame {
    pub fn new(num_heaps: usize, heap_max: u32) -> NormalNimGame {
        assert!(num_heaps >= 1);

        NormalNimGame {
            name: format!("normalnim_{num_heaps}x{heap_max}"),
            shape: vec![heap_max + 1; num_heaps],
        }
    }
}

impl Game for NormalNimGame {
    fn name(&self) -> &str {
        &self.name
    }

    fn shape(&self) -> &[u32] {
        &self.shape
    }

    fn validate_moves(&self, _side_to_move: Side, position: &Position) -> Vec<Vec<u32>> {
        let mut output = Vec::new();

        for (heap, &tokens) in position.iter().enumerate() {
            // Taking `tokens - after` tokens, for every positive amount up to
            // the whole heap.
            for after in 0..tokens {
                let mut move_out = position.to_vec();
                move_out[heap] = after;
                output.push(move_out);
            }
        }

        output
    }

    fn position_to_string(&self, position: &Position) -> String {
        let mut out = String::new();
        for (heap, tokens) in position.iter().enumerate() {
            if heap > 0 {
                out.push(',');
            }
            out.push_str(&tokens.to_string());
        }
        out
    }
}
