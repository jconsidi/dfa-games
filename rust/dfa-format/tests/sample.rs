//! Uniform sampling over a DFA's language.

use std::collections::BTreeSet;
use std::path::Path;

use dfa_format::{convert, Automaton, Dfa, LegacyDfa, Rng, Sampler};
use tempfile::TempDir;

fn published(a: &Automaton, tmp: &TempDir, tag: &str) -> Dfa {
    let src = tmp.path().join(format!("legacy-{tag}"));
    a.write_legacy_dir(&src).unwrap();
    let legacy = LegacyDfa::open(&src).unwrap();
    let converted = convert(&legacy, Path::new(tmp.path()), true).unwrap();
    Dfa::open(&converted.path).unwrap()
}

#[test]
fn every_string_of_a_small_language_turns_up() {
    let mut a = Automaton::new(vec![2u32, 2]);
    a.set_initial_state(1);
    let tmp = TempDir::new().unwrap();
    let dfa = published(&a, &tmp, "all");

    let sampler = Sampler::new(&dfa).unwrap();
    assert_eq!(sampler.total(), 4.0);

    let mut rng = Rng::new(7);
    let mut seen = BTreeSet::new();
    for _ in 0..200 {
        seen.insert(sampler.sample(&mut rng).unwrap());
    }
    assert_eq!(seen.len(), 4, "{seen:?}");
}

#[test]
fn an_empty_language_yields_nothing() {
    // The reject DFA, which the verifiers meet routinely.
    let a = Automaton::new(vec![2u32, 3, 2]);
    assert_eq!(a.initial_state(), 0);
    let tmp = TempDir::new().unwrap();
    let dfa = published(&a, &tmp, "none");

    let sampler = Sampler::new(&dfa).unwrap();
    assert_eq!(sampler.total(), 0.0);
    assert!(sampler.sample(&mut Rng::new(1)).is_none());
}

#[test]
fn every_sample_is_accepted() {
    // The property that makes a sample usable as a witness at all.
    let mut a = Automaton::new(vec![3u32, 2, 4, 2]);
    let l3 = a.add_state(3, vec![1, 0]);
    let l2a = a.add_state(2, vec![l3, 0, l3, 1]);
    let l2b = a.add_state(2, vec![0, l3, 0, 0]);
    let l1 = a.add_state(1, vec![l2a, l2b]);
    let start = a.add_state(0, vec![l1, 0, l1]);
    a.set_initial_state(start);

    let tmp = TempDir::new().unwrap();
    let dfa = published(&a, &tmp, "tangled");
    let sampler = Sampler::new(&dfa).unwrap();
    assert!(sampler.total() > 0.0);

    let mut rng = Rng::new(99);
    for _ in 0..500 {
        let s = sampler.sample(&mut rng).unwrap();
        assert!(
            dfa.accepts(&s).unwrap(),
            "{s:?} was sampled but is not accepted"
        );
    }
}

#[test]
fn sampling_is_uniform_over_the_language_not_over_paths() {
    // Character 0 opens onto four strings, character 1 onto exactly one. A
    // sampler that picked characters uniformly would split the draws in half;
    // weighting by suffix counts splits them 4 to 1. This is the whole
    // difference between the two designs, so it gets its own test.
    let mut a = Automaton::new(vec![2u32, 2, 2]);
    let u = a.add_state(2, vec![1, 0]);
    let t = a.add_state(1, vec![u, 0]);
    let start = a.add_state(0, vec![1, t]);
    a.set_initial_state(start);

    let tmp = TempDir::new().unwrap();
    let dfa = published(&a, &tmp, "skewed");
    let sampler = Sampler::new(&dfa).unwrap();
    assert_eq!(sampler.total(), 5.0);

    let mut rng = Rng::new(12345);
    let draws = 1000;
    let leading_zero = (0..draws)
        .filter(|_| sampler.sample(&mut rng).unwrap()[0] == 0)
        .count();

    assert!(
        (700..900).contains(&leading_zero),
        "expected about 800 of {draws} to start with 0, got {leading_zero}"
    );
}

#[test]
fn a_seed_reproduces_its_samples() {
    // A witness that only appears on some runs would be worth very little.
    let mut a = Automaton::new(vec![3u32, 3]);
    let l1 = a.add_state(1, vec![1, 0, 1]);
    let start = a.add_state(0, vec![l1, 0, l1]);
    a.set_initial_state(start);

    let tmp = TempDir::new().unwrap();
    let dfa = published(&a, &tmp, "seeded");
    let sampler = Sampler::new(&dfa).unwrap();

    let draw = |seed| {
        let mut rng = Rng::new(seed);
        (0..50)
            .map(|_| sampler.sample(&mut rng).unwrap())
            .collect::<Vec<_>>()
    };
    assert_eq!(draw(4), draw(4));
    assert_ne!(draw(4), draw(5));
}
