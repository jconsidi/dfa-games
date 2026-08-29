//! Hand-written positions checked against the rules, config driven.
//!
//! Modelled on `test_perft_u` and the `tests.json` files it reads through
//! `run_test_cases` (`src/test_utils.cpp`): `config/<game>/positions-manual.json`
//! holds a `"game"` key naming the game and a `"tests"` array of cases, each
//! with `"type"`, `"position"` and `"side_to_move"` as the perft cases have,
//! plus `"expected_moves"` and `"expected_result"`.
//!
//! `"expected_moves"` is the full list of resulting positions, compared as a
//! set — the file does not pin down the order moves come out in, but a
//! duplicate on either side is a failure. `"expected_result"` is what
//! `validate_result` must return: `-1`, `0`, `1`, or `null` for a position
//! that is not terminal.
//!
//! A file naming a game these rules cannot build fails the run rather than
//! being skipped. Test data that silently never executes is worse than none.

use std::collections::BTreeSet;
use std::path::{Path, PathBuf};

use dfa_games::game::Game;
use dfa_games::get_game;
use serde_json::Value;

/// The test cases this file knows how to run. Other types in the same file,
/// once the configs are merged, are left to whoever handles them.
const CASE_TYPE: &str = "validate";

fn config_dir() -> PathBuf {
    // CARGO_MANIFEST_DIR is rust/dfa-games, and config/ is at the repo root.
    Path::new(env!("CARGO_MANIFEST_DIR")).join("../../config")
}

/// Every game directory carrying a `positions-manual.json`, as
/// `get_test_game_names` scans for `tests.json`.
fn games_with_positions() -> Vec<(String, PathBuf)> {
    let dir = config_dir();
    let entries = std::fs::read_dir(&dir)
        .unwrap_or_else(|e| panic!("could not scan {}: {e}", dir.display()));

    let mut output = Vec::new();
    for entry in entries {
        let entry = entry.expect("could not read a config directory entry");
        if !entry.path().is_dir() {
            continue;
        }

        let path = entry.path().join("positions-manual.json");
        if !path.exists() {
            // games are not required to have manual positions
            continue;
        }

        let game_name = entry
            .file_name()
            .into_string()
            .expect("config directory name is not valid UTF-8");
        output.push((game_name, path));
    }

    output.sort();
    output
}

fn characters(value: &Value, what: &str) -> Vec<u32> {
    let array = value
        .as_array()
        .unwrap_or_else(|| panic!("{what} is not an array: {value}"));

    array
        .iter()
        .map(|c| {
            let n = c
                .as_u64()
                .unwrap_or_else(|| panic!("{what} holds a non-character {c}"));
            u32::try_from(n).unwrap_or_else(|_| panic!("{what} holds an out of range character {n}"))
        })
        .collect()
}

/// A position must fit the game before it can say anything about the rules.
fn check_shape(game: &dyn Game, position: &[u32], what: &str) {
    let shape = game.shape();
    assert_eq!(
        position.len(),
        shape.len(),
        "{what} has {} characters, but {} positions have {}",
        position.len(),
        game.name(),
        shape.len()
    );

    for (i, (&character, &size)) in position.iter().zip(shape).enumerate() {
        assert!(
            character < size,
            "{what} has character {character} at index {i}, outside the alphabet of size {size}"
        );
    }
}

fn as_set(moves: &[Vec<u32>], what: &str) -> BTreeSet<Vec<u32>> {
    let set: BTreeSet<Vec<u32>> = moves.iter().cloned().collect();
    assert_eq!(
        set.len(),
        moves.len(),
        "{what} lists the same position more than once"
    );
    set
}

