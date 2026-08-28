//! Game rules and the verifiers that check solved DFAs against them.
//!
//! The DFA file format lives in `dfa-format`, which knows nothing about games;
//! this crate knows the rules and nothing about how a DFA is stored.  The
//! verifiers in [`verify`] are the join: they enumerate a DFA's positions and
//! check each one against [`game::Game`].
//!
//! Only games whose rules are ported are here — see [`registry::get_game`].

pub mod amazons;
pub mod breakthrough;
pub mod game;
pub mod load;
pub mod registry;
pub mod verify;

pub use game::Game;
pub use registry::{get_game, parse_side_to_move};
