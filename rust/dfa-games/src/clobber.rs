//! Clobber.
//!
//! Rules from `GAMES.md`, not from the C++: `ClobberGame` has no
//! `validate_moves` to port — the declaration in `src/ClobberGame.h` is
//! commented out — so this is the only implementation of the rules anywhere,
//! and porting the move graph would only have re-stated whatever it says.
//!
//! Every piece sits on an orthogonal grid and the only move is onto an
//! adjacent enemy piece, which is replaced. Out of moves is a loss, so the
//! `validate_result` default is the whole result rule.
//!
//! Encoding, which is shared with the C++ and cannot be derived: `W * H`
//! layers of 3 characters, square `x + width * y`, 0 empty, 1 first player,
//! 2 second player. `position_to_string` prints `y` descending, matching
//! `ClobberGame::position_to_string`.

use crate::game::{Game, Position, Side};

pub struct ClobberGame {
    name: String,
    width: usize,
    height: usize,
    shape: Vec<u32>,
}

impl ClobberGame {
    pub fn new(width: usize, height: usize) -> ClobberGame {
        assert!(width >= 1);
        assert!(height >= 1);

        ClobberGame {
            name: format!("clobber_{width}x{height}"),
            width,
            height,
            shape: vec![3u32; width * height],
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

impl Game for ClobberGame {
    fn name(&self) -> &str {
        &self.name
    }

    fn shape(&self) -> &[u32] {
        &self.shape
    }

    fn validate_moves(&self, side_to_move: Side, position: &Position) -> Vec<Vec<u32>> {
        let mut output = Vec::new();

        let friendly = 1 + side_to_move;
        let hostile = 2 - side_to_move;

        for y in 0..self.height {
            for x in 0..self.width {
                let layer_from = self.layer(x, y);
                if position[layer_from] != friendly {
                    continue;
                }

                for layer_to in self.neighbors(x, y) {
                    // The only legal destination is an enemy piece: there is
                    // no non-capturing move in clobber.
                    if position[layer_to] != hostile {
                        continue;
                    }

                    let mut move_out = position.to_vec();
                    move_out[layer_from] = 0;
                    move_out[layer_to] = friendly;
                    output.push(move_out);
                }
            }
        }

        output
    }

    fn position_to_string(&self, position: &Position) -> String {
        let mut out = String::with_capacity(self.height * (self.width + 1));
        for y in (0..self.height).rev() {
            for x in 0..self.width {
                out.push(match position[self.layer(x, y)] {
                    0 => '.',
                    1 => 'w',
                    2 => 'b',
                    c => panic!("character {c} is outside the clobber alphabet"),
                });
            }
            out.push('\n');
        }
        out
    }
}
