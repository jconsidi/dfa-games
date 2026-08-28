//! Resolving a game and a DFA name to an open file.
//!
//! Mirrors `get_file_name` in `src/DFA.cpp`: a 64 character digest addresses
//! `dfas_by_hash/<digest>.dfa` directly, and any other name is
//! `<scratch>/<game>/<name>`, which on disk is a symbolic link to one of those.

use std::path::{Path, PathBuf};

use anyhow::Result;
use dfa_format::{is_hash, Dfa};

use crate::game::Game;

pub fn dfa_path(scratch: &Path, game_name: &str, dfa_name: &str) -> PathBuf {
    if is_hash(dfa_name) {
        return scratch.join("dfas_by_hash").join(format!("{dfa_name}.dfa"));
    }

    scratch.join(game_name).join(dfa_name)
}

/// Open a DFA and check it is shaped like a position of this game.
///
/// The C++ `DFA(shape, name)` constructor makes the same cross check: the file
/// carries its own shape, so a name resolving to a DFA of the wrong shape is a
/// mistake worth catching here rather than as nonsense output much later.
pub fn load(scratch: &Path, game: &dyn Game, dfa_name: &str) -> Result<Dfa> {
    let path = dfa_path(scratch, game.name(), dfa_name);

    // Not `with_context`: FormatError::Io already names the path and its cause,
    // and anyhow's chain would then print the cause a second time.
    let dfa = Dfa::open(&path).map_err(|e| {
        anyhow::anyhow!(
            "could not load DFA \"{dfa_name}\" for game \"{}\": {e}",
            game.name()
        )
    })?;

    let want = game.shape();
    let got = dfa.layout().shape();
    if got != want {
        anyhow::bail!(
            "DFA \"{dfa_name}\" is shaped {} but game \"{}\" positions are shaped {} ({})",
            describe_shape(got),
            game.name(),
            describe_shape(want),
            path.display()
        );
    }

    Ok(dfa)
}

/// A shape as `LENGTHxCHARACTERS` when it is uniform, which every ported
/// game's is, and as a list when it is not.
fn describe_shape(shape: &[u32]) -> String {
    match shape.first() {
        None => "empty".to_string(),
        Some(first) if shape.iter().all(|c| c == first) => format!("{}x{first}", shape.len()),
        _ => format!("{shape:?}"),
    }
}