fn run_case(game: &dyn Game, index: usize, case: &Value) {
    let label = format!("{} case {index}", game.name());

    let position = characters(
        case.get("position")
            .unwrap_or_else(|| panic!("{label} has no \"position\"")),
        &format!("{label} position"),
    );
    check_shape(game, &position, &format!("{label} position"));

    let side_to_move = case
        .get("side_to_move")
        .and_then(|s| s.as_u64())
        .unwrap_or_else(|| panic!("{label} has no \"side_to_move\""));
    assert!(
        side_to_move < 2,
        "{label} has side_to_move {side_to_move}, which is neither 0 nor 1"
    );
    let side_to_move = side_to_move as u32;

    let expected_moves: Vec<Vec<u32>> = case
        .get("expected_moves")
        .unwrap_or_else(|| panic!("{label} has no \"expected_moves\""))
        .as_array()
        .unwrap_or_else(|| panic!("{label} expected_moves is not an array"))
        .iter()
        .enumerate()
        .map(|(i, m)| {
            let move_out = characters(m, &format!("{label} expected move {i}"));
            check_shape(game, &move_out, &format!("{label} expected move {i}"));
            move_out
        })
        .collect();

    let expected_result = match case
        .get("expected_result")
        .unwrap_or_else(|| panic!("{label} has no \"expected_result\""))
    {
        Value::Null => None,
        value => {
            let result = value
                .as_i64()
                .unwrap_or_else(|| panic!("{label} expected_result is not a number or null"));
            assert!(
                (-1..=1).contains(&result),
                "{label} expected_result is {result}, not -1, 0, 1 or null"
            );
            Some(result as i32)
        }
    };

    // moves

    let moves = game.validate_moves(side_to_move, &position);
    let found = as_set(&moves, &format!("{label} generated moves"));
    let wanted = as_set(&expected_moves, &format!("{label} expected_moves"));

    if found != wanted {
        let mut report = format!(
            "{label} move mismatch, side to move {side_to_move}:\n{}",
            game.position_to_string(&position)
        );
        for missing in wanted.difference(&found) {
            report += &format!("MISSING:\n{}", game.position_to_string(missing));
        }
        for extra in found.difference(&wanted) {
            report += &format!("UNEXPECTED:\n{}", game.position_to_string(extra));
        }
        panic!("{report}");
    }

    // result

    let result = game.validate_result(side_to_move, &position, &moves);
    assert_eq!(
        result,
        expected_result,
        "{label} result mismatch, side to move {side_to_move}: got {result:?}, expected \
         {expected_result:?}\n{}",
        game.position_to_string(&position)
    );
}

#[test]
fn manual_positions_match_the_rules() {
    let games = games_with_positions();
    assert!(
        !games.is_empty(),
        "no positions-manual.json found under {}",
        config_dir().display()
    );

    let mut cases_run = 0usize;
    for (game_name, path) in &games {
        let text = std::fs::read_to_string(path)
            .unwrap_or_else(|e| panic!("could not read {}: {e}", path.display()));
        let config: Value = serde_json::from_str(&text)
            .unwrap_or_else(|e| panic!("could not parse {}: {e}", path.display()));

        let configured = config
            .get("game")
            .and_then(|g| g.as_str())
            .unwrap_or_else(|| panic!("{} has no \"game\"", path.display()));
        assert_eq!(
            configured,
            game_name,
            "{} is for {configured} instead of {game_name}",
            path.display()
        );

        // Not a skip: a file for a game with no rules here is a check that
        // would never run, which is what this is meant to prevent.
        let game = get_game(game_name)
            .unwrap_or_else(|e| panic!("{} names a game these rules cannot build: {e:#}", path.display()));

        let cases = config
            .get("tests")
            .and_then(|t| t.as_array())
            .unwrap_or_else(|| panic!("{} has no \"tests\" array", path.display()));

        let mut ran_here = 0usize;
        for (index, case) in cases.iter().enumerate() {
            let case_type = case
                .get("type")
                .and_then(|t| t.as_str())
                .unwrap_or_else(|| panic!("{} case {index} has no \"type\"", path.display()));
            if case_type != CASE_TYPE {
                continue;
            }

            run_case(game.as_ref(), index, case);
            ran_here += 1;
        }

        assert!(
            ran_here > 0,
            "{} holds no \"{CASE_TYPE}\" cases",
            path.display()
        );
        cases_run += ran_here;
    }

    println!("checked {cases_run} positions across {} games", games.len());
}
