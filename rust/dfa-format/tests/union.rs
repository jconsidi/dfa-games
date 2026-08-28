//! `verify_dfa_union` against brute force over the whole shape.
//!
//! The reference is deliberately dumb: enumerate every string of the shape,
//! decide membership by predicate, and build the minimal automaton for that
//! set by grouping prefixes with equal residuals. Nothing about it resembles
//! the product walk under test.

use std::collections::BTreeSet;
use std::path::Path;

use dfa_format::union::{Caveat, UnionFailure};
use dfa_format::{
    convert, sample_for_witness, verify_dfa_union, Automaton, Dfa, FormatError, LegacyDfa,
};

/// The failure and partial statistics a refutation carries, or a panic if the
/// call did not refute.
fn refutation(result: dfa_format::Result<dfa_format::UnionStats>) -> (UnionFailure, dfa_format::UnionStats) {
    match result {
        Ok(stats) => panic!("expected a refutation, but the union held: {stats:?}"),
        Err(FormatError::Refuted { failure, stats, .. }) => {
            (*failure, stats.expect("a walk refutation carries its statistics"))
        }
        Err(other) => panic!("expected a refutation, got {other}"),
    }
}

/// Whether the relation holds, for tests that only care about the verdict.
fn holds(result: dfa_format::Result<dfa_format::UnionStats>) -> bool {
    match result {
        Ok(_) => true,
        Err(FormatError::Refuted { .. }) => false,
        Err(other) => panic!("expected a verdict, got {other}"),
    }
}
use tempfile::TempDir;

/// Every string over `shape[from..]`.
fn suffixes(shape: &[u32], from: usize) -> Vec<Vec<u32>> {
    let mut out = vec![Vec::new()];
    for &s in &shape[from..] {
        let mut next = Vec::new();
        for prefix in &out {
            for c in 0..s {
                let mut extended = prefix.clone();
                extended.push(c);
                next.push(extended);
            }
        }
        out = next;
    }
    out
}

/// The minimal, canonically numbered layered automaton for `accepts`.
///
/// A prefix's residual is the set of suffixes that complete it into the
/// language; two prefixes with equal residuals are the same state, which is
/// minimality. Numbering runs from the last layer backwards, because a row is
/// written in terms of the next layer's numbers: within a layer the ordinary
/// rows are sorted and numbered from 2, which is what the format calls
/// canonical. `convert` only checks that property, it never renumbers, so the
/// helper has to get it right for `verify_dfa_union` to accept the result.
fn minimal_dfa(shape: &[u32], accepts: &dyn Fn(&[u32]) -> bool) -> Automaton {
    type Residual = BTreeSet<Vec<u32>>;

    let ndim = shape.len();

    let residual = |prefix: &[u32]| -> Residual {
        suffixes(shape, prefix.len())
            .into_iter()
            .filter(|s| {
                let mut whole = prefix.to_vec();
                whole.extend_from_slice(s);
                accepts(&whole)
            })
            .collect()
    };

    // Residuals actually reachable at each layer. Only these get states, which
    // is the other half of what canonical numbering demands.
    let mut reachable: Vec<BTreeSet<Residual>> = vec![BTreeSet::new(); ndim + 1];
    let mut prefixes: Vec<Vec<u32>> = vec![Vec::new()];
    for layer in 0..=ndim {
        for prefix in &prefixes {
            reachable[layer].insert(residual(prefix));
        }
        if layer == ndim {
            break;
        }
        prefixes = prefixes
            .iter()
            .flat_map(|p| {
                (0..shape[layer]).map(move |c| {
                    let mut extended = p.clone();
                    extended.push(c);
                    extended
                })
            })
            .collect();
    }

    let mut ids: Vec<std::collections::BTreeMap<Residual, u32>> =
        vec![std::collections::BTreeMap::new(); ndim + 1];
    for r in &reachable[ndim] {
        ids[ndim].insert(r.clone(), if r.is_empty() { 0 } else { 1 });
    }

    let mut ordinary_rows: Vec<Vec<Vec<u32>>> = vec![Vec::new(); ndim];
    for layer in (0..ndim).rev() {
        let rows: Vec<(Residual, Vec<u32>)> = reachable[layer]
            .iter()
            .map(|r| {
                let row = (0..shape[layer])
                    .map(|c| {
                        let quotient: Residual = r
                            .iter()
                            .filter(|s| s[0] == c)
                            .map(|s| s[1..].to_vec())
                            .collect();
                        ids[layer + 1][&quotient]
                    })
                    .collect();
                (r.clone(), row)
            })
            .collect();

        let mut sorted: Vec<Vec<u32>> = rows
            .iter()
            .map(|(_, row)| row.clone())
            .filter(|row| !row.iter().all(|&v| v == 0) && !row.iter().all(|&v| v == 1))
            .collect();
        sorted.sort();
        sorted.dedup();

        for (r, row) in &rows {
            let id = if row.iter().all(|&v| v == 0) {
                0
            } else if row.iter().all(|&v| v == 1) {
                1
            } else {
                2 + sorted.iter().position(|x| x == row).unwrap() as u32
            };
            ids[layer].insert(r.clone(), id);
        }
        ordinary_rows[layer] = sorted;
    }

    let mut a = Automaton::new(shape.to_vec());
    for (layer, rows) in ordinary_rows.iter().enumerate() {
        for row in rows {
            // add_state appends, so sorted order in gives 2, 3, ... out.
            a.add_state(layer, row.clone());
        }
    }
    a.set_initial_state(ids[0][&residual(&[])]);
    a
}

