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
//! That reasoning needs `A` minimal, and the same is true of the trivial cases
//! below, where `a == 1` and `a == 0` are *required* rather than checked by
//! descending: in a minimal automaton the accept-everything residual is state 1
//! and the reject-everything residual is state 0, because the format reserves
//! those two indices for exactly them.  A non-minimal `A` could instead reach an
//! ordinary state with either residual and be reported wrongly.
//!
//! `A` is *not* required to carry the format's canonical flag, which is what
//! asserts minimality.  Every triple is walked and every disagreement is a
//! failure, but a failure carries a [`Caveat`] saying how much it proves:
//!
//! - at the terminal pseudo-layer every state is 0 or 1, so a disagreement
//!   there is about acceptance itself and holds whatever `A`'s numbering is
//!   like — [`Caveat::Definitive`];
//! - earlier than that the argument rests on minimality, which the flag
//!   asserts — [`Caveat::RestsOnCanonicalFlag`];
//! - earlier than that with no flag, the disagreement may be an artifact of
//!   `A`'s numbering rather than a difference in language —
//!   [`Caveat::MayNotBeMinimal`].
//!
//! The flag is an assertion made by whatever wrote the file, and this module
//! does not re-derive it.
//!
//! # Refutations are errors
//!
//! [`verify_dfa_union`] returns its statistics on success and an error on a
//! refutation.  There is deliberately no form that hands a refutation back as
//! a value: a caller that forgets to inspect it has checked nothing, and a run
//! that skipped the check looks exactly like one that passed it.  The error
//! carries the disagreement and how far the walk got, so a caller can print
//! whatever detail it likes before giving up.
//!
//! This diverges from `read::validate`, which reports conformance and lets the
//! caller judge.  The difference is that a union relation is asserted by
//! whoever calls this, so failing it is not a judgement call.
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

/// How much a failure proves, which depends on whether `A` is minimal.
///
/// Only the terminal layer is free of that dependence, so only it is
/// unconditional.  Everything else is still reported as a failure — a
/// disagreement is a disagreement — but with the assumption it rests on named.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum Caveat {
    /// Found at the terminal pseudo-layer, where every state is 0 or 1.  A
    /// difference in acceptance, independent of how A is numbered.
    Definitive,

    /// Found earlier, so it rests on A being minimal, which A's canonical flag
    /// asserts.
    RestsOnCanonicalFlag,

    /// Found earlier, and A does not carry the canonical flag.  If A is not
    /// minimal then two of its states can share a residual, and this may be an
    /// artifact of the numbering rather than a difference in language.
    MayNotBeMinimal { a_name: String },
}

impl Caveat {
    fn of(terminal: bool, a_canonical: bool, a_name: &str) -> Caveat {
        if terminal {
            Caveat::Definitive
        } else if a_canonical {
            Caveat::RestsOnCanonicalFlag
        } else {
            Caveat::MayNotBeMinimal {
                a_name: a_name.to_string(),
            }
        }
    }

    /// Whether the failure holds without assuming anything about A.
    pub fn is_definitive(&self) -> bool {
        matches!(self, Caveat::Definitive)
    }

