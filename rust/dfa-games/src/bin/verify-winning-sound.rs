//! Check that every position in a winning DFA either is won or has some move
//! into the opponent's losing DFA from the previous ply.  Rust port of
//! `src/verify_winning_sound.cpp`.

use std::path::PathBuf;
use std::process::ExitCode;

use clap::Parser;
use dfa_games::{get_game, load, parse_side_to_move, verify};

#[derive(Parser, Debug)]
#[command(about = "Verify that a winning DFA only holds winning positions", long_about = None)]
struct Args {
    /// Scratch directory holding <game>/ and dfas_by_hash/
    #[arg(long, default_value = "scratch")]
    scratch: PathBuf,

    /// Game the DFAs belong to, e.g. breakthrough_4x4
    game: String,

    /// Winning DFA to check.  The side to move is read from its name.
    winning_curr: String,

    /// The other side's losing DFA from the previous ply
    losing_prev: String,
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
    let side_to_move = parse_side_to_move(&args.winning_curr)?;

    let winning_curr = load::load(&args.scratch, game.as_ref(), &args.winning_curr)?;
    let losing_prev = load::load(&args.scratch, game.as_ref(), &args.losing_prev)?;

    verify::verify_winning_sound(
        game.as_ref(),
        side_to_move,
        &winning_curr,
        &args.winning_curr,
        &losing_prev,
        &args.losing_prev,
    )?;
    Ok(())
}
