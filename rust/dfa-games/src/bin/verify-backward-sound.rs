//! Check a whole backward solve, ply by ply.  Rust port of
//! `src/verify_backward_sound.cpp`.
//!
//! Ply 0 is the base case: the `losing` DFA holds terminal lost positions and
//! the `winning` DFA holds terminal won positions.  Every later ply is
//! relative to the previous one for the other side.

use std::path::PathBuf;
use std::process::ExitCode;

use clap::Parser;
use dfa_format::{union, Dfa};
use dfa_games::{get_game, load, verify};

#[derive(Parser, Debug)]
#[command(about = "Verify every ply of a backward solve", long_about = None)]
struct Args {
    /// Scratch directory holding <game>/ and dfas_by_hash/
    #[arg(long, default_value = "scratch")]
    scratch: PathBuf,

    /// Game to verify, e.g. breakthrough_4x4
    game: String,

    /// Highest ply to verify
    #[arg(default_value_t = 1)]
    ply_max: u32,
}

fn losing_eq_name(side_to_move: u32, ply: u32) -> String {
    format!("backward,ply={ply:03},side={side_to_move},losing")
}

fn losing_max_name(side_to_move: u32, ply: u32) -> String {
    format!("backward,ply_max={ply:03},side={side_to_move},losing")
}

fn winning_eq_name(side_to_move: u32, ply: u32) -> String {
    format!("backward,ply={ply:03},side={side_to_move},winning")
}

fn winning_max_name(side_to_move: u32, ply: u32) -> String {
    format!("backward,ply_max={ply:03},side={side_to_move},winning")
}

fn main() -> ExitCode {
    let args = Args::parse();
    match run(&args) {
        Ok(()) => ExitCode::SUCCESS,
        Err(e) => {
            eprintln!("{e:#}");
            ExitCode::FAILURE
        }
    }
}

fn run(args: &Args) -> anyhow::Result<()> {
    let game = get_game(&args.game)?;
    let game = game.as_ref();

    let open = |name: &str| -> anyhow::Result<Dfa> { load::load(&args.scratch, game, name) };

    // ply 0: the base cases, which stand on the rules alone.
    for side_to_move in 0..2 {
        let lost = losing_max_name(side_to_move, 0);
        verify::verify_lost_sound(game, side_to_move, &open(&lost)?, &lost)?;

        let won = winning_max_name(side_to_move, 0);
        verify::verify_won_sound(game, side_to_move, &open(&won)?, &won)?;
    }

    // later ply: each one against the other side's previous ply.
    for ply in 1..=args.ply_max {
        for side_to_move in 0..2 {
            let losing_curr_name = losing_max_name(side_to_move, ply);
            let losing_curr = open(&losing_curr_name)?;

            let losing_prev_name = losing_max_name(side_to_move, ply - 1);
            let losing_prev = open(&losing_prev_name)?;

            let losing_new_name = losing_eq_name(side_to_move, ply);
            let losing_new = open(&losing_new_name)?;

            union::verify_dfa_union(
                &losing_curr,
                &losing_curr_name,
                &losing_prev,
                &losing_prev_name,
                &losing_new,
                &losing_new_name,
            )?;

            let winning_prev_name = winning_max_name(1 - side_to_move, ply - 1);
            let winning_prev = open(&winning_prev_name)?;

            verify::verify_losing_sound(
                game,
                side_to_move,
                &losing_new,
                &losing_new_name,
                &winning_prev,
                &winning_prev_name,
            )?;

            let winning_curr_name = winning_max_name(side_to_move, ply);
            let winning_curr = open(&winning_curr_name)?;

            let winning_prev_name = winning_max_name(side_to_move, ply - 1);
            let winning_prev = open(&winning_prev_name)?;

            let winning_new_name = winning_eq_name(side_to_move, ply);
            let winning_new = open(&winning_new_name)?;

            union::verify_dfa_union(
                &winning_curr,
                &winning_curr_name,
                &winning_prev,
                &winning_prev_name,
                &winning_new,
                &winning_new_name,
            )?;

            let losing_prev_name = losing_max_name(1 - side_to_move, ply - 1);
            let losing_prev = open(&losing_prev_name)?;

            verify::verify_winning_sound(
                game,
                side_to_move,
                &winning_new,
                &winning_new_name,
                &losing_prev,
                &losing_prev_name,
            )?;
        }
    }

    Ok(())
}
