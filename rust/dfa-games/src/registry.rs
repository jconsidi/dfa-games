//! Game name to game, the part of `get_game` in `src/test_utils.cpp` that has
//! a Rust implementation of the rules behind it.

use anyhow::{bail, Result};

use crate::amazons::AmazonsGame;
use crate::breakthrough::BreakthroughGame;
use crate::clobber::ClobberGame;
use crate::game::Game;
use crate::normalnim::NormalNimGame;

/// Games the C++ knows about that have no Rust rules yet.  Named separately so
/// the error can say "not ported" rather than "unrecognized", which are very
/// different things to read when a command fails.
const NOT_PORTED: &[&str] = &[
    "breakthroughcw_",
    "chess+",
    "othello_",
    "tictactoe_",
];

/// Parse `<prefix>WxH` into its two dimensions.
fn parse_dimensions(rest: &str) -> Option<(usize, usize)> {
    let (w, h) = rest.split_once('x')?;
    Some((w.parse().ok()?, h.parse().ok()?))
}

pub fn get_game(game_name: &str) -> Result<Box<dyn Game>> {
    let game: Box<dyn Game> = if let Some(rest) = game_name.strip_prefix("breakthrough_") {
        let (width, height) = parse_dimensions(rest)
            .ok_or_else(|| anyhow::anyhow!("could not parse breakthrough game name \"{game_name}\", expected breakthrough_WIDTHxHEIGHT"))?;
        if width < 1 || height < 4 {
            bail!("breakthrough needs width >= 1 and height >= 4, got {width}x{height}");
        }
        Box::new(BreakthroughGame::new(width, height))
    } else if let Some(rest) = game_name.strip_prefix("amazons_") {
        let (width, height) = parse_dimensions(rest)
            .ok_or_else(|| anyhow::anyhow!("could not parse amazons game name \"{game_name}\", expected amazons_WIDTHxHEIGHT"))?;
        if width < 1 || height < 1 {
            bail!("amazons needs a board at least 1x1, got {width}x{height}");
        }
        Box::new(AmazonsGame::new(width, height))
    } else if let Some(rest) = game_name.strip_prefix("clobber_") {
        let (width, height) = parse_dimensions(rest)
            .ok_or_else(|| anyhow::anyhow!("could not parse clobber game name \"{game_name}\", expected clobber_WIDTHxHEIGHT"))?;
        if width < 1 || height < 1 {
            bail!("clobber needs a board at least 1x1, got {width}x{height}");
        }
        Box::new(ClobberGame::new(width, height))
    } else if let Some(rest) = game_name.strip_prefix("normalnim_") {
        let (num_heaps, heap_max) = parse_dimensions(rest)
            .ok_or_else(|| anyhow::anyhow!("could not parse normalnim game name \"{game_name}\", expected normalnim_HEAPSxHEAPMAX"))?;
        if num_heaps < 1 {
            bail!("normalnim needs at least one heap, got {num_heaps}");
        }
        let heap_max = u32::try_from(heap_max)
            .map_err(|_| anyhow::anyhow!("normalnim heap maximum {heap_max} is too large"))?;
        Box::new(NormalNimGame::new(num_heaps, heap_max))
    } else if let Some(prefix) = NOT_PORTED.iter().find(|p| game_name.starts_with(**p)) {
        bail!(
            "game \"{game_name}\" ({prefix}...) exists in the C++ but its rules are not ported to Rust yet; \
             use the C++ verify_* binaries for it"
        );
    } else {
        bail!(
            "unrecognized game name \"{game_name}\"; ported games are amazons_WxH, \
             breakthrough_WxH, clobber_WxH and normalnim_HEAPSxHEAPMAX"
        );
    };

    // The C++ asserts this. A name that parses but does not round trip means
    // the parse and the constructor disagree, which would silently verify the
    // wrong game.
    if game.name() != game_name {
        bail!(
            "game name \"{game_name}\" did not round trip, got \"{}\"",
            game.name()
        );
    }

    Ok(game)
}

/// `verify_parse_side_to_move` from `src/verify_utils.cpp`: which side a DFA is
/// about, read off its name.
pub fn parse_side_to_move(dfa_name: &str) -> Result<u32> {
    for template in [",side_to_move=", ",side="] {
        for side in 0..2u32 {
            if dfa_name.contains(&format!("{template}{side}")) {
                return Ok(side);
            }
        }
    }

    bail!(
        "could not find the side to move in DFA name \"{dfa_name}\"; \
         expected it to contain \",side_to_move=N\" or \",side=N\""
    )
}
