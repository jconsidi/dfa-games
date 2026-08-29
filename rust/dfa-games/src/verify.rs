//! Checking a solved DFA against the rules, position by position.
//!
//! Port of `src/verify_utils.cpp`.  Each verifier takes a DFA that claims
//! something about every position in it and checks that claim against
//! `Game::validate_moves` / `validate_result`, which know nothing about DFAs.
//!
//! # Terminal positions
//!
//! The C++ consults `game.get_positions_lost(side)` and `get_positions_won`
//! as a fast path before generating moves.  This does not, for three reasons:
//! those DFAs come from `load_or_build`, so depending on them would drag DFA
//! construction into a program that only reads DFAs; `won,side_to_move=N` is
//! the reject DFA for a normal play game, so the `won` half of it is dead code
//! anyway; and several scratch directories never had `lost,side_to_move=1`
//! built at all.
//!
//! Instead every position generates its moves once, and the empty move list is
//! the branch to the terminal check, where `validate_result` must return the
//! base case the DFA asserts.  That is *stricter* than the C++ on both
//! branches: a position wrongly listed in `lost` still has all of its moves
//! checked here, and a terminal position in a `losing` DFA has to actually
//! report a loss, where the C++ move loop passed it vacuously.
//!
//! What is given up is that the losing and winning verifiers no longer
//! incidentally cross check the `lost` DFA.  Running `verify-lost-sound` on
//! `lost,side_to_move=N` is what covers that, and it should stay part of the
//! routine.

use anyhow::{bail, Result};
use dfa_format::Dfa;
use rayon::prelude::*;

use crate::game::{Game, Position, Side};

/// Positions are enumerated in batches this big and each batch is checked in
/// parallel, as `DFAUtil::for_each_position` does.  Enumeration itself stays
/// on one thread: the iterator is inherently sequential.
const BATCH: usize = 4096;

/// Enumerate every position in `dfa` and run `check` over all of them,
/// returning how many were checked.
pub fn for_each_position<F>(dfa: &Dfa, check: F) -> Result<u64>
where
    F: Fn(&Position) -> Result<()> + Send + Sync,
{
    let mut count: u64 = 0;
    let mut batch: Vec<Vec<u32>> = Vec::with_capacity(BATCH);
    let mut positions = dfa.positions();

    loop {
        batch.clear();
        for position in positions.by_ref().take(BATCH) {
            batch.push(position?);
        }
        if batch.is_empty() {
            return Ok(count);
        }

        // Report the earliest failure in the batch, so which position gets
        // named does not depend on how rayon scheduled the work.
        let failure = batch
            .par_iter()
            .enumerate()
            .filter_map(|(index, position)| check(position).err().map(|e| (index, e)))
            .min_by_key(|(index, _)| *index);

        if let Some((_, e)) = failure {
            return Err(e);
        }

        count += batch.len() as u64;
    }
}

/// What the moves out of a non-terminal position have to satisfy.
pub enum Continuation<'a> {
    /// Nothing: the position was supposed to be terminal, so having a move at
    /// all is the failure.  `verify-lost-sound` and `verify-won-sound`.
    Terminal,

    /// Every move must be in this DFA.  A losing position has to hand the
    /// opponent a win however it moves.
    Every { dfa: &'a Dfa, label: &'a str },

    /// At least one move must be in this DFA.  A winning position needs one
    /// move that hands the opponent a loss.
    Any { dfa: &'a Dfa, label: &'a str },
}

/// What a DFA asserts about every position in it.
pub struct Claim<'a> {
    /// What `validate_result` must return at a terminal position.
    pub terminal_result: i32,
    pub continuation: Continuation<'a>,
}

fn result_name(result: i32) -> &'static str {
    match result {
        -1 => "lost",
        0 => "drawn",
        1 => "won",
        _ => "unknown",
    }
}