fn publish(a: &Automaton, tmp: &TempDir, tag: &str) -> Dfa {
    let src = tmp.path().join(format!("legacy-{tag}"));
    a.write_legacy_dir(&src).unwrap();
    let legacy = LegacyDfa::open(&src).unwrap();
    let converted = convert(&legacy, Path::new(tmp.path()), true).unwrap();
    Dfa::open(&converted.path).unwrap()
}

/// `publish`, plus the assertion that `minimal_dfa` really did produce
/// canonical numbering. Without this a helper bug would show up as every test
/// failing on the canonical gate, which is a confusing way to find out.
fn publish_canonical(a: &Automaton, tmp: &TempDir, tag: &str) -> Dfa {
    let dfa = publish(a, tmp, tag);
    assert!(
        dfa.header().canonical(),
        "{tag}: minimal_dfa did not produce a canonical automaton"
    );
    dfa
}

/// Publish A, B and C, with A built from a predicate of its own.
fn triple(
    tmp: &TempDir,
    tag: &str,
    shape: &[u32],
    a: &dyn Fn(&[u32]) -> bool,
    b: &dyn Fn(&[u32]) -> bool,
    c: &dyn Fn(&[u32]) -> bool,
) -> (Dfa, Dfa, Dfa) {
    (
        publish_canonical(&minimal_dfa(shape, a), tmp, &format!("{tag}-a")),
        publish(&minimal_dfa(shape, b), tmp, &format!("{tag}-b")),
        publish(&minimal_dfa(shape, c), tmp, &format!("{tag}-c")),
    )
}

/// A handful of predicates over a mixed shape, chosen so the unions of pairs
/// of them are neither empty nor everything.
const SHAPE: [u32; 4] = [3, 2, 3, 2];

fn first_is_zero(s: &[u32]) -> bool {
    s[0] == 0
}
fn last_is_zero(s: &[u32]) -> bool {
    s[3] == 0
}
fn sum_is_even(s: &[u32]) -> bool {
    s.iter().sum::<u32>() % 2 == 0
}
fn never(_: &[u32]) -> bool {
    false
}
fn always(_: &[u32]) -> bool {
    true
}

#[test]
fn a_true_union_holds() {
    let tmp = TempDir::new().unwrap();
    let union = |s: &[u32]| first_is_zero(s) || sum_is_even(s);
    let (a, b, c) = triple(&tmp, "true", &SHAPE, &union, &first_is_zero, &sum_is_even);

    let stats = verify_dfa_union(&a, "A", &b, "B", &c, "C").expect("this union holds");
    // Guard against a vacuous test: the walk must have done real work on the
    // two-sided memo.
    assert!(stats.pairs_both > 0);
    assert!(stats.steps > 0);
}

#[test]
fn the_union_is_checked_both_ways() {
    let tmp = TempDir::new().unwrap();

    // A too small: it misses the strings only C contributes.
    let (a, b, c) = triple(&tmp, "small", &SHAPE, &first_is_zero, &first_is_zero, &sum_is_even);
    assert!(!holds(verify_dfa_union(&a, "A", &b, "B", &c, "C")));

    // A too big: it holds strings neither B nor C has.
    let union = |s: &[u32]| first_is_zero(s) || sum_is_even(s);
    let (a, b, c) = triple(&tmp, "big", &SHAPE, &union, &first_is_zero, &never);
    assert!(!holds(verify_dfa_union(&a, "A", &b, "B", &c, "C")));
}

