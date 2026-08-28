//! Checking a DFA relation of the form `L(A) = L(B) ∪ L(C)`.
//!
//! Independent of the solver: this reads transition tables and nothing else.
//! The three automata must agree on ndim and shape, which is the only thing
//! that makes the walk below meaningful.
//!
//! # The walk
//!
//! Triples `(a, b, c)` are visited from the initial states, layer by layer.
//! Each visited triple is stepped on every character and the successors
//! visited in turn.  At the terminal pseudo-layer every state is 0 or 1, so
//! the obligation `accept_A == accept_B || accept_C` is exactly what the
//! trivial dispatch below already checks, and needs no separate case.
//!
//! # Why the memo is keyed on `(b, c)` and not on the triple
//!
//! Quotients distribute over union, so the residual of `L(B) ∪ L(C)` after
//! any prefix depends only on `(b, c)`.  If `A` is minimal, distinct states in
//! a layer have distinct residuals, so the correct `a` is a *function* of
//! `(b, c)`.  Two consequences:
//!
//! - reaching one pair with two different `a` values is a failure;
//! - work is bounded by reachable *pairs*, not triples.
//!
//! That is licensed by minimality, so `A` is **required** to carry the format's
//! canonical flag, which asserts it; a triple whose `A` does not is rejected
//! rather than walked.  The flag is an assertion in the file and this function
//! does not re-derive it — `dfa-convert` sets it only after checking, and
//! `dfa-validate` re-checks it — so the guarantee here is exactly as good as
//! that flag.  Given it, a conflict refutes `A = B ∪ C`.
//!
//! The same reasoning is why `a == 1` and `a == 0` can be *required* in the
//! trivial cases rather than checked by descending: in a minimal automaton the
//! accept-everything residual is state 1 and the reject-everything residual is
//! state 0, because the format reserves those two indices for exactly them.
//!
//! # Dispatch
//!
//! States 0 (reject all) and 1 (accept all) are numbered first in every layer,
//! so the dispatch is a comparison against 2 and never inspects state
//! contents:
//!
//! | condition           | obligation        | memo         |
//! |---------------------|-------------------|--------------|
//! | `b == 1 \|\| c == 1`  | `a == 1`          | none, stop   |
//! | `b == 0 && c == 0`  | `a == 0`          | none, stop   |
//! | `b == 0`            | `L(a) == L(c)`    | keyed on `c` |
//! | `c == 0`            | `L(a) == L(b)`    | keyed on `b` |
//! | otherwise           | `L(a) == L(b) ∪ L(c)` | keyed on `(b, c)` |
//!
//! The subtree below a collapsed `(0, c)` pair is *not* pruned: those states
//! are reachable from pairs with a non-trivial `b` as well, so they are walked
//! regardless.  Collapsing saves memo entries, not traversal.
//!
//! # Cost
//!
//! Worst case is the product, `sum over layers of |B_i| * |C_i| * |alphabet|`.
//! Subquadratic is not on offer: with `A` accept-all this problem contains
//! 2-DFA intersection emptiness, where the product construction is still the
//! best known and SETH implies quadratic is needed.
//!
//! Building the union and comparing digests would cost the same asymptotically
//! and add minimization, canonical renumbering and an output write on top.

use std::collections::HashMap;

use crate::error::{FormatError, Result};
use crate::layout::{STATE_ACCEPT, STATE_REJECT};
use crate::read::Dfa;
use crate::sample::{Rng, Sampler};

/// Marks an unfilled slot in the single-sided memos.  A real state number can
/// never be this, since a layer that large cannot be addressed at all.
const UNVISITED: u64 = u64::MAX;

/// How the walk divided up, recorded because it says whether the single-sided
/// memos are carrying real load or are noise.  Binary unions and n-ary unions
/// over invariant-partitioned components are expected to look quite different
/// here.
#[derive(Debug, Default, Clone, Copy)]
pub struct UnionStats {
    /// Pairs with both `b` and `c` ordinary, held in the hash map.
    pub pairs_both: u64,
    /// Pairs with `b` reject-all, keyed on `c` in a flat vector.
    pub pairs_b_reject: u64,
    /// Pairs with `c` reject-all, keyed on `b` in a flat vector.
    pub pairs_c_reject: u64,
    /// Triples cut short by `b` or `c` being accept-all.
    pub stops_accept: u64,
    /// Triples cut short by both `b` and `c` being reject-all.
    pub stops_reject: u64,
    /// Triples stepped, i.e. transitions followed in all three automata.
    pub steps: u64,
}

/// Which memo a conflict was found in.
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum MemoKey {
    /// `b` was reject-all, so the obligation was keyed on this `c`.
    C(u64),
    /// `c` was reject-all, so the obligation was keyed on this `b`.
    B(u64),
    Pair(u64, u64),
}

