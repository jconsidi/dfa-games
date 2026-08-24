//! Error and violation types.
//!
//! Two distinct things are modelled here.  A [`FormatError`] is a condition
//! that stops work: an I/O failure, or a file so damaged that parsing cannot
//! continue.  A [`Violation`] is one specific way a file fails to conform;
//! the validator collects every violation it can find rather than stopping at
//! the first, because when a file is wrong you want the whole story.

use std::path::PathBuf;

pub type Result<T> = std::result::Result<T, FormatError>;

#[derive(Debug, thiserror::Error)]
pub enum FormatError {
    #[error("{path}: {source}")]
    Io {
        path: PathBuf,
        #[source]
        source: std::io::Error,
    },

    #[error("{0}")]
    Io2(#[from] std::io::Error),

    /// The layout arithmetic overflowed, i.e. the described automaton cannot
    /// be represented in a file at all.
    #[error("layout overflow: {0}")]
    Overflow(String),

    /// A legacy `scratch/<name>` directory that is not a well formed DFA.
    #[error("{path}: {message}")]
    BadLegacy { path: PathBuf, message: String },

    /// A `.dfa` file that failed a required check from FORMAT-DFA.md section 7.
    #[error("{path}: {message}")]
    Invalid { path: PathBuf, message: String },

    #[error("{0}")]
    Other(String),
}

impl FormatError {
    pub fn io(path: impl Into<PathBuf>, source: std::io::Error) -> Self {
        FormatError::Io {
            path: path.into(),
            source,
        }
    }

    pub fn bad_legacy(path: impl Into<PathBuf>, message: impl Into<String>) -> Self {
        FormatError::BadLegacy {
            path: path.into(),
            message: message.into(),
        }
    }

    pub fn invalid(path: impl Into<PathBuf>, message: impl Into<String>) -> Self {
        FormatError::Invalid {
            path: path.into(),
            message: message.into(),
        }
    }
}

/// One way in which a file fails to conform to the specification.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Violation {
    /// Byte offset the problem was found at, when there is a meaningful one.
    pub offset: Option<u64>,
    pub message: String,
}

impl Violation {
    pub fn at(offset: u64, message: impl Into<String>) -> Self {
        Violation {
            offset: Some(offset),
            message: message.into(),
        }
    }

    pub fn new(message: impl Into<String>) -> Self {
        Violation {
            offset: None,
            message: message.into(),
        }
    }
}

impl std::fmt::Display for Violation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self.offset {
            Some(offset) => write!(f, "at 0x{offset:x} ({offset}): {}", self.message),
            None => write!(f, "{}", self.message),
        }
    }
}
