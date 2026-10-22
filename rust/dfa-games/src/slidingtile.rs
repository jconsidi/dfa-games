//! Sliding tile puzzles.
//!
//! Rules from `GAMES.md`, not from the C++. The only move slides a tile into
//! an orthogonally adjacent gap. There is one player, so `sides` is 1 and the
//! side to move is always 0.
//!
//! A puzzle has no opponent to lose to and never runs out of moves, so
//! `validate_result` never reports a result, as `Puzzle::validate_outcome`
//! does not. Being solved is a property of the position, not the end of a
//! two-player game, and the verifiers refuse one-player games outright.
//!
//! Encoding, which is shared with the C++ and cannot be derived: `W * H`
//! layers of `W * H` characters, square `x + width * y`, 0 the gap and `n` the
//! tile numbered `n`. `position_to_string` prints `y` descending with two
//! digits per tile, matching `SlidingTilePuzzle::position_to_string`.

use crate::game::{Game, Position, Side};

pub struct SlidingTilePuzzle {
    name: String,
    width: usize,
    height: usize,
    shape: Vec<u32>,
}

impl SlidingTilePuzzle {
    pub fn new(width: usize, height: usize) -> SlidingTilePuzzle {
        assert!(width >= 1);
        assert!(height >= 1);

        let squares = width * height;
        let alphabet = u32::try_from(squares).expect("board too large for the alphabet");

        SlidingTilePuzzle {
            name: format!("slidingtile_{width}x{height}"),
            width,
            height,
            shape: vec![alphabet; squares],
        }
    }

    fn layer(&self, x: usize, y: usize) -> usize {
        x + self.width * y
    }

    /// The orthogonal neighbours of `(x, y)`, in ascending layer order.
    fn neighbors(&self, x: usize, y: usize) -> Vec<usize> {
        let mut out = Vec::with_capacity(4);
        if y > 0 {
            out.push(self.layer(x, y - 1));
        }
        if x > 0 {
            out.push(self.layer(x - 1, y));
        }
        if x + 1 < self.width {
            out.push(self.layer(x + 1, y));
        }
        if y + 1 < self.height {
            out.push(self.layer(x, y + 1));
        }
        out
    }
}

impl Game for SlidingTilePuzzle {
    fn name(&self) -> &str {
        &self.name
    }

    fn shape(&self) -> &[u32] {
        &self.shape
    }

    fn sides(&self) -> u32 {
        1
    }

    fn validate_moves(&self, side_to_move: Side, position: &Position) -> Vec<Vec<u32>> {
        assert_eq!(side_to_move, 0, "a puzzle has only side 0");

        let mut output = Vec::new();

        for y in 0..self.height {
            for x in 0..self.width {
                let gap = self.layer(x, y);
                if position[gap] != 0 {
                    continue;
                }

                for tile in self.neighbors(x, y) {
                    // A gap next to the gap has no tile to slide.
                    if position[tile] == 0 {
                        continue;
                    }

                    let mut move_out = position.to_vec();
                    move_out.swap(gap, tile);
                    output.push(move_out);
                }
            }
        }

        output
    }

    fn validate_result(
        &self,
        _side_to_move: Side,
        _position: &Position,
        _moves: &[Vec<u32>],
    ) -> Option<i32> {
        None
    }

    fn position_to_string(&self, position: &Position) -> String {
        let mut out = String::with_capacity(self.height * (2 * self.width + 1));
        for y in (0..self.height).rev() {
            for x in 0..self.width {
                match position[self.layer(x, y)] {
                    0 => out.push_str("  "),
                    c => out.push_str(&format!("{c:02}")),
                }
            }
            out.push('\n');
        }
        out
    }
}
