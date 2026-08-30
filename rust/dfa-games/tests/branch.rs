//! The terminal / non-terminal branch in `check_position`.
//!
//! This is the part with no counterpart in the C++, which instead consults the
//! `lost` and `won` DFAs, so nothing else covers it. A stub game makes the two
//! branches observable: it counts calls to `validate_result` and returns
//! whatever the test wants.

use std::path::Path;
use std::sync::atomic::{AtomicUsize, Ordering};

use dfa_format::{write_automaton, Automaton, Dfa};
use dfa_games::game::{Game, Position, Side};
use dfa_games::verify::{check_position, Claim, Continuation};
use tempfile::TempDir;

struct Stub {
    /// Returned by `validate_moves` when the position starts with a 1.
    moves: Vec<Vec<u32>>,
    /// Returned by `validate_result`.
    result: Option<i32>,
    result_calls: AtomicUsize,
}

impl Stub {
    fn new(moves: Vec<Vec<u32>>, result: Option<i32>) -> Stub {
        Stub {
            moves,
            result,
            result_calls: AtomicUsize::new(0),
        }
    }

    fn result_calls(&self) -> usize {
        self.result_calls.load(Ordering::Relaxed)
    }
}

impl Game for Stub {
    fn name(&self) -> &str {
        "stub"
    }

    fn shape(&self) -> &[u32] {
        &[2, 2]
    }

    fn validate_moves(&self, _side: Side, position: &Position) -> Vec<Vec<u32>> {
        if position[0] == 1 {
            self.moves.clone()
        } else {
            Vec::new()
        }
    }

    fn validate_result(
        &self,
        _side: Side,
        _position: &Position,
        _moves: &[Vec<u32>],
    ) -> Option<i32> {
        self.result_calls.fetch_add(1, Ordering::Relaxed);
        self.result
    }

    fn position_to_string(&self, position: &Position) -> String {
        format!("{position:?}\n")
    }
}

const HAS_MOVES: [u32; 2] = [1, 0];
const TERMINAL: [u32; 2] = [0, 0];

fn lost_claim() -> Claim<'static> {
    Claim {
        terminal_result: -1,
        continuation: Continuation::Terminal,
    }
}

/// A DFA over shape [2, 2] accepting exactly the string [1, 1].
fn only_eleven(tmp: &TempDir) -> Dfa {
    let mut a = Automaton::new(vec![2, 2]);
    let l1 = a.add_state(1, vec![0, 1]);
    let l0 = a.add_state(0, vec![0, l1]);
    a.set_initial_state(l0);

    let converted = write_automaton(&a, Path::new(tmp.path()), true).unwrap();
    Dfa::open(&converted.path).unwrap()
}

#[test]
fn a_position_with_moves_never_asks_for_a_result() {
    let stub = Stub::new(vec![vec![1, 1]], Some(-1));
    let err = check_position(&stub, 0, &HAS_MOVES, &lost_claim()).unwrap_err();
    assert!(err.to_string().contains("should be terminal"), "{err}");
    assert_eq!(stub.result_calls(), 0);
}

#[test]
fn a_terminal_position_must_report_the_claimed_base_case() {
    let stub = Stub::new(Vec::new(), Some(-1));
    check_position(&stub, 0, &TERMINAL, &lost_claim()).unwrap();
    assert_eq!(stub.result_calls(), 1);
}

#[test]
fn a_terminal_position_reporting_the_wrong_result_fails() {
    // A won position sitting in a losing DFA. The C++ move loop passed this
    // vacuously; here it is a failure.
    let stub = Stub::new(Vec::new(), Some(1));
    let err = check_position(&stub, 0, &TERMINAL, &lost_claim()).unwrap_err();
    let message = err.to_string();
    assert!(message.contains("result is 1 (won)"), "{message}");
    assert!(message.contains("claims -1 (lost)"), "{message}");
}

#[test]
fn no_moves_but_not_terminal_fails() {
    let stub = Stub::new(Vec::new(), None);
    let err = check_position(&stub, 0, &TERMINAL, &lost_claim()).unwrap_err();
    assert!(err.to_string().contains("not terminal"), "{err}");
}

#[test]
fn every_move_must_be_in_the_other_dfa() {
    let tmp = TempDir::new().unwrap();
    let dfa = only_eleven(&tmp);
    let claim = Claim {
        terminal_result: -1,
        continuation: Continuation::Every {
            dfa: &dfa,
            label: "winning_prev",
        },
    };

    let all_in = Stub::new(vec![vec![1, 1]], Some(-1));
    check_position(&all_in, 0, &HAS_MOVES, &claim).unwrap();
    assert_eq!(all_in.result_calls(), 0);

    let one_out = Stub::new(vec![vec![1, 1], vec![1, 0]], Some(-1));
    let err = check_position(&one_out, 0, &HAS_MOVES, &claim).unwrap_err();
    assert!(err.to_string().contains("not in winning_prev"), "{err}");
}

#[test]
fn some_move_must_be_in_the_other_dfa() {
    let tmp = TempDir::new().unwrap();
    let dfa = only_eleven(&tmp);
    let claim = Claim {
        terminal_result: 1,
        continuation: Continuation::Any {
            dfa: &dfa,
            label: "losing_prev",
        },
    };

    let one_in = Stub::new(vec![vec![1, 0], vec![1, 1]], Some(1));
    check_position(&one_in, 0, &HAS_MOVES, &claim).unwrap();
    assert_eq!(one_in.result_calls(), 0);

    let none_in = Stub::new(vec![vec![1, 0], vec![0, 1]], Some(1));
    let err = check_position(&none_in, 0, &HAS_MOVES, &claim).unwrap_err();
    assert!(
        err.to_string().contains("none of them is in losing_prev"),
        "{err}"
    );
}

#[test]
fn a_terminal_position_satisfies_a_losing_claim_without_touching_the_other_dfa() {
    // The substitution for the C++ lost-DFA shortcut: a position with no moves
    // is accepted on the strength of validate_result alone.
    let tmp = TempDir::new().unwrap();
    let dfa = only_eleven(&tmp);
    let claim = Claim {
        terminal_result: -1,
        continuation: Continuation::Every {
            dfa: &dfa,
            label: "winning_prev",
        },
    };

    let stub = Stub::new(Vec::new(), Some(-1));
    check_position(&stub, 0, &TERMINAL, &claim).unwrap();
    assert_eq!(stub.result_calls(), 1);
}