#[test]
fn a_reject_all_side_collapses_to_equality() {
    let tmp = TempDir::new().unwrap();

    // C empty, so the obligation is A == B and every pair is keyed on b alone.
    let (a, b, c) = triple(&tmp, "cempty", &SHAPE, &first_is_zero, &first_is_zero, &never);
    let stats = verify_dfa_union(&a, "A", &b, "B", &c, "C").expect("this union holds");
    assert_eq!(stats.pairs_both, 0);
    assert_eq!(stats.pairs_b_reject, 0);
    assert!(stats.pairs_c_reject > 0, "{stats:?}");

    // B empty, so the mirror: keyed on c alone.
    let (a, b, c) = triple(&tmp, "bempty", &SHAPE, &first_is_zero, &never, &first_is_zero);
    let stats = verify_dfa_union(&a, "A", &b, "B", &c, "C").expect("this union holds");
    assert_eq!(stats.pairs_both, 0);
    assert!(stats.pairs_b_reject > 0, "{stats:?}");
    assert_eq!(stats.pairs_c_reject, 0);

    // And the equality is really checked, not assumed.
    let (a, b, c) = triple(&tmp, "cempty-bad", &SHAPE, &first_is_zero, &last_is_zero, &never);
    assert!(!holds(verify_dfa_union(&a, "A", &b, "B", &c, "C")));
}

#[test]
fn an_accept_all_side_short_circuits() {
    let tmp = TempDir::new().unwrap();

    let (a, b, c) = triple(&tmp, "ball", &SHAPE, &always, &always, &first_is_zero);
    let stats = verify_dfa_union(&a, "A", &b, "B", &c, "C").expect("this union holds");
    // B is accept-all from its initial state, so the walk stops immediately
    // and never expands anything.
    assert_eq!(stats.stops_accept, 1);
    assert_eq!(stats.steps, 0);

    // A must then be accept-all too.
    let (a, b, c) = triple(&tmp, "ball-bad", &SHAPE, &first_is_zero, &always, &never);
    let (failure, _) = refutation(verify_dfa_union(&a, "A", &b, "B", &c, "C"));
    assert!(matches!(failure, UnionFailure::Rule { required_a: 1, .. }), "{failure:?}");
}

#[test]
fn all_three_empty_holds() {
    let tmp = TempDir::new().unwrap();
    let (a, b, c) = triple(&tmp, "empty", &SHAPE, &never, &never, &never);
    let stats = verify_dfa_union(&a, "A", &b, "B", &c, "C").expect("this union holds");
    assert_eq!(stats.stops_reject, 1);
    assert_eq!(stats.steps, 0);
}

#[test]
fn a_union_with_itself_holds() {
    let tmp = TempDir::new().unwrap();
    let (a, b, c) = triple(&tmp, "self", &SHAPE, &sum_is_even, &sum_is_even, &sum_is_even);
    verify_dfa_union(&a, "A", &b, "B", &c, "C").expect("this union holds");
}

#[test]
fn overlapping_sides_are_fine() {
    let tmp = TempDir::new().unwrap();
    // B and C share strings; union still has to come out right.
    let union = |s: &[u32]| first_is_zero(s) || last_is_zero(s);
    let (a, b, c) = triple(&tmp, "overlap", &SHAPE, &union, &first_is_zero, &last_is_zero);
    verify_dfa_union(&a, "A", &b, "B", &c, "C").expect("this union holds");
}

