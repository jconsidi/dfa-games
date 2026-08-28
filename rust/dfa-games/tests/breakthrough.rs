//! Breakthrough move generation, against hand-worked positions.

use dfa_games::breakthrough::BreakthroughGame;
use dfa_games::game::Game;

/// Build a 4x4 position from a picture, row 0 first, as
/// `position_to_string` prints it.
fn board(rows: [&str; 4]) -> Vec<u32> {
    let mut out = Vec::new();
    for row in rows {
        assert_eq!(row.len(), 4);
        for c in row.chars() {
            out.push(match c {
                '.' => 0,
                'w' => 1,
                'b' => 2,
                _ => panic!("bad square {c}"),
            });
        }
    }
    out
}

fn moves_as_boards(game: &BreakthroughGame, side: u32, position: &[u32]) -> Vec<String> {
    game.validate_moves(side, position)
        .iter()
        .map(|m| game.position_to_string(m))
        .collect()
}

#[test]
fn round_trips_through_position_to_string() {
    let game = BreakthroughGame::new(4, 4);
    let position = board(["wwww", "....", "....", "bbbb"]);
    assert_eq!(
        game.position_to_string(&position),
        "wwww\n....\n....\nbbbb\n"
    );
}

#[test]
fn shape_is_one_character_per_square() {
    let game = BreakthroughGame::new(4, 5);
    assert_eq!(game.shape(), vec![3u32; 20]);
    assert_eq!(game.name(), "breakthrough_4x5");
}

#[test]
fn push_capture_and_blocks() {
    let game = BreakthroughGame::new(4, 4);

    // w at 1,1 has: a forward push to 2,1 blocked by nothing? No: 2,1 is b,
    // so the forward capture is illegal. The two diagonals are a capture at
    // 2,0 and a push to 2,2. w at 1,2 is a friendly piece and does not block
    // anything here; the w at 2,3 blocks nothing either.
    let position = board([
        "....", //
        ".w..", //
        "bb..", //
        "....",
    ]);

    let got = moves_as_boards(&game, 0, &position);
    assert_eq!(
        got,
        vec![
            // diagonal capture left, onto the b at 2,0
            "....\n....\nwb..\n....\n",
            // diagonal push right, to the empty 2,2
            "....\n....\nbbw.\n....\n",
        ],
        "forward capture onto 2,1 must be excluded"
    );
}

#[test]
fn cannot_capture_own_piece() {
    let game = BreakthroughGame::new(4, 4);
    // w at 1,1 is boxed in by friendly pieces on all three forward squares.
    let position = board([
        "....", //
        ".w..", //
        "www.", //
        "....",
    ]);

    let from_boxed_in: Vec<String> = game
        .validate_moves(0, &position)
        .iter()
        .filter(|m| m[4 + 1] == 0)
        .map(|m| game.position_to_string(m))
        .collect();
    assert!(
        from_boxed_in.is_empty(),
        "the piece at 1,1 should have no moves: {from_boxed_in:?}"
    );
}

#[test]
fn edge_columns_do_not_wrap() {
    let game = BreakthroughGame::new(4, 4);

    // Column 0 has no left diagonal, column 3 has no right diagonal.
    let position = board([
        "w..w", //
        "....", //
        "....", //
        "....",
    ]);

    let got = moves_as_boards(&game, 0, &position);
    assert_eq!(
        got,
        vec![
            "...w\nw...\n....\n....\n",
            "...w\n.w..\n....\n....\n",
            "w...\n..w.\n....\n....\n",
            "w...\n...w\n....\n....\n",
        ]
    );
}

#[test]
fn a_piece_on_the_last_row_has_no_moves() {
    let game = BreakthroughGame::new(4, 4);
    // Side 1 moves toward row 0, so its piece on row 0 cannot move. Side 1
    // has also already won, but check the geometry from side 1's own view by
    // putting the piece somewhere it has nowhere to go.
    let position = board([
        "....", //
        "....", //
        "....", //
        "..b.",
    ]);
    // b at 3,2 moving toward row 0 has three squares, all empty.
    assert_eq!(game.validate_moves(1, &position).len(), 3);

    let at_the_end = board([
        "..b.", //
        "....", //
        "....", //
        "....",
    ]);
    // b has reached row 0 and won, so side 1 has no moves either way.
    assert!(game.validate_moves(1, &at_the_end).is_empty());
}

#[test]
fn reaching_the_far_row_ends_the_game() {
    let game = BreakthroughGame::new(4, 4);

    // b has reached row 0, so side 0 has lost and has no moves even though
    // its own pieces could otherwise move.
    let position = board([
        "b...", //
        ".w..", //
        "....", //
        "....",
    ]);
    assert!(game.validate_moves(0, &position).is_empty());
    assert_eq!(game.validate_result(0, &position, &[]), Some(-1));

    // The mirror: w on row 3 ends it for side 1.
    let mirrored = board([
        "....", //
        "....", //
        "..b.", //
        "w...",
    ]);
    assert!(game.validate_moves(1, &mirrored).is_empty());
    assert_eq!(game.validate_result(1, &mirrored, &[]), Some(-1));
}

#[test]
fn side_one_moves_the_other_way() {
    let game = BreakthroughGame::new(4, 4);
    let position = board([
        "....", //
        "w...", //
        ".b..", //
        "....",
    ]);

    // b at 2,1 moves toward row 1: a diagonal capture of the w at 1,0, and
    // pushes to the two empty squares.
    let got = moves_as_boards(&game, 1, &position);
    assert_eq!(
        got,
        vec![
            "....\nb...\n....\n....\n",
            "....\nwb..\n....\n....\n",
            "....\nw.b.\n....\n....\n",
        ]
    );
}

#[test]
fn a_position_with_no_pieces_is_lost() {
    let game = BreakthroughGame::new(4, 4);
    let position = board(["....", "....", "....", "...."]);
    let moves = game.validate_moves(0, &position);
    assert!(moves.is_empty());
    assert_eq!(game.validate_result(0, &position, &moves), Some(-1));
}
