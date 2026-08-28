//! Check that every position in a losing DFA either is lost or has all of its
//! moves into the opponent's winning DFA from the previous ply.  Rust port of
//! `src/verify_losing_sound.cpp`.

use std::path::PathBuf;
use std::process::ExitCode;

use clap::Parser;
use dfa_games::{get_game, load, parse_side_to_move, verify};

#[derive(Parser, Debug)]
#[command(about = "Verify that a losing DFA only holds losing positions", long_about = None)]
struct Args {
    /// Scratch directory holding <game>/ and dfas_by_hash/
    #[arg(long, default_value = "scratch")]
    scratch: PathBuf,

    /// Game the DFAs belong to, e.g. breakthrough_4x4
    game: String,

    /// Losing DFA to check.  The side to move is read from its name.
    losing_curr: String,

    /// The other side's winning DFA from the previous ply
    winning_prev: String,
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
    let side_to_move = parse_side_to_move(&args.losing_curr)?;

    let losing_curr = load::load(&args.scratch, game.as_ref(), &args.losing_curr)?;
    let winning_prev = load::load(&args.scratch, game.as_ref(), &args.winning_prev)?;

    verify::verify_losing_sound(
        game.as_ref(),
        side_to_move,
        &losing_curr,
        &args.losing_curr,
        &winning_prev,
        &args.winning_prev,
    )?;
    Ok(())
}
