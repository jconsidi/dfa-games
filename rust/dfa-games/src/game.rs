//! The position level rules a verifier checks DFAs against.
//!
//! This is the part of `src/Game.h` that verification needs, and only that
//! part: `validate_moves`, `validate_result` and `position_to_string`.  Move
//! *graphs*, which is how the C++ builds DFAs, are not here and are not needed
//! — the verifiers only read DFAs.

/// A position is one character per board square, the string the DFA accepts.
pub type Position = [u32];

/// Which side is to move, 0 or 1, as in the C++.
pub type Side = u32;

pub trait Game: Send + Sync {
    fn name(&self) -> &str;

    fn shape(&self) -> &[u32];

    /// Every position reachable by one legal move.  Empty means terminal.
    fn validate_moves(&self, side_to_move: Side, position: &Position) -> Vec<Vec<u32>>;

    /// The game result at a terminal position, from the point of view of the
    /// side to move: `Some(-1)` lost, `Some(0)` drawn, `Some(1)` won, `None`
    /// not terminal.
    ///
    /// `moves` is the output of `validate_moves` for the same position, passed
    /// in because the normal play rule *is* a statement about it and because
    /// the verifiers have it in hand already — generating moves twice per
    /// position is not free at 10^8 positions.  A game whose result does not
    /// follow from the move list (chess, where an empty list is checkmate or
    /// stalemate depending on whether the king is attacked) overrides this and
    /// ignores the argument.
    fn validate_result(
        &self,
        _side_to_move: Side,
        _position: &Position,
        moves: &[Vec<u32>],
    ) -> Option<i32> {
        // NormalPlayGame::validate_result: out of moves is a loss.
        if moves.is_empty() {
            Some(-1)
        } else {
            None
        }
    }

    /// A human readable board, used in failure reports.
    fn position_to_string(&self, position: &Position) -> String;
}
