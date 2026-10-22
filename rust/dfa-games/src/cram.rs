//! Cram.
//!
//! Rules from `GAMES.md`, not from the C++. Every move covers two orthogonally
//! adjacent empty squares with a domino, and the dominoes belong to no one, so
//! both sides have the same moves. Out of moves is a loss, so the
//! `validate_result` default is the whole result rule.
//!
//! Encoding, which is shared with the C++ and cannot be derived: `W * H`
//! layers of 2 characters, square `x + width * y`, 0 empty, 1 covered.
//! `position_to_string` prints `y` descending, matching
//! `CramGame::position_to_string`.

use crate::game::{Game, Position, Side};

pub struct CramGame {
    name: String,
    width: usize,
    height: usize,
    shape: Vec<u32>,
}

impl CramGame {
    pub fn new(width: usize, height: usize) -> CramGame {
        assert!(width >= 1);
        assert!(height >= 1);

        CramGame {
            name: format!("cram_{width}x{height}"),
            width,
            height,
            shape: vec![2u32; width * height],
        }
    }

    fn layer(&self, x: usize, y: usize) -> usize {
        x + self.width * y
    }
}

impl Game for CramGame {
    fn name(&self) -> &str {
        &self.name
    }

    fn shape(&self) -> &[u32] {
        &self.shape
    }

    fn validate_moves(&self, _side_to_move: Side, position: &Position) -> Vec<Vec<u32>> {
        let mut output = Vec::new();

        for y in 0..self.height {
            for x in 0..self.width {
                let layer1 = self.layer(x, y);
                if position[layer1] != 0 {
                    continue;
                }

                // Each domino is generated once, from its lower-numbered
                // square: the right and upper neighbours only.
                let mut partners = Vec::with_capacity(2);
                if x + 1 < self.width {
                    partners.push(self.layer(x + 1, y));
                }
                if y + 1 < self.height {
                    partners.push(self.layer(x, y + 1));
                }

                for layer2 in partners {
                    if position[layer2] != 0 {
                        continue;
                    }

                    let mut move_out = position.to_vec();
                    move_out[layer1] = 1;
                    move_out[layer2] = 1;
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
                    1 => '#',
                    c => panic!("character {c} is outside the cram alphabet"),
                });
            }
            out.push('\n');
        }
        out
    }
}
