//! Amazons move generation and the queen move table.

use dfa_games::amazons::AmazonsGame;
use dfa_games::game::Game;

/// Destinations of the queen moves out of `layer`, in table order.
fn destinations(game: &AmazonsGame, layer: usize) -> Vec<usize> {
    game.queen_moves(layer).iter().map(|(to, _)| *to).collect()
}

fn between(game: &AmazonsGame, from: usize, to: usize) -> Vec<usize> {
    game.queen_moves(from)
        .iter()
        .find(|(t, _)| *t == to)
        .unwrap_or_else(|| panic!("no queen move from {from} to {to}"))
        .1
        .clone()
}

#[test]
fn queen_moves_from_a_corner_of_a_4x4_board() {
    // Layer is x + 4 * y, so layer 0 is the corner x=0, y=0. From there a
    // queen reaches its own row (1, 2, 3), its own column (4, 8, 12) and the
    // main diagonal (5, 10, 15), and nothing else. Destinations ascend
    // because the C++ builds the table by scanning to-squares in order.
    let game = AmazonsGame::new(4, 4);
    assert_eq!(destinations(&game, 0), vec![1, 2, 3, 4, 5, 8, 10, 12, 15]);

    // The squares a slide passes over, which all have to be empty.
    assert_eq!(between(&game, 0, 1), Vec::<usize>::new());
    assert_eq!(between(&game, 0, 2), vec![1]);
    assert_eq!(between(&game, 0, 3), vec![1, 2]);
    assert_eq!(between(&game, 0, 4), Vec::<usize>::new());
    assert_eq!(between(&game, 0, 5), Vec::<usize>::new());
    assert_eq!(between(&game, 0, 8), vec![4]);
    assert_eq!(between(&game, 0, 10), vec![5]);
    assert_eq!(between(&game, 0, 12), vec![4, 8]);
    assert_eq!(between(&game, 0, 15), vec![5, 10]);
}

#[test]
fn queen_moves_are_symmetric() {
    let game = AmazonsGame::new(4, 4);
    for from in 0..16 {
        for (to, between_layers) in game.queen_moves(from) {
            let back = between(&game, *to, from);
            let mut forward = between_layers.clone();
            forward.sort();
            let mut backward = back;
            backward.sort();
            assert_eq!(forward, backward, "{from} -> {to}");
        }
    }
}

#[test]
fn a_blocker_stops_the_slide_past_it() {
    let game = AmazonsGame::new(4, 4);

    // w in the corner, an opposing amazon two squares along its row. The
    // blocked square itself and everything past it are unreachable.
    let mut position = vec![0u32; 16];
    position[0] = 1;
    position[2] = 2;

    let reached: Vec<usize> = game
        .validate_moves(0, &position)
        .iter()
        .map(|m| m.iter().position(|&c| c == 1).unwrap())
        .collect();

    assert!(reached.contains(&1), "the square before the blocker");
    assert!(!reached.contains(&2), "the blocker's own square");
    assert!(!reached.contains(&3), "past the blocker");
}

#[test]
fn shape_and_name() {
    let game = AmazonsGame::new(4, 6);
    assert_eq!(game.name(), "amazons_4x6");
    assert_eq!(game.shape(), vec![4u32; 24]);
}

#[test]
fn position_to_string_prints_the_top_row_first() {
    let game = AmazonsGame::new(4, 4);
    let mut position = vec![0u32; 16];
    position[0] = 1; // x=0, y=0, which prints on the last line
    position[15] = 2; // x=3, y=3, which prints on the first
    position[5] = 3; // burned
    assert_eq!(
        game.position_to_string(&position),
        "...b\n....\n.*..\nw...\n"
    );
}

#[test]
fn every_move_and_shot_on_the_smallest_board() {
    // A 2x2 board with one amazon and nothing else: every queen move and
    // every shot is legal, so the whole move list can be written out. It
    // includes the three shots back onto the square the amazon left, which is
    // the case a naive "the shot target must be empty" test would drop.
    let game = AmazonsGame::new(2, 2);
    let position = vec![1u32, 0, 0, 0];

    let got = game.validate_moves(0, &position);
    assert_eq!(
        got,
        vec![
            // moved to layer 1
            vec![3, 1, 0, 0], // shot back at the vacated square
            vec![0, 1, 3, 0],
            vec![0, 1, 0, 3],
            // moved to layer 2
            vec![3, 0, 1, 0], // shot back
            vec![0, 3, 1, 0],
            vec![0, 0, 1, 3],
            // moved to layer 3
            vec![3, 0, 0, 1], // shot back
            vec![0, 3, 0, 1],
            vec![0, 0, 3, 1],
        ]
    );
}

#[test]
fn a_shot_cannot_pass_over_an_occupied_square() {
    // 1x4 column: the amazon at y=0 moves to y=1, and its shot along the
    // column has to stop at the piece on y=3.
    let game = AmazonsGame::new(1, 4);
    let mut position = vec![0u32; 4];
    position[0] = 1;
    position[3] = 2;

    // Only the move to layer 1 and the move to layer 2 exist; from layer 1
    // the shot can reach layer 0 (vacated) and layer 2, not layer 3.
    let shots_from_layer_1: Vec<usize> = game
        .validate_moves(0, &position)
        .iter()
        .filter(|m| m[1] == 1)
        .map(|m| m.iter().position(|&c| c == 3).unwrap())
        .collect();
    assert_eq!(shots_from_layer_1, vec![0, 2]);
}

#[test]
fn an_amazon_with_nowhere_to_go_is_lost() {
    // 1x4 column, amazon boxed in against the wall by the piece next to it.
    let game = AmazonsGame::new(1, 4);
    let mut position = vec![0u32; 4];
    position[0] = 1;
    position[1] = 2;

    let moves = game.validate_moves(0, &position);
    assert!(moves.is_empty());
    assert_eq!(game.validate_result(0, &position, &moves), Some(-1));
}