    fn note(&self) -> String {
        match self {
            Caveat::Definitive => String::new(),
            Caveat::RestsOnCanonicalFlag => {
                ". This rests on A being minimal, which A's canonical flag asserts".to_string()
            }
            Caveat::MayNotBeMinimal { a_name } => format!(
                ". This rests on A being minimal, and A \"{a_name}\" does not carry the \
                 canonical flag that would assert it: if A is not minimal, two of its states \
                 can share a residual and this disagreement may be an artifact of A's \
                 numbering rather than a difference in language"
            ),
        }
    }
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
        caveat: Caveat,
    },

    /// One `(b, c)` pair was reached with two different `a` values.  Since `A`
    /// is minimal, the residual after a prefix is determined by `(b, c)`, so
    /// two answers means `A` is not the union.
    Conflict {
        layer: usize,
        key: MemoKey,
        first_a: u64,
        second_a: u64,
        caveat: Caveat,
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
                caveat,
            } => write!(
                f,
                "layer {layer}: triple (a={a}, b={b}, c={c}) requires a == {required_a} \
                 because {because}{}",
                caveat.note()
            ),

            UnionFailure::Conflict {
                layer,
                key,
                first_a,
                second_a,
                caveat,
            } => {
                let key = match key {
                    MemoKey::C(c) => format!("c={c} (with b reject-all)"),
                    MemoKey::B(b) => format!("b={b} (with c reject-all)"),
                    MemoKey::Pair(b, c) => format!("(b={b}, c={c})"),
                };
                write!(
                    f,
                    "layer {layer}: {key} was reached with a={first_a} and again with \
                     a={second_a}, so A distinguishes two prefixes that B union C does not{}",
                    caveat.note()
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

/// The parts of a run that do not vary from triple to triple.
struct Context<'a> {
    ndim: usize,
    /// Whether A claims the canonical numbering that asserts minimality.
    a_canonical: bool,
    a_name: &'a str,
}

impl Context<'_> {
    fn caveat(&self, layer: usize) -> Caveat {
        Caveat::of(layer == self.ndim, self.a_canonical, self.a_name)
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
/// Each automaton is passed with the name it is known by, as the game
/// verifiers take theirs, so a refutation can say which files were at fault
/// rather than which letters.
///
/// Returns the walk's statistics on success.  A refutation is an error, and
/// carries the disagreement and the partial statistics with it; see the module
/// documentation for why.
pub fn verify_dfa_union(

    a: &Dfa,
    a_name: &str,
    b: &Dfa,
    b_name: &str,
    c: &Dfa,
    c_name: &str,
) -> Result<UnionStats> {
    let layout = a.layout();
    let ndim = layout.ndim();

    for (other, role, other_name) in [(b, "B", b_name), (c, "C", c_name)] {
        if other.layout().ndim() != ndim || other.layout().shape() != layout.shape() {
            return Err(FormatError::Other(format!(
                "A \"{a_name}\" is shaped {:?} but {role} \"{other_name}\" is shaped {:?}; \
                 a union relation between different shapes is not meaningful",
                layout.shape(),
                other.layout().shape()
            )));
        }
    }

    // Not a precondition: a triple whose A is not canonical is still walked,
    // and any disagreement is still a failure. What changes is how much the
    // failure proves, which each one carries as a Caveat.
    let context = Context {
        ndim,
        a_canonical: a.header().canonical(),
        a_name,
    };

    let mut stats = UnionStats::default();

    let mut current = Frontier::new(usize_states(b, 0, "B")?, usize_states(c, 0, "C")?);
    if let Some(failure) = visit(
        &mut current,
        &mut stats,
        0,
        &context,
        a.header().initial_state,
        b.header().initial_state,
        c.header().initial_state,
    ) {
        return Err(refuted(a_name, b_name, c_name, failure, stats));
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
                    &context,
                    a.entry(layer, av, sigma),
                    b.entry(layer, bv, sigma),
                    c.entry(layer, cv, sigma),
                );
                if let Some(failure) = failure {
                    return Err(refuted(a_name, b_name, c_name, failure, stats));
                }
            }
        }

        current = next;
    }

    Ok(stats)
}

/// The error a refutation becomes.
///
/// The statistics are the walk up to the disagreement, not a measurement of
/// the triple -- the walk stops at the first one -- so they are labelled as
/// such rather than presented as totals.
fn refuted(
    a_name: &str,
    b_name: &str,
    c_name: &str,
    failure: UnionFailure,
    stats: UnionStats,
) -> FormatError {
    let message = format!(
        "\"{a_name}\" is not the union of \"{b_name}\" and \"{c_name}\": {failure}\n\
         walk stopped after {} triples stepped, having reached {} pairs with both sides \
         non-trivial, {} with b reject-all and {} with c reject-all",
        stats.steps, stats.pairs_both, stats.pairs_b_reject, stats.pairs_c_reject
    );

    FormatError::Refuted {
        message,
        failure: Box::new(failure),
        stats: Some(stats),
    }
}

/// Dispatch one triple: check the trivial obligations, or memoize and queue.
fn visit(
    frontier: &mut Frontier,
    stats: &mut UnionStats,
    layer: usize,
    context: &Context,
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
            caveat: context.caveat(layer),
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
            // A conflict needs an ordinary b or c, which the terminal layer
            // does not have, so `layer` is never ndim here and this is never
            // Definitive.
            caveat: context.caveat(layer),
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
///
/// A witness is a refutation, so it comes back as an error like any other.
pub fn sample_for_witness(a: &Dfa, b: &Dfa, c: &Dfa, samples: u32, seed: u64) -> Result<()> {
    if samples == 0 {
        return Ok(());
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
                let failure = UnionFailure::Witness {
                    string,
                    in_a,
                    in_b,
                    in_c,
                };
                return Err(FormatError::Refuted {
                    message: failure.to_string(),
                    failure: Box::new(failure),
                    // No walk happened, so there is nothing to report about one.
                    stats: None,
                });
            }
        }
    }

    Ok(())
}