#[test]
fn exhaustive_over_every_triple_of_small_languages() {
    // Every language over shape [2, 2] is one of 16 subsets, so all 4096
    // ordered triples can be checked. Publishing is the expensive part and
    // there are only 16 distinct automata, so publish once and reuse.
    let shape = [2u32, 2];
    let all: Vec<Vec<u32>> = suffixes(&shape, 0);
    let tmp = TempDir::new().unwrap();

    let member = |mask: u32, s: &[u32]| -> bool {
        let index = all.iter().position(|t| t == s).unwrap();
        mask & (1 << index) != 0
    };

    let published: Vec<Dfa> = (0u32..16)
        .map(|mask| {
            let pred = move |s: &[u32]| member(mask, s);
            publish_canonical(&minimal_dfa(&shape, &pred), &tmp, &format!("mask{mask}"))
        })
        .collect();

    let mut rules = 0;
    let mut conflicts = 0;
    let mut definitive = 0;
    let mut conditional = 0;
    for a_mask in 0u32..16 {
        for b_mask in 0u32..16 {
            for c_mask in 0u32..16 {
                let result = verify_dfa_union(
                    &published[a_mask as usize],
                    &format!("mask{a_mask}"),
                    &published[b_mask as usize],
                    &format!("mask{b_mask}"),
                    &published[c_mask as usize],
                    &format!("mask{c_mask}"),
                );

                let expected = a_mask == (b_mask | c_mask);
                match result {
                    Ok(_) => assert!(
                        expected,
                        "A={a_mask:04b} B={b_mask:04b} C={c_mask:04b} held but should not"
                    ),
                    Err(FormatError::Refuted { failure, .. }) => {
                        assert!(
                            !expected,
                            "A={a_mask:04b} B={b_mask:04b} C={c_mask:04b} refuted but should hold: {failure}"
                        );
                        match *failure {
                            UnionFailure::Rule { caveat, .. } => {
                                rules += 1;
                                if caveat.is_definitive() {
                                    definitive += 1;
                                } else {
                                    conditional += 1;
                                }
                            }
                            UnionFailure::Conflict { caveat, .. } => {
                                conflicts += 1;
                                // Never definitive: a conflict needs an ordinary b or c.
                                assert!(!caveat.is_definitive());
                                conditional += 1;
                            }
                            UnionFailure::Witness { .. } => {
                                unreachable!("the walk emits no witnesses")
                            }
                        }
                    }
                    Err(other) => panic!("unexpected error: {other}"),
                }
            }
        }
    }

    // Both ways of detecting a wrong A have to be exercised, or one of the two
    // code paths is untested here.
    assert!(rules > 0 && conflicts > 0, "rules {rules}, conflicts {conflicts}");
    // Both a failure that needs no assumption about A and one that does.
    assert!(
        definitive > 0 && conditional > 0,
        "definitive {definitive}, conditional {conditional}"
    );
}

#[test]
fn a_non_canonical_a_is_walked_and_the_failure_says_so() {
    // A accepts exactly the union, but splits layer 1 into two equal states,
    // so it is not minimal. The pair keyed memo then sees one pair with two
    // different a values and reports a conflict -- which is, in this case, an
    // artifact of A's numbering and not a difference in language. That is
    // exactly what the caveat is for: the triple is still walked and still
    // fails, but the report says what the failure rests on.
    let shape = vec![2u32, 2];
    let tmp = TempDir::new().unwrap();

    let second_is_zero = |s: &[u32]| s[1] == 0;
    let b = publish(&minimal_dfa(&shape, &second_is_zero), &tmp, "noncanon-b");
    let c = publish(&minimal_dfa(&shape, &never), &tmp, "noncanon-c");

    let mut split = Automaton::new(shape);
    let p = split.add_state(1, vec![1, 0]);
    let q = split.add_state(1, vec![1, 0]); // same row as p, so not minimal
    let start = split.add_state(0, vec![p, q]);
    split.set_initial_state(start);
    let a = publish(&split, &tmp, "noncanon-a");

    for s in suffixes(&[2, 2], 0) {
        assert_eq!(a.accepts(&s).unwrap(), second_is_zero(&s));
    }
    assert!(!a.header().canonical());

    let (failure, _) = refutation(verify_dfa_union(&a, "lost,side_to_move=0", &b, "B", &c, "C"));
    match &failure {
        UnionFailure::Conflict { layer, caveat, .. } => {
            assert_eq!(*layer, 1);
            assert!(!caveat.is_definitive());
            match caveat {
                Caveat::MayNotBeMinimal { a_name } => assert_eq!(a_name, "lost,side_to_move=0"),
                other => panic!("expected MayNotBeMinimal, got {other:?}"),
            }
        }
        other => panic!("expected a conflict, got {other:?}"),
    }

    // And the message names A, so a reader knows which file to look at.
    let text = failure.to_string();
    assert!(text.contains("lost,side_to_move=0"), "{text}");
    assert!(text.contains("canonical flag"), "{text}");
}

#[test]
fn a_canonical_a_gets_no_such_caveat() {
    // The same shape of failure against a minimal A rests on the flag rather
    // than on nothing, and says so differently.
    let tmp = TempDir::new().unwrap();
    let (a, b, c) = triple(&tmp, "canon", &SHAPE, &first_is_zero, &last_is_zero, &never);

    let (failure, _) = refutation(verify_dfa_union(&a, "A", &b, "B", &c, "C"));
    let caveat = match &failure {
        UnionFailure::Rule { caveat, .. } | UnionFailure::Conflict { caveat, .. } => caveat,
        other => panic!("expected a walk failure, got {other:?}"),
    };
    assert!(!matches!(caveat, Caveat::MayNotBeMinimal { .. }), "{caveat:?}");
    assert!(!failure.to_string().contains("does not carry"), "{failure}");
}

