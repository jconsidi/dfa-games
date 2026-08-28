//! Position level rules for the games the solver solves.
//!
//! The DFA file format lives in `dfa-format`, which knows nothing about games;
//! this crate knows the rules and nothing about how a DFA is stored.
//!
//! Only games whose rules are ported are here — see [`registry::get_game`].

pub mod amazons;
pub mod breakthrough;
pub mod game;
pub mod load;
pub mod registry;

pub use game::Game;
pub use registry::{get_game, parse_side_to_move};
