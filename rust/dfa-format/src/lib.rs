//! Reader, writer and validator for the DFA single-file format.
//!
//! See `FORMAT-DFA.md` at the repository root for the specification.  Section
//! references in the source refer to it.
//!
//! [`layout`] is the single authority on where bytes go; everything else in
//! this crate goes through it.

pub mod error;
pub mod layout;

pub use error::{FormatError, Result, Violation};
pub use layout::Layout;

/// Lower case hex, the spelling used for every digest this crate prints.
pub fn hex(bytes: &[u8]) -> String {
    use std::fmt::Write;
    let mut out = String::with_capacity(bytes.len() * 2);
    for b in bytes {
        let _ = write!(out, "{b:02x}");
    }
    out
}

pub fn is_hash(s: &str) -> bool {
    s.len() == 64
        && s.bytes()
            .all(|b| b.is_ascii_hexdigit() && !b.is_ascii_uppercase())
}