#[test]
fn mismatched_shapes_are_an_error() {
    let tmp = TempDir::new().unwrap();
    let a = publish(&minimal_dfa(&[2, 2], &always), &tmp, "shape-a");
    let b = publish(&minimal_dfa(&[2, 2], &always), &tmp, "shape-b");
    let c = publish(&minimal_dfa(&[2, 3], &always), &tmp, "shape-c");

    let err = verify_dfa_union(&a, "the-union", &b, "left-side", &c, "right-side")
        .expect_err("the shapes do not agree")
        .to_string();
    assert!(err.contains("shaped"), "{err}");
    // Both ends of the mismatch, so it says which pair disagrees.
    assert!(err.contains("the-union"), "{err}");
    assert!(err.contains("right-side"), "{err}");
}

#[test]
fn the_prefilter_finds_a_witness_and_stays_quiet_when_there_is_none() {
    let tmp = TempDir::new().unwrap();

    let union = |s: &[u32]| first_is_zero(s) || sum_is_even(s);
    let (a, b, c) = triple(&tmp, "wit-ok", &SHAPE, &union, &first_is_zero, &sum_is_even);
    sample_for_witness(&a, &b, &c, 200, 1).expect("no witness exists for a true union");

    // A is missing everything C contributes, so a sample from C refutes it.
    let (a, b, c) = triple(&tmp, "wit-bad", &SHAPE, &first_is_zero, &first_is_zero, &sum_is_even);
    // A witness is a refutation, so it arrives as an error like any other.
    let err = sample_for_witness(&a, &b, &c, 200, 1).expect_err("A is missing what C contributes");
    let FormatError::Refuted { failure, stats, .. } = err else {
        panic!("expected a refutation, got {err}")
    };
    // Nothing was walked, so there are no walk statistics to report.
    assert!(stats.is_none());
    match *failure {
        UnionFailure::Witness {
            string,
            in_a,
            in_b,
            in_c,
        } => {
            // The witness has to be re-checkable, so check it.
            assert_eq!(a.accepts(&string).unwrap(), in_a);
            assert_eq!(b.accepts(&string).unwrap(), in_b);
            assert_eq!(c.accepts(&string).unwrap(), in_c);
            assert_ne!(in_a, in_b || in_c);
        }
        other => panic!("expected a witness, got {other:?}"),
    }
}

#[test]
fn the_failing_form_propagates_a_refutation() {
    // verify_dfa_union exists so that `?` carries a refutation up. A caller
    // that writes `verify_dfa_union(..)?;` and nothing else is correct, which
    // is the whole difference from the reporting form.
    let tmp = TempDir::new().unwrap();

    let union = |s: &[u32]| first_is_zero(s) || sum_is_even(s);
    let (a, b, c) = triple(&tmp, "fail-ok", &SHAPE, &union, &first_is_zero, &sum_is_even);
    verify_dfa_union(&a, "A", &b, "B", &c, "C").expect("this union holds");

    // A too small: missing everything C contributes.
    let (a, b, c) = triple(
        &tmp,
        "fail-bad",
        &SHAPE,
        &first_is_zero,
        &first_is_zero,
        &sum_is_even,
    );
    let err = verify_dfa_union(&a, "curr", &b, "prev", &c, "new")
        .expect_err("this union does not hold")
        .to_string();

    // The message has to name the relation, not just the disagreement, since
    // the caller checks many of these in a row.
    assert!(err.contains("\"curr\" is not the union of \"prev\" and \"new\""), "{err}");
    // ... and still carry the detail the walk found.
    assert!(err.contains("layer"), "{err}");
}

#[test]
fn the_failing_form_still_errors_on_unusable_input() {
    // A shape mismatch is a different kind of failure -- the check could not
    // be run at all -- but both reach the caller as Err, which is what makes
    // `?` sufficient at the call site.
    let tmp = TempDir::new().unwrap();
    let a = publish(&minimal_dfa(&[2, 2], &always), &tmp, "fs-a");
    let b = publish(&minimal_dfa(&[2, 2], &always), &tmp, "fs-b");
    let c = publish(&minimal_dfa(&[2, 3], &always), &tmp, "fs-c");

    let err = verify_dfa_union(&a, "A", &b, "B", &c, "C")
        .expect_err("the shapes do not agree")
        .to_string();
    assert!(err.contains("shaped"), "{err}");
    assert!(!err.contains("is not the union of"), "{err}");
}
