//! Breakthrough, row-wise square numbering.
//!
//! Port of `BreakthroughBase::validate_moves` and `position_to_string` from
//! `src/BreakthroughGame.cpp`.  Only `BreakthroughRowWiseGame`'s
//! `calculate_layer` is implemented; the column-wise variant is the same rules
//! over a transposed numbering and is not ported yet.
//!
//! Characters: 0 empty, 1 first player, 2 second player.  Side 0 moves toward
//! increasing row and wins by reaching row `height - 1`.

use crate::game::{Game, Position, Side};

pub struct BreakthroughGame {
    name: String,
    width: usize,
    height: usize,
    shape: Vec<u32>,
}

impl BreakthroughGame {
    pub fn new(width: usize, height: usize) -> BreakthroughGame {
        // The C++ constructor asserts these.
        assert!(width >= 1);
        assert!(height >= 4);

        BreakthroughGame {
            name: format!("breakthrough_{width}x{height}"),
            width,
            height,
            shape: vec![3u32; width * height],
        }
    }

    fn layer(&self, row: usize, column: usize) -> usize {
        row * self.width + column
    }
}

impl Game for BreakthroughGame {
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

        // The row the opponent had to reach to have already won, and the
        // direction this side advances.
        let (goal_row, row_delta): (usize, isize) = if side_to_move == 0 {
            (0, 1)
        } else {
            (self.height - 1, -1)
        };

        // Game already over: no moves, whatever is on the board.
        for column in 0..self.width {
            if position[self.layer(goal_row, column)] == hostile {
                return output;
            }
        }

        for row_from in 0..self.height {
            let row_to = row_from as isize + row_delta;
            if row_to < 0 || row_to >= self.height as isize {
                continue;
            }
            let row_to = row_to as usize;

            for col_from in 0..self.width {
                let layer_from = self.layer(row_from, col_from);
                if position[layer_from] != friendly {
                    continue;
                }

                for col_delta in -1isize..=1 {
                    let col_to = col_from as isize + col_delta;
                    if col_to < 0 || col_to >= self.width as isize {
                        continue;
                    }
                    let col_to = col_to as usize;

                    let layer_to = self.layer(row_to, col_to);
                    if position[layer_to] == friendly {
                        // cannot capture own pieces
                        continue;
                    }
                    if col_delta == 0 && position[layer_to] == hostile {
                        // cannot capture forward
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
        for row in 0..self.height {
            for column in 0..self.width {
                out.push(match position[self.layer(row, column)] {
                    0 => '.',
                    1 => 'w',
                    2 => 'b',
                    c => panic!("character {c} is outside the breakthrough alphabet"),
                });
            }
            out.push('\n');
        }
        out
    }
}
