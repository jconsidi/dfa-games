//! Hex on an `N x N` rhombus.
//!
//! Rules from `GAMES.md`. The C++ takes them from `config/hex_N/` instead of
//! code, and nothing is read here at run time: running the verifiers over the
//! solver's own output is what holds the two together.
//!
//! Normal play, so `validate_result` is the default: no moves is a loss. Only
//! the *opponent's* chain stops the side to move, as in tictactoe. A board
//! where the side to move already has a chain cannot be reached in play, but
//! backward solving still covers it.
//!
//! Encoding, which is shared with the C++ and cannot be derived: `N * N`
//! layers of 3 characters, cell `row * n + col`, 0 empty, 1 first player,
//! 2 second player. The first player joins row 0 to row `n - 1`, the second
//! joins column 0 to column `n - 1`. Each row sits half a cell right of the
//! one above, so the neighbours of `(row, col)` are `(row, col +- 1)`,
//! `(row - 1, col)`, `(row - 1, col + 1)`, `(row + 1, col - 1)` and
//! `(row + 1, col)`.

use crate::game::{Game, Position, Side};

pub struct HexGame {
    name: String,
    n: usize,
    shape: Vec<u32>,

    /// The neighbours of each cell. Depends only on `n`, so it is built once.
    neighbours: Vec<Vec<usize>>,
}

fn build_neighbours(n: usize) -> Vec<Vec<usize>> {
    const OFFSETS: [(isize, isize); 6] = [(-1, 0), (-1, 1), (0, -1), (0, 1), (1, -1), (1, 0)];

    let mut output = Vec::with_capacity(n * n);
    for row in 0..n {
        for col in 0..n {
            let cell_neighbours = OFFSETS
                .iter()
                .filter_map(|&(dr, dc)| {
                    let row2 = row.checked_add_signed(dr)?;
                    let col2 = col.checked_add_signed(dc)?;
                    (row2 < n && col2 < n).then_some(row2 * n + col2)
                })
                .collect();
            output.push(cell_neighbours);
        }
    }

    output
}

impl HexGame {
    pub fn new(n: usize) -> HexGame {
        assert!(n >= 1);

        HexGame {
            name: format!("hex_{n}"),
            n,
            shape: vec![3u32; n * n],
            neighbours: build_neighbours(n),
        }
    }

    /// Whether `side` has a chain of its stones joining its two edges.
    fn has_chain(&self, position: &Position, side: Side) -> bool {
        let n = self.n;
        let stone = side + 1;

        // the cell on line `i` of the edge at `depth` from the start edge.
        let edge_cell = |depth: usize, i: usize| {
            if side == 0 {
                depth * n + i
            } else {
                i * n + depth
            }
        };

        let mut reached = vec![false; n * n];
        let mut pending: Vec<usize> = (0..n)
            .map(|i| edge_cell(0, i))
            .filter(|&cell| position[cell] == stone)
            .collect();
        for &cell in &pending {
            reached[cell] = true;
        }

        while let Some(cell) = pending.pop() {
            for &next in &self.neighbours[cell] {
                if position[next] == stone && !reached[next] {
                    reached[next] = true;
                    pending.push(next);
                }
            }
        }

        (0..n).any(|i| reached[edge_cell(n - 1, i)])
    }
}

impl Game for HexGame {
    fn name(&self) -> &str {
        &self.name
    }

    fn shape(&self) -> &[u32] {
        &self.shape
    }

    fn validate_moves(&self, side_to_move: Side, position: &Position) -> Vec<Vec<u32>> {
        let mut output = Vec::new();

        // The opponent completing a chain ended the game on their move.
        if self.has_chain(position, 1 - side_to_move) {
            return output;
        }

        for (cell, &character) in position.iter().enumerate() {
            if character != 0 {
                continue;
            }

            let mut move_out = position.to_vec();
            move_out[cell] = side_to_move + 1;
            output.push(move_out);
        }

        output
    }

    fn position_to_string(&self, position: &Position) -> String {
        let mut out = String::new();

        for row in 0..self.n {
            out.push_str(&" ".repeat(row));
            for col in 0..self.n {
                if col > 0 {
                    out.push(' ');
                }
                out.push(match position[row * self.n + col] {
                    0 => '.',
                    1 => '0',
                    2 => '1',
                    c => panic!("character {c} is outside the hex alphabet"),
                });
            }
            out.push('\n');
        }

        out
    }
}
