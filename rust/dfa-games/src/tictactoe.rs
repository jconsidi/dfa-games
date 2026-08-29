//! Tic-tac-toe on an `N x N` board, `N` in a row to win.
//!
//! Rules from `GAMES.md`. The C++ takes them from `config/tictactoe_N/`
//! instead of code, and nothing is read here at run time: this states the
//! rules for general `N`, and running the verifiers over the solver's own
//! output is what holds the two together.
//!
//! Not normal play, so `validate_result` is implemented rather than
//! inherited: out of moves is a loss when the opponent has a line and a draw
//! when the board is simply full.
//!
//! Only the *opponent's* line stops the side to move. A board where the side
//! to move already has a line cannot be reached in play — that line would have
//! ended the game on their previous turn — so `GAMES.md` says nothing about
//! it, while backward solving still covers it. Treating it as game over would
//! turn every such board into a spurious refutation, so it is left playable,
//! which is also what the C++ does.
//!
//! Encoding, which is shared with the C++ and cannot be derived: `N * N`
//! layers of 3 characters, square `row * n + col`, 0 empty, 1 first player,
//! 2 second player.

use crate::game::{Game, Position, Side};

pub struct TicTacToeGame {
    name: String,
    n: usize,
    shape: Vec<u32>,

    /// Every winning line as a list of squares: the `n` rows, the `n` columns
    /// and both diagonals. Depends only on `n`, so it is built once.
    lines: Vec<Vec<usize>>,
}

fn build_lines(n: usize) -> Vec<Vec<usize>> {
    let mut lines = Vec::with_capacity(2 * n + 2);

    for row in 0..n {
        lines.push((0..n).map(|col| row * n + col).collect());
    }
    for col in 0..n {
        lines.push((0..n).map(|row| row * n + col).collect());
    }
    lines.push((0..n).map(|i| i * n + i).collect());
    lines.push((0..n).map(|i| i * n + (n - 1 - i)).collect());

    lines
}

impl TicTacToeGame {
    pub fn new(n: usize) -> TicTacToeGame {
        assert!(n >= 1);

        TicTacToeGame {
            name: format!("tictactoe_{n}"),
            n,
            shape: vec![3u32; n * n],
            lines: build_lines(n),
        }
    }

    /// Whether `character` fills any line.
    fn has_line(&self, position: &Position, character: u32) -> bool {
        self.lines
            .iter()
            .any(|line| line.iter().all(|&square| position[square] == character))
    }

    pub fn lines(&self) -> &[Vec<usize>] {
        &self.lines
    }
}

impl Game for TicTacToeGame {
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

        // The opponent completing a line ended the game on their move.
        if self.has_line(position, hostile) {
            return output;
        }

        for (square, &character) in position.iter().enumerate() {
            if character != 0 {
                continue;
            }

            let mut move_out = position.to_vec();
            move_out[square] = friendly;
            output.push(move_out);
        }

        output
    }

    /// The board decides, not the move list: both a completed enemy line and a
    /// full board leave no moves, and they are different results.
    fn validate_result(
        &self,
        side_to_move: Side,
        position: &Position,
        _moves: &[Vec<u32>],
    ) -> Option<i32> {
        if self.has_line(position, 2 - side_to_move) {
            return Some(-1);
        }

        if position.iter().all(|&character| character != 0) {
            return Some(0);
        }

        None
    }

    fn position_to_string(&self, position: &Position) -> String {
        let mut out = String::new();

        for row in 0..self.n {
            for col in 0..self.n {
                out.push(match position[row * self.n + col] {
                    0 => ' ',
                    1 => '0',
                    2 => '1',
                    c => panic!("character {c} is outside the tictactoe alphabet"),
                });
                if col + 1 < self.n {
                    out.push('|');
                }
            }
            out.push('\n');

            if row + 1 < self.n {
                for i in 0..(2 * self.n - 1) {
                    out.push(if i % 2 == 0 { '-' } else { '+' });
                }
                out.push('\n');
            }
        }

        out
    }
}
