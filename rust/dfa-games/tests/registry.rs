//! Game name parsing and the side-to-move a DFA name carries.

use dfa_games::{get_game, parse_side_to_move};

/// `Box<dyn Game>` is not `Debug`, so `unwrap_err` is not available.
fn game_error(name: &str) -> String {
    match get_game(name) {
        Ok(game) => panic!("expected \"{name}\" to be rejected, got {}", game.name()),
        Err(e) => e.to_string(),
    }
}

#[test]
fn ported_games_round_trip() {
    for name in ["breakthrough_4x4", "breakthrough_5x5", "amazons_4x6"] {
        assert_eq!(get_game(name).unwrap().name(), name);
    }
}

#[test]
fn a_game_that_exists_in_cpp_says_so() {
    // "not ported" and "unrecognized" are very different things to read when
    // a command fails, so they are different messages.
    for name in ["breakthroughcw_4x4", "chess+1", "othello_6x6", "tictactoe_3"] {
        let err = game_error(name);
        assert!(err.contains("not ported"), "{name}: {err}");
    }
}

#[test]
fn an_unknown_game_is_rejected() {
    let err = game_error("hnefatafl_11x11");
    assert!(err.contains("unrecognized"), "{err}");
}

#[test]
fn a_malformed_size_is_rejected() {
    for name in ["breakthrough_4", "breakthrough_4x", "amazons_xx", "amazons_-1x4"] {
        assert!(get_game(name).is_err(), "{name} should not parse");
    }
}

#[test]
fn breakthrough_enforces_its_minimum_board() {
    // The C++ constructor asserts height >= 4.
    assert!(get_game("breakthrough_4x3").is_err());
    assert!(get_game("breakthrough_4x4").is_ok());
}

#[test]
fn side_to_move_comes_from_the_dfa_name() {
    assert_eq!(parse_side_to_move("lost,side_to_move=0").unwrap(), 0);
    assert_eq!(parse_side_to_move("won,side_to_move=1").unwrap(), 1);
    assert_eq!(
        parse_side_to_move("backward,ply_max=003,side=0,losing").unwrap(),
        0
    );
    assert_eq!(
        parse_side_to_move("backward,ply_max=003,side=1,winning").unwrap(),
        1
    );
}

#[test]
fn a_name_with_no_side_is_rejected() {
    let err = parse_side_to_move("forward,ply=001").unwrap_err().to_string();
    assert!(err.contains("side_to_move"), "{err}");
}