#[derive(Debug, Clone)]
pub enum UnionFailure {
    /// A trivial case demanded a specific `a` and got something else.
    Rule {
        layer: usize,
        a: u64,
        b: u64,
        c: u64,
        required_a: u64,
        because: &'static str,
    },

    /// One `(b, c)` pair was reached with two different `a` values.  Since `A`
    /// is minimal, the residual after a prefix is determined by `(b, c)`, so
    /// two answers means `A` is not the union.
    Conflict {
        layer: usize,
        key: MemoKey,
        first_a: u64,
        second_a: u64,
    },

    /// A sampled string is in one side and not the other.  Re-checkable in
    /// O(n) by anything that can read a DFA.
    Witness {
        string: Vec<u32>,
        in_a: bool,
        in_b: bool,
        in_c: bool,
    },
}

impl std::fmt::Display for UnionFailure {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            UnionFailure::Rule {
                layer,
                a,
                b,
                c,
                required_a,
                because,
            } => write!(
                f,
                "layer {layer}: triple (a={a}, b={b}, c={c}) requires a == {required_a} because {because}"
            ),

            UnionFailure::Conflict {
                layer,
                key,
                first_a,
                second_a,
            } => {
                let key = match key {
                    MemoKey::C(c) => format!("c={c} (with b reject-all)"),
                    MemoKey::B(b) => format!("b={b} (with c reject-all)"),
                    MemoKey::Pair(b, c) => format!("(b={b}, c={c})"),
                };
                write!(
                    f,
                    "layer {layer}: {key} was reached with a={first_a} and again with \
                     a={second_a}, so A distinguishes two prefixes that B union C does not"
                )
            }

            UnionFailure::Witness {
                string,
                in_a,
                in_b,
                in_c,
            } => write!(
                f,
                "{string:?} is {}in A but {}in B union C (B: {in_b}, C: {in_c})",
                if *in_a { "" } else { "not " },
                if *in_b || *in_c { "" } else { "not " }
            ),
        }
    }
}

#[derive(Debug, Clone)]
pub struct UnionReport {
    pub stats: UnionStats,
    /// The first failure found.  The walk stops there: after a conflict the
    /// memo no longer means what the rest of the walk assumes, and one
    /// refutation is the whole answer anyway.
    pub failure: Option<UnionFailure>,
}

impl UnionReport {
    pub fn holds(&self) -> bool {
        self.failure.is_none()
    }
}

/// One layer's worth of memo, plus the triples still to be stepped.
struct Frontier {
    /// Indexed by `c`, for pairs whose `b` is reject-all.
    by_c: Vec<u64>,
    /// Indexed by `b`, for pairs whose `c` is reject-all.
    by_b: Vec<u64>,
    /// Everything else.  A hash map on the tuple, deliberately not an
    /// arithmetic pair index: `b * width + c` silently retracts distinct pairs
    /// onto one slot as soon as the factor is wrong for the layer.
    both: HashMap<(u64, u64), u64>,
    queue: Vec<(u64, u64, u64)>,
}

impl Frontier {
    fn new(b_states: usize, c_states: usize) -> Frontier {
        Frontier {
            by_c: vec![UNVISITED; c_states],
            by_b: vec![UNVISITED; b_states],
            both: HashMap::new(),
            queue: Vec::new(),
        }
    }
}

fn states_at(dfa: &Dfa, layer: usize) -> u64 {
    let layout = dfa.layout();
    if layer < layout.ndim() {
        layout.layer_size()[layer]
    } else {
        crate::layout::TERMINAL_LAYER_SIZE
    }
}

fn usize_states(dfa: &Dfa, layer: usize, which: &str) -> Result<usize> {
    let n = states_at(dfa, layer);
    usize::try_from(n).map_err(|_| {
        FormatError::Other(format!(
            "{which} layer {layer} has {n} states, too many to hold a memo for"
        ))
    })
}

