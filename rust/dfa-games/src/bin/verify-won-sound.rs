//! Check that every position in a DFA is terminal and won for the side to
//! move.  Rust port of `src/verify_won_sound.cpp`.

use std::path::PathBuf;
use std::process::ExitCode;

use clap::Parser;
use dfa_games::{get_game, load, parse_side_to_move, verify};

#[derive(Parser, Debug)]
#[command(about = "Verify that a DFA holds only terminal won positions", long_about = None)]
struct Args {
    /// Scratch directory holding <game>/ and dfas_by_hash/
    #[arg(long, default_value = "scratch")]
    scratch: PathBuf,

    /// Game the DFA belongs to, e.g. breakthrough_4x4
    game: String,

    /// DFA name under <scratch>/<game>/, or a 64 character digest
    dfa_name: String,
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
    let side_to_move = parse_side_to_move(&args.dfa_name)?;
    let dfa = load::load(&args.scratch, game.as_ref(), &args.dfa_name)?;

    verify::verify_won_sound(game.as_ref(), side_to_move, &dfa, &args.dfa_name)?;
    Ok(())
}