/// The one check every verifier makes, on one position.
pub fn check_position(
    game: &dyn Game,
    side_to_move: Side,
    position: &Position,
    claim: &Claim,
) -> Result<()> {
    let moves = game.validate_moves(side_to_move, position);
    let board = || game.position_to_string(position);

    if moves.is_empty() {
        let expected = claim.terminal_result;
        match game.validate_result(side_to_move, position, &moves) {
            Some(actual) if actual == expected => Ok(()),
            Some(actual) => bail!(
                "{}has no moves, but the result is {actual} ({}) where this DFA claims {expected} ({})",
                board(),
                result_name(actual),
                result_name(expected)
            ),
            None => bail!(
                "{}has no moves, but the result is not terminal",
                board()
            ),
        }
    } else {
        match &claim.continuation {
            Continuation::Terminal => bail!(
                "{}should be terminal, but has {} moves",
                board(),
                moves.len()
            ),

            Continuation::Every { dfa, label } => {
                for move_position in &moves {
                    if !dfa.accepts(move_position)? {
                        bail!(
                            "{}has a move to a position that is not in {label}:\n{}",
                            board(),
                            game.position_to_string(move_position)
                        );
                    }
                }
                Ok(())
            }

            Continuation::Any { dfa, label } => {
                for move_position in &moves {
                    if dfa.accepts(move_position)? {
                        return Ok(());
                    }
                }
                bail!(
                    "{}has {} moves and none of them is in {label}",
                    board(),
                    moves.len()
                );
            }
        }
    }
}

/// Check `claim` against every position in `dfa`.
pub fn verify_sound(
    game: &dyn Game,
    side_to_move: Side,
    dfa: &Dfa,
    label: &str,
    claim: &Claim,
) -> Result<u64> {
    // The count is f64, and exact below 2^53. Printed first because it is what
    // makes the progress of a long run legible, as in the C++.
    let expected_count = dfa_format::stats::count_accepted(dfa)?;
    println!(
        "verifying {} positions in {label} (side to move {side_to_move})",
        dfa_format::stats::format_positions(expected_count)
    );

    let verified = for_each_position(dfa, |position| {
        check_position(game, side_to_move, position, claim)
    })?;

    println!(
        "verified {verified} / {} positions in {label}",
        dfa_format::stats::format_positions(expected_count)
    );

    if verified as f64 != expected_count {
        bail!(
            "verified {verified} positions in {label} but it holds {}",
            dfa_format::stats::format_positions(expected_count)
        );
    }

    Ok(verified)
}

/// Every position is terminal and lost for the side to move.
pub fn verify_lost_sound(
    game: &dyn Game,
    side_to_move: Side,
    dfa: &Dfa,
    label: &str,
) -> Result<u64> {
    verify_sound(
        game,
        side_to_move,
        dfa,
        label,
        &Claim {
            terminal_result: -1,
            continuation: Continuation::Terminal,
        },
    )
}

/// Every position is terminal and won for the side to move.
pub fn verify_won_sound(
    game: &dyn Game,
    side_to_move: Side,
    dfa: &Dfa,
    label: &str,
) -> Result<u64> {
    verify_sound(
        game,
        side_to_move,
        dfa,
        label,
        &Claim {
            terminal_result: 1,
            continuation: Continuation::Terminal,
        },
    )
}

/// Every position is either lost or has all of its moves in `winning_prev`.
pub fn verify_losing_sound(
    game: &dyn Game,
    side_to_move: Side,
    losing_curr: &Dfa,
    losing_label: &str,
    winning_prev: &Dfa,
    winning_label: &str,
) -> Result<u64> {
    verify_sound(
        game,
        side_to_move,
        losing_curr,
        losing_label,
        &Claim {
            terminal_result: -1,
            continuation: Continuation::Every {
                dfa: winning_prev,
                label: winning_label,
            },
        },
    )
}

/// Every position is either won or has some move into `losing_prev`.
pub fn verify_winning_sound(
    game: &dyn Game,
    side_to_move: Side,
    winning_curr: &Dfa,
    winning_label: &str,
    losing_prev: &Dfa,
    losing_label: &str,
) -> Result<u64> {
    verify_sound(
        game,
        side_to_move,
        winning_curr,
        winning_label,
        &Claim {
            terminal_result: 1,
            continuation: Continuation::Any {
                dfa: losing_prev,
                label: losing_label,
            },
        },
    )
}