/// Check `L(A) = L(B) ∪ L(C)`.
///
/// Returns a report rather than a bare bool: the statistics are wanted even on
/// success, and the failure carries enough to act on.
pub fn verify_dfa_union(a: &Dfa, b: &Dfa, c: &Dfa) -> Result<UnionReport> {
    let layout = a.layout();
    let ndim = layout.ndim();

    for (other, name) in [(b, "B"), (c, "C")] {
        if other.layout().ndim() != ndim || other.layout().shape() != layout.shape() {
            return Err(FormatError::Other(format!(
                "A is shaped {:?} but {name} is shaped {:?}; a union relation between \
                 different shapes is not meaningful",
                layout.shape(),
                other.layout().shape()
            )));
        }
    }

    // The pair keyed memo below is only sound if the correct `a` is a function
    // of `(b, c)`, which needs A minimal. The canonical flag asserts exactly
    // that, so require it rather than silently producing a result whose
    // meaning depends on something unchecked.
    if !a.header().canonical() {
        return Err(FormatError::Other(
            "A does not carry the canonical flag, which is what asserts it is minimal. \
             This check requires it: without minimality two distinct states of A can share \
             a residual, and a disagreement would not distinguish \"A is not the union\" \
             from \"A is not minimal\". Republish A with dfa-convert, and confirm with \
             dfa-validate."
                .to_string(),
        ));
    }

    let mut stats = UnionStats::default();

    let mut current = Frontier::new(usize_states(b, 0, "B")?, usize_states(c, 0, "C")?);
    if let Some(failure) = visit(
        &mut current,
        &mut stats,
        0,
        a.header().initial_state,
        b.header().initial_state,
        c.header().initial_state,
    ) {
        return Ok(UnionReport {
            stats,
            failure: Some(failure),
        });
    }

    for layer in 0..ndim {
        let shape = layout.shape()[layer];
        let mut next = Frontier::new(
            usize_states(b, layer + 1, "B")?,
            usize_states(c, layer + 1, "C")?,
        );

        // Only the current and the next layer are ever live, so peak memory is
        // two layers of frontier rather than the whole product.
        for &(av, bv, cv) in &current.queue {
            for sigma in 0..shape {
                stats.steps += 1;
                let failure = visit(
                    &mut next,
                    &mut stats,
                    layer + 1,
                    a.entry(layer, av, sigma),
                    b.entry(layer, bv, sigma),
                    c.entry(layer, cv, sigma),
                );
                if let Some(failure) = failure {
                    return Ok(UnionReport {
                        stats,
                        failure: Some(failure),
                    });
                }
            }
        }

        current = next;
    }

    Ok(UnionReport {
        stats,
        failure: None,
    })
}

/// Dispatch one triple: check the trivial obligations, or memoize and queue.
fn visit(
    frontier: &mut Frontier,
    stats: &mut UnionStats,
    layer: usize,
    a: u64,
    b: u64,
    c: u64,
) -> Option<UnionFailure> {
    const ACCEPT: u64 = STATE_ACCEPT as u64;
    const REJECT: u64 = STATE_REJECT as u64;

    let rule = |required_a: u64, because: &'static str| {
        Some(UnionFailure::Rule {
            layer,
            a,
            b,
            c,
            required_a,
            because,
        })
    };

    if b == ACCEPT || c == ACCEPT {
        stats.stops_accept += 1;
        if a != ACCEPT {
            return rule(ACCEPT, "B or C accepts every continuation, so A must too");
        }
        return None;
    }

    if b == REJECT && c == REJECT {
        stats.stops_reject += 1;
        if a != REJECT {
            return rule(REJECT, "B and C both reject every continuation, so A must too");
        }
        return None;
    }

    // Exactly one of the two sides may still be reject-all, which collapses
    // the obligation to an equality against the other side alone.
    let (slot, key, counter) = if b == REJECT {
        (
            &mut frontier.by_c[c as usize],
            MemoKey::C(c),
            &mut stats.pairs_b_reject,
        )
    } else if c == REJECT {
        (
            &mut frontier.by_b[b as usize],
            MemoKey::B(b),
            &mut stats.pairs_c_reject,
        )
    } else {
        let entry = frontier.both.entry((b, c)).or_insert(UNVISITED);
        (entry, MemoKey::Pair(b, c), &mut stats.pairs_both)
    };

    if *slot == UNVISITED {
        *slot = a;
        *counter += 1;
        frontier.queue.push((a, b, c));
        return None;
    }

    if *slot != a {
        return Some(UnionFailure::Conflict {
            layer,
            key,
            first_a: *slot,
            second_a: a,
        });
    }

    None
}

/// Sample strings from all three languages and check them against the others.
///
/// A cheap pre-filter for [`verify_dfa_union`]: it costs `O(samples * ndim)`
/// after one dynamic program per automaton, and any string it finds is a
/// refutation witness that can be re-checked without trusting this crate.
///
/// Sampling `L(A)` alone would only ever catch `A ⊄ B ∪ C`, so `L(B)` and
/// `L(C)` are sampled too, which catches the other direction.
pub fn sample_for_witness(
    a: &Dfa,
    b: &Dfa,
    c: &Dfa,
    samples: u32,
    seed: u64,
) -> Result<Option<UnionFailure>> {
    if samples == 0 {
        return Ok(None);
    }

    let mut rng = Rng::new(seed);
    let samplers = [Sampler::new(a)?, Sampler::new(b)?, Sampler::new(c)?];

    for sampler in &samplers {
        for _ in 0..samples {
            let Some(string) = sampler.sample(&mut rng) else {
                break; // empty language, nothing to draw
            };

            let in_a = a.accepts(&string)?;
            let in_b = b.accepts(&string)?;
            let in_c = c.accepts(&string)?;

            if in_a != (in_b || in_c) {
                return Ok(Some(UnionFailure::Witness {
                    string,
                    in_a,
                    in_b,
                    in_c,
                }));
            }
        }
    }

    Ok(None)
}
