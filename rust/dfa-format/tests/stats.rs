//! Position counting, checked against brute force enumeration.

use std::path::PathBuf;

use dfa_format::stats::{count_accepted, format_positions};
use dfa_format::{write_automaton, Automaton, Dfa, Stats};
use tempfile::TempDir;

fn build(a: &Automaton) -> (TempDir, PathBuf) {
    let tmp = TempDir::new().unwrap();
    let converted = write_automaton(a, &tmp.path().join("out"), true).unwrap();
    (tmp, converted.path)
}

/// Count by enumerating every string, which is only feasible for small shapes
/// but needs no shared code with the implementation under test.
fn brute_force(a: &Automaton) -> u64 {
    a.all_strings().iter().filter(|s| a.accepts(s)).count() as u64
}

fn assert_counts_match(a: &Automaton) {
    let (_tmp, path) = build(a);
    let dfa = Dfa::open(&path).unwrap();
    let counted = count_accepted(&dfa).unwrap();
    assert_eq!(
        counted,
        brute_force(a) as f64,
        "shape {:?} initial {}",
        a.shape(),
        a.initial_state()
    );
}

#[test]
fn empty_set_accepts_nothing() {
    let mut a = Automaton::new(vec![3, 2, 4]);
    a.set_initial_state(0);
    assert_counts_match(&a);
}

#[test]
fn universal_set_accepts_the_product_of_the_shape() {
    let mut a = Automaton::new(vec![3, 2, 4]);
    a.set_initial_state(1);
    let (_tmp, path) = build(&a);
    let dfa = Dfa::open(&path).unwrap();
    // Reached by walking row 1, which is all ones, so the count comes out as
    // the product of the remaining alphabet sizes at every step.
    assert_eq!(count_accepted(&dfa).unwrap(), 24.0);
    assert_counts_match(&a);
}

#[test]
fn a_singleton_accepts_one_string() {
    let shape = vec![3u32, 2, 4];
    let mut a = Automaton::new(shape.clone());
    let word = [2u32, 0, 3];
    let mut next = 1u32;
    for layer in (0..shape.len()).rev() {
        let mut row = vec![0u32; shape[layer] as usize];
        row[word[layer] as usize] = next;
        next = a.add_state(layer, row);
    }
    a.set_initial_state(next);
    assert_counts_match(&a);

    let (_tmp, path) = build(&a);
    assert_eq!(count_accepted(&Dfa::open(&path).unwrap()).unwrap(), 1.0);
}

#[test]
fn a_tangled_automaton_matches_brute_force() {
    let shape = vec![3u32, 2, 4, 2, 3];
    let mut a = Automaton::new(shape);
    let l4a = a.add_state(4, vec![1, 0, 1]);
    let l4b = a.add_state(4, vec![0, 1, 0]);
    let l3a = a.add_state(3, vec![l4a, l4b]);
    let l3b = a.add_state(3, vec![l4b, 1]);
    let l3c = a.add_state(3, vec![0, l4a]);
    let l2a = a.add_state(2, vec![l3a, l3b, l3c, 0]);
    let l2b = a.add_state(2, vec![l3c, 1, l3a, l3b]);
    let l1a = a.add_state(1, vec![l2a, l2b]);
    let l1b = a.add_state(1, vec![l2b, 0]);
    let start = a.add_state(0, vec![l1a, l1b, 1]);
    a.set_initial_state(start);

    // Guard against a vacuous comparison.
    let accepted = brute_force(&a);
    assert!(accepted > 0 && accepted < 144, "accepted {accepted}");
    assert_counts_match(&a);
}

#[test]
fn states_and_layout_totals_are_consistent() {
    let mut a = Automaton::new(vec![3, 2, 4]);
    a.add_state(2, vec![0, 1, 1, 0]);
    a.add_state(1, vec![2, 0]);
    let start = a.add_state(0, vec![2, 0, 1]);
    a.set_initial_state(start);

    let (_tmp, path) = build(&a);
    let dfa = Dfa::open(&path).unwrap();
    let stats = Stats::collect(&dfa, true).unwrap();

    // states is the sum of the layer sizes, as DFA::states() reports it.
    assert_eq!(stats.states, 3 + 3 + 3);
    assert_eq!(stats.transitions, 3 * 3 + 3 * 2 + 3 * 4);
    assert_eq!(
        stats.header_and_tables + stats.transition_bytes + stats.padding_bytes,
        stats.file_len
    );
    assert_eq!(stats.file_len, std::fs::metadata(&path).unwrap().len());
    assert_eq!(stats.shape_summary(), "3,2,4");
    assert_eq!(stats.positions, Some(brute_force(&a) as f64));
}

#[test]
fn shape_summaries_are_run_length_encoded() {
    let a = Automaton::new(vec![3, 3, 3, 3, 5]);
    let (_tmp, path) = build(&a);
    let stats = Stats::collect(&Dfa::open(&path).unwrap(), false).unwrap();
    assert_eq!(stats.shape_summary(), "3x4,5");
    assert_eq!(stats.positions, None);
}

#[test]
fn counts_print_exactly_while_they_are_exact() {
    assert_eq!(format_positions(0.0), "0");
    assert_eq!(format_positions(1.0), "1");
    assert_eq!(format_positions(34573426.0), "34573426");
    // Past 2^53 an f64 cannot be trusted digit for digit, so stop pretending.
    assert!(format_positions(1.0e30).contains('e'));
    assert!(format_positions(9007199254740994.0).contains('e'));
}
