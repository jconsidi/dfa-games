//! Amazons.
//!
//! Port of `AmazonsGame::validate_moves` and `position_to_string` from
//! `src/AmazonsGame.cpp`, plus the queen move table that
//! `GameUtil::_build_queen_moves` builds in `src/GameUtil.cpp`.
//!
//! Characters: 0 empty, 1 first player, 2 second player, 3 burned.  A move is
//! a queen move followed by a shot from the destination, and both slides need
//! every square between to be empty.
//!
//! Square numbering is `x + width * y`, not breakthrough's `row * width +
//! column`, and `position_to_string` prints `y` descending where breakthrough
//! prints rows ascending.  The two games share no geometry.

use crate::game::{Game, Position, Side};

/// One queen move out of some square: where it lands, and the squares it
/// passes over.
type QueenMove = (usize, Vec<usize>);

pub struct AmazonsGame {
    name: String,
    width: usize,
    height: usize,
    shape: Vec<u32>,

    /// Queen moves grouped by starting layer.  Depends only on the board size,
    /// so it is built once here rather than per position, as the C++ does.
    queen_moves: Vec<Vec<QueenMove>>,
}

fn signum(x: isize) -> isize {
    x.signum()
}

/// `GameUtil::_build_queen_moves`, grouped by from-layer the way
/// `build_queen_moves_by_layer` groups it.  From-square and to-square both
/// ascending, which is the order `validate_moves` emits moves in.
fn build_queen_moves(width: usize, height: usize) -> Vec<Vec<QueenMove>> {
    let squares = width * height;
    let mut output: Vec<Vec<QueenMove>> = vec![Vec::new(); squares];

    for (square_from, moves) in output.iter_mut().enumerate() {
        let x_from = (square_from % width) as isize;
        let y_from = (square_from / width) as isize;

        for square_to in 0..squares {
            if square_to == square_from {
                continue;
            }

            let x_to = (square_to % width) as isize;
            let y_to = (square_to / width) as isize;

            let same_row = y_from == y_to;
            let same_column = x_from == x_to;
            let same_diagonal = (x_from - x_to).abs() == (y_from - y_to).abs();
            if !(same_row || same_column || same_diagonal) {
                continue;
            }

            let x_delta = signum(x_to - x_from);
            let y_delta = signum(y_to - y_from);
            let distance = if same_column {
                (y_from - y_to).abs()
            } else {
                (x_from - x_to).abs()
            };

            let mut between = Vec::new();
            for i in 1..distance {
                let x_mid = x_from + i * x_delta;
                let y_mid = y_from + i * y_delta;
                between.push((x_mid + (width as isize) * y_mid) as usize);
            }

            moves.push((square_to, between));
        }
    }

    output
}

impl AmazonsGame {
    pub fn new(width: usize, height: usize) -> AmazonsGame {
        AmazonsGame {
            name: format!("amazons_{width}x{height}"),
            width,
            height,
            shape: vec![4u32; width * height],
            queen_moves: build_queen_moves(width, height),
        }
    }

    /// Queen moves out of `layer`, for tests to check the table directly.
    pub fn queen_moves(&self, layer: usize) -> &[QueenMove] {
        &self.queen_moves[layer]
    }
}

impl Game for AmazonsGame {
    fn name(&self) -> &str {
        &self.name
    }

    fn shape(&self) -> &[u32] {
        &self.shape
    }

    fn validate_moves(&self, side_to_move: Side, position: &Position) -> Vec<Vec<u32>> {
        let mut output = Vec::new();
        let friendly = 1 + side_to_move;

        let clear = |between: &[usize]| between.iter().all(|&layer| position[layer] == 0);

        for from_layer in 0..position.len() {
            if position[from_layer] != friendly {
                continue;
            }

            for (to_layer, between) in &self.queen_moves[from_layer] {
                let to_layer = *to_layer;
                if position[to_layer] != 0 {
                    continue;
                }
                if !clear(between) {
                    continue;
                }

                for (shot_layer, shot_between) in &self.queen_moves[to_layer] {
                    let shot_layer = *shot_layer;

                    // The queen has left from_layer, so it is empty for the
                    // shot: both as a target and as a square to shoot over.
                    if shot_layer != from_layer && position[shot_layer] != 0 {
                        continue;
                    }
                    if !shot_between
                        .iter()
                        .all(|&layer| layer == from_layer || position[layer] == 0)
                    {
                        continue;
                    }

                    let mut move_out = position.to_vec();
                    move_out[from_layer] = 0;
                    move_out[to_layer] = friendly;
                    move_out[shot_layer] = 3;
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
                out.push(match position[x + self.width * y] {
                    0 => '.',
                    1 => 'w',
                    2 => 'b',
                    3 => '*',
                    c => panic!("character {c} is outside the amazons alphabet"),
                });
            }
            out.push('\n');
        }
        out
    }
}
