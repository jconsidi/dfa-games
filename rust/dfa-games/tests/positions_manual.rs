//! Configured positions checked against the rules.
//!
//! read `/config/<game>/positions-manual.json` and
//! `/config/<game>/positions-generated.json` for positions to test.
//! `"position"` gives the vector representation.
//! `"expected_moves"` gives the vectors of the moves from that position to be compared as a set.
//! `"expected_result"` gives the result (-1, 0, or 1) for terminal positions or null for non-terminal positions.
//! `"expected_perft_u"` gives the number of distinct positions at each depth, sides alternating, as `test_perft_u` counts them.
//!
//! Manual positions require `"expected_moves"` and `"expected_result"`.
//! Generated positions require `"expected_perft_u"`, which the generators
//! can afford for every size where a full move list would not be.
//! Any of the three that is present is checked.
//!
//! A file naming a game these rules cannot build fails the run rather than
//! being skipped.

use std::collections::BTreeSet;
use std::path::{Path, PathBuf};

use dfa_games::game::Game;
use dfa_games::get_game;
use serde_json::Value;

fn config_dir() -> PathBuf {
    // CARGO_MANIFEST_DIR is rust/dfa-games, and config/ is at the repo root.
    Path::new(env!("CARGO_MANIFEST_DIR")).join("../../config")
}

/// Every game directory carrying `filename`, as `get_test_game_names` scans
/// for `tests.json`.
fn games_with_positions(filename: &str) -> Vec<(String, PathBuf)> {
    let dir = config_dir();
    let entries =
        std::fs::read_dir(&dir).unwrap_or_else(|e| panic!("could not scan {}: {e}", dir.display()));

    let mut output = Vec::new();
    for entry in entries {
        let entry = entry.expect("could not read a config directory entry");
        if !entry.path().is_dir() {
            continue;
        }

        let path = entry.path().join(filename);
        if !path.exists() {
            // games are not required to have positions
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
            u32::try_from(n)
                .unwrap_or_else(|_| panic!("{what} holds an out of range character {n}"))
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

/// Distinct positions at each depth from `position`, as `test_perft_u`
/// counts them with `get_moves_forward`.
fn perft_u(game: &dyn Game, side_to_move: u32, position: &[u32], depth: usize) -> Vec<usize> {
    let mut output = Vec::with_capacity(depth);
    let mut frontier: BTreeSet<Vec<u32>> = BTreeSet::from([position.to_vec()]);
    for ply in 0..depth {
        let side = (side_to_move + ply as u32) % 2;
        frontier = frontier
            .iter()
            .flat_map(|p| game.validate_moves(side, p))
            .collect();
        output.push(frontier.len());
    }
    output
}

fn run_case(game: &dyn Game, filename: &str, index: usize, case: &Value) {
    let manual = filename == "positions-manual.json";
    let label = format!("{} {filename} case {index}", game.name());

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

    let expected_moves: Option<Vec<Vec<u32>>> = case.get("expected_moves").map(|m| {
        m.as_array()
            .unwrap_or_else(|| panic!("{label} expected_moves is not an array"))
            .iter()
            .enumerate()
            .map(|(i, m)| {
                let move_out = characters(m, &format!("{label} expected move {i}"));
                check_shape(game, &move_out, &format!("{label} expected move {i}"));
                move_out
            })
            .collect()
    });

    let expected_result = case.get("expected_result").map(|value| match value {
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
    });

    let expected_perft_u: Option<Vec<usize>> = case.get("expected_perft_u").map(|p| {
        p.as_array()
            .unwrap_or_else(|| panic!("{label} expected_perft_u is not an array"))
            .iter()
            .map(|n| {
                let n = n
                    .as_u64()
                    .unwrap_or_else(|| panic!("{label} expected_perft_u holds a non-count {n}"));
                usize::try_from(n).unwrap_or_else(|_| {
                    panic!("{label} expected_perft_u holds an out of range {n}")
                })
            })
            .collect()
    });

    if manual {
        assert!(
            expected_moves.is_some(),
            "{label} has no \"expected_moves\""
        );
        assert!(
            expected_result.is_some(),
            "{label} has no \"expected_result\""
        );
    } else {
        assert!(
            expected_perft_u.is_some(),
            "{label} has no \"expected_perft_u\""
        );
    }

    // moves

    let moves = game.validate_moves(side_to_move, &position);
    let found = as_set(&moves, &format!("{label} generated moves"));

    if let Some(expected_moves) = expected_moves {
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
    }

    // result

    if let Some(expected_result) = expected_result {
        let result = game.validate_result(side_to_move, &position, &moves);
        assert_eq!(
            result,
            expected_result,
            "{label} result mismatch, side to move {side_to_move}: got {result:?}, expected \
             {expected_result:?}\n{}",
            game.position_to_string(&position)
        );
    }

    // perft

    if let Some(expected_perft_u) = expected_perft_u {
        let counts = perft_u(game, side_to_move, &position, expected_perft_u.len());
        assert_eq!(
            counts,
            expected_perft_u,
            "{label} perft_u mismatch, side to move {side_to_move}\n{}",
            game.position_to_string(&position)
        );
    }
}

fn check_positions(filename: &str) {
    let games = games_with_positions(filename);
    assert!(
        !games.is_empty(),
        "no {filename} found under {}",
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
        let game = get_game(game_name).unwrap_or_else(|e| {
            panic!(
                "{} names a game these rules cannot build: {e:#}",
                path.display()
            )
        });

        let cases = config
            .get("positions")
            .and_then(|t| t.as_array())
            .unwrap_or_else(|| panic!("{} has no \"tests\" array", path.display()));

        let mut ran_here = 0usize;
        for (index, case) in cases.iter().enumerate() {
            run_case(game.as_ref(), filename, index, case);
            ran_here += 1;
        }

        cases_run += ran_here;
    }

    println!(
        "checked {cases_run} positions from {filename} across {} games",
        games.len()
    );
}

#[test]
fn manual_positions_match_the_rules() {
    check_positions("positions-manual.json");
}

#[test]
fn generated_positions_match_the_rules() {
    check_positions("positions-generated.json");
}
