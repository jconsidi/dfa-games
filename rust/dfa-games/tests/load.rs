//! Resolving a game and a DFA name to a file, and the shape cross check.

use std::path::{Path, PathBuf};

use dfa_format::{convert, Automaton, LegacyDfa};
use dfa_games::load::{dfa_path, load};
use dfa_games::registry::get_game;
use tempfile::TempDir;

/// A scratch directory holding one DFA of `shape`, published under `dfa_name`
/// in `game_name`'s directory the way the C++ solver leaves it.
fn scratch_with(tmp: &TempDir, game_name: &str, dfa_name: &str, shape: Vec<u32>) -> PathBuf {
    let scratch = tmp.path().join("scratch");

    // Something non-trivial, so the file is not the reject DFA.
    let ndim = shape.len();
    let mut a = Automaton::new(shape);
    let mut state = 1u32;
    for layer in (0..ndim).rev() {
        let transitions = std::iter::once(state)
            .chain(std::iter::repeat_n(0, a.shape()[layer] as usize - 1))
            .collect();
        state = a.add_state(layer, transitions);
    }
    a.set_initial_state(state);

    let legacy_dir = tmp.path().join(format!("legacy-{dfa_name}"));
    a.write_legacy_dir(&legacy_dir).unwrap();
    let legacy = LegacyDfa::open(&legacy_dir).unwrap();

    let by_hash = scratch.join("dfas_by_hash");
    let converted = convert(&legacy, &by_hash, true).unwrap();

    let game_dir = scratch.join(game_name);
    std::fs::create_dir_all(&game_dir).unwrap();
    std::fs::copy(&converted.path, game_dir.join(dfa_name)).unwrap();

    scratch
}

#[test]
fn a_name_resolves_under_the_game_directory() {
    assert_eq!(
        dfa_path(Path::new("scratch"), "breakthrough_4x4", "lost,side_to_move=0"),
        PathBuf::from("scratch/breakthrough_4x4/lost,side_to_move=0")
    );
}

#[test]
fn a_digest_resolves_under_dfas_by_hash() {
    let digest = "f03c9b5de59eae9308276200781cf301dae053128f9520f31b4942d4867d1654";
    assert_eq!(
        dfa_path(Path::new("scratch"), "breakthrough_4x4", digest),
        PathBuf::from(format!("scratch/dfas_by_hash/{digest}.dfa"))
    );
}

#[test]
fn a_dfa_of_the_right_shape_loads() {
    let tmp = TempDir::new().unwrap();
    let game = get_game("breakthrough_4x4").unwrap();
    let scratch = scratch_with(&tmp, game.name(), "lost,side_to_move=0", vec![3u32; 16]);

    let dfa = load(&scratch, game.as_ref(), "lost,side_to_move=0").unwrap();
    assert_eq!(dfa.layout().shape(), game.shape());
}

#[test]
fn a_dfa_of_the_wrong_shape_is_rejected() {
    // An amazons DFA (16 layers of 4 characters) sitting where a breakthrough
    // one (16 layers of 3) was wanted. Both are 16 layers, so nothing but the
    // alphabet size catches it, and reading it as breakthrough would produce
    // nonsense rather than an error.
    let tmp = TempDir::new().unwrap();
    let game = get_game("breakthrough_4x4").unwrap();
    let scratch = scratch_with(&tmp, game.name(), "lost,side_to_move=0", vec![4u32; 16]);

    let err = load(&scratch, game.as_ref(), "lost,side_to_move=0")
        .err()
        .unwrap()
        .to_string();
    assert!(err.contains("is shaped 16x4"), "{err}");
    assert!(err.contains("shaped 16x3"), "{err}");
}

#[test]
fn a_missing_dfa_names_the_game_and_the_dfa() {
    // The C++ load path reports only "open() failed" with no indication of
    // what was wanted, which is what verify_load exists to fix.
    let tmp = TempDir::new().unwrap();
    let game = get_game("breakthrough_4x4").unwrap();
    let scratch = tmp.path().join("scratch");

    let err = load(&scratch, game.as_ref(), "backward,ply_max=099,side=0,losing")
        .err()
        .unwrap()
        .to_string();
    assert!(err.contains("backward,ply_max=099,side=0,losing"), "{err}");
    assert!(err.contains("breakthrough_4x4"), "{err}");
    assert!(err.contains("No such file"), "{err}");
    // The cause must appear once, not twice.
    assert_eq!(err.matches("No such file").count(), 1, "{err}");
}
