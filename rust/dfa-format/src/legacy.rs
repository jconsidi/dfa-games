//! Reading the legacy directory-per-DFA layout written by `src/DFA.cpp`.
//!
//! A legacy DFA is a directory holding
//!
//! ```text
//! initial_state   one uint32
//! layer=<i>       uint32[layer_size[i] * shape[i]], row major, i = 0 .. ndim-1
//! size_cache      one double (derived data; not part of the hash)
//! ```
//!
//! The shape is *not* stored: the C++ reader is handed it by the `Game`
//! object.  It is recoverable from the bytes, though, because a conforming
//! writer stores row 0 of every layer as all zeros and row 1 as all ones.  The
//! number of leading zero entries in `layer=i` is therefore exactly `shape[i]`,
//! and `layer_size[i]` follows by division.

use std::fs::{self, File};
use std::io::{BufReader, Read, Seek, SeekFrom};
use std::path::{Path, PathBuf};

use sha2::{Digest, Sha256};

use crate::error::{FormatError, Result};
use crate::hex;

/// Read granularity for the full-file passes.
const CHUNK: usize = 8 << 20;

/// A legacy directory that has passed structural checks.
#[derive(Debug, Clone)]
pub struct LegacyDfa {
    dir: PathBuf,
    resolved_dir: PathBuf,
    shape: Vec<u32>,
    layer_size: Vec<u64>,
    initial_state: u32,
    layer_paths: Vec<PathBuf>,
}

impl LegacyDfa {
    /// Open `dir`, deriving the shape and layer sizes from the layer files.
    pub fn open(dir: &Path) -> Result<LegacyDfa> {
        let resolved_dir = fs::canonicalize(dir).unwrap_or_else(|_| dir.to_path_buf());
        let layer_paths = layer_paths(dir)?;

        let initial_state = read_initial_state(dir)?;

        let mut shape = Vec::with_capacity(layer_paths.len());
        let mut layer_size = Vec::with_capacity(layer_paths.len());
        for (i, path) in layer_paths.iter().enumerate() {
            let (s, n) = describe_layer(dir, i, path)?;
            shape.push(s);
            layer_size.push(n);
        }

        if u64::from(initial_state) >= layer_size[0] {
            return Err(FormatError::bad_legacy(
                dir,
                format!(
                    "initial_state {initial_state} >= layer_size[0] {}",
                    layer_size[0]
                ),
            ));
        }

        Ok(LegacyDfa {
            dir: dir.to_path_buf(),
            resolved_dir,
            shape,
            layer_size,
            initial_state,
            layer_paths,
        })
    }

    pub fn dir(&self) -> &Path {
        &self.dir
    }

    /// The directory the name resolved to, with symlinks followed.  A named
    /// DFA is a symlink into `dfas_by_hash/`, so this is where the stored hash
    /// can be recovered from.
    pub fn resolved_dir(&self) -> &Path {
        &self.resolved_dir
    }

    pub fn ndim(&self) -> usize {
        self.shape.len()
    }

    pub fn shape(&self) -> &[u32] {
        &self.shape
    }

    pub fn layer_size(&self) -> &[u64] {
        &self.layer_size
    }

    pub fn initial_state(&self) -> u32 {
        self.initial_state
    }

    pub fn layer_paths(&self) -> &[PathBuf] {
        &self.layer_paths
    }

    /// The hash the directory claims to have, i.e. its own basename, when that
    /// looks like one of `DFA::calculate_hash()`'s hex digests.
    pub fn stored_hash(&self) -> Option<String> {
        let name = self.resolved_dir.file_name()?.to_str()?;
        crate::is_hash(name).then(|| name.to_string())
    }

    /// A copy of this DFA restricted to its first `ndim` layers.
    ///
    /// Used to recover from directories that hold leftover layer files from an
    /// unrelated DFA; see [`LegacyDfa::check_name`].
    pub fn truncated(&self, ndim: usize) -> LegacyDfa {
        LegacyDfa {
            dir: self.dir.clone(),
            resolved_dir: self.resolved_dir.clone(),
            shape: self.shape[..ndim].to_vec(),
            layer_size: self.layer_size[..ndim].to_vec(),
            initial_state: self.initial_state,
            layer_paths: self.layer_paths[..ndim].to_vec(),
        }
    }

    /// Recompute `DFA::calculate_hash()` (src/DFA.cpp:409) over the contents:
    ///
    /// ```text
    /// sha256( initial_state:u32le || shape:i32le[ndim]
    ///         || layer_sizes:u64le[ndim] || layer=0 bytes || ... )
    /// ```
    ///
    /// The C++ hashes the raw memory of `std::vector<int>` and
    /// `std::vector<size_t>`, so this assumes `sizeof(int) == 4` and
    /// `sizeof(size_t) == 8` -- true for every 64-bit target the C++ builds on.
    pub fn legacy_hash(&self) -> Result<String> {
        self.legacy_hash_prefix(self.ndim())
    }

    /// The same hash, computed as though the DFA had only its first `ndim`
    /// layers.
    fn legacy_hash_prefix(&self, ndim: usize) -> Result<String> {
        let mut hasher = Sha256::new();
        hasher.update(self.initial_state.to_le_bytes());
        for &s in &self.shape[..ndim] {
            hasher.update(s.to_le_bytes());
        }
        for &n in &self.layer_size[..ndim] {
            hasher.update(n.to_le_bytes());
        }

        let mut buf = vec![0u8; CHUNK];
        for path in &self.layer_paths[..ndim] {
            let mut file = File::open(path).map_err(|e| FormatError::io(path, e))?;
            loop {
                let n = file.read(&mut buf).map_err(|e| FormatError::io(path, e))?;
                if n == 0 {
                    break;
                }
                hasher.update(&buf[..n]);
            }
        }

        Ok(hex(&hasher.finalize()))
    }

    /// Largest stored transition in each layer.
    ///
    /// Only the terminal test matters here: the last layer of a well formed
    /// DFA points exclusively at the two reserved states, so a layer whose
    /// maximum is below 2 is a candidate for being the real last layer.
    fn layer_max_entries(&self) -> Result<Vec<u64>> {
        let mut out = Vec::with_capacity(self.ndim());
        let mut buf = vec![0u8; CHUNK];
        for path in &self.layer_paths {
            let mut file = File::open(path).map_err(|e| FormatError::io(path, e))?;
            let mut max = 0u32;
            loop {
                let n = file.read(&mut buf).map_err(|e| FormatError::io(path, e))?;
                if n == 0 {
                    break;
                }
                for w in buf[..n].chunks_exact(4) {
                    let v = u32::from_le_bytes([w[0], w[1], w[2], w[3]]);
                    if v > max {
                        max = v;
                    }
                }
            }
            out.push(u64::from(max));
        }
        Ok(out)
    }

    /// Compare the contents against the name the directory is stored under.
    ///
    /// Some directories in the store hold leftover `layer=` files from an
    /// unrelated DFA.  `DFA::DFA(shape)` builds its scratch directory as
    /// `scratch/temp/<next_dfa_id++>` with a counter that is static per
    /// process (src/DFA.cpp:25), so two concurrent processes march through the
    /// same temp directories in lockstep.  A shorter DFA landing in a
    /// directory an earlier, longer one had used overwrites `layer=0` upward
    /// and leaves the tail behind, then renames the mixture into place under
    /// its own perfectly correct hash.
    ///
    /// The name is therefore evidence, not damage: when some prefix of the
    /// layers reproduces it, that prefix is the DFA that was meant to be
    /// stored here.
    pub fn check_name(&self) -> Result<NameCheck> {
        let Some(stored) = self.stored_hash() else {
            return Ok(NameCheck::Unnamed {
                hash: self.legacy_hash()?,
            });
        };

        let hash = self.legacy_hash()?;
        if hash == stored {
            return Ok(NameCheck::Matches { hash });
        }

        // The real last layer points only at states 0 and 1, which rules out
        // almost every prefix without hashing it.
        let max_entries = self.layer_max_entries()?;
        for ndim in 1..self.ndim() {
            if max_entries[ndim - 1] >= 2 {
                continue;
            }
            if self.legacy_hash_prefix(ndim)? == stored {
                return Ok(NameCheck::Repaired {
                    ndim,
                    hash: stored,
                    extra_layers: self.ndim() - ndim,
                });
            }
        }

        Ok(NameCheck::Mismatch { hash, stored })
    }
}

/// How a directory's contents relate to the name it is stored under.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum NameCheck {
    /// The directory is not named by a hash, so there is nothing to check.
    Unnamed { hash: String },
    /// The contents hash to the directory name.
    Matches { hash: String },
    /// The first `ndim` layers hash to the directory name, and the remaining
    /// `extra_layers` are leftovers from an unrelated DFA.
    Repaired {
        ndim: usize,
        hash: String,
        extra_layers: usize,
    },
    /// Neither the contents nor any prefix of them hash to the directory name.
    Mismatch { hash: String, stored: String },
}

/// `layer=0 .. layer=ndim-1`, requiring the sequence to have no gaps.
fn layer_paths(dir: &Path) -> Result<Vec<PathBuf>> {
    let entries = fs::read_dir(dir).map_err(|e| FormatError::io(dir, e))?;

    let mut numbers = Vec::new();
    for entry in entries {
        let entry = entry.map_err(|e| FormatError::io(dir, e))?;
        let name = entry.file_name();
        let Some(name) = name.to_str() else { continue };
        let Some(rest) = name.strip_prefix("layer=") else {
            continue;
        };
        match rest.parse::<usize>() {
            Ok(n) => numbers.push(n),
            Err(_) => {
                return Err(FormatError::bad_legacy(
                    dir,
                    format!("unparseable layer file name {name:?}"),
                ))
            }
        }
    }

    if numbers.is_empty() {
        return Err(FormatError::bad_legacy(dir, "no layer= files"));
    }
    numbers.sort_unstable();
    for (expected, &got) in numbers.iter().enumerate() {
        if expected != got {
            return Err(FormatError::bad_legacy(
                dir,
                format!(
                    "layer files are not contiguous: expected layer={expected}, found layer={got}"
                ),
            ));
        }
    }

    Ok((0..numbers.len())
        .map(|i| dir.join(format!("layer={i}")))
        .collect())
}

fn read_initial_state(dir: &Path) -> Result<u32> {
    let path = dir.join("initial_state");
    let raw = fs::read(&path).map_err(|e| FormatError::io(&path, e))?;
    let bytes: [u8; 4] = raw.as_slice().try_into().map_err(|_| {
        FormatError::bad_legacy(
            dir,
            format!("initial_state is {} bytes, expected 4", raw.len()),
        )
    })?;
    Ok(u32::from_le_bytes(bytes))
}

/// Derive `(shape[i], layer_size[i])` from one layer file, and check that its
/// two reserved rows hold the required values.
fn describe_layer(dir: &Path, layer: usize, path: &Path) -> Result<(u32, u64)> {
    let file = File::open(path).map_err(|e| FormatError::io(path, e))?;
    let byte_len = file.metadata().map_err(|e| FormatError::io(path, e))?.len();

    if byte_len % 4 != 0 {
        return Err(FormatError::bad_legacy(
            dir,
            format!("layer={layer} is {byte_len} bytes, not a multiple of 4"),
        ));
    }
    let entries = byte_len / 4;
    if entries == 0 {
        return Err(FormatError::bad_legacy(
            dir,
            format!("layer={layer} is empty"),
        ));
    }

    let mut reader = BufReader::with_capacity(1 << 20, file);

    // Row 0 is all zeros, so the leading zero run is exactly shape[layer].
    let mut zeros: u64 = 0;
    let mut word = [0u8; 4];
    loop {
        if zeros == entries {
            return Err(FormatError::bad_legacy(
                dir,
                format!("layer={layer} is entirely zeros, so its shape cannot be derived"),
            ));
        }
        reader
            .read_exact(&mut word)
            .map_err(|e| FormatError::io(path, e))?;
        if u32::from_le_bytes(word) != 0 {
            break;
        }
        zeros += 1;
    }

    if zeros == 0 {
        return Err(FormatError::bad_legacy(
            dir,
            format!("layer={layer} row 0 is not all zeros, so it is not a conforming layer"),
        ));
    }
    let shape = u32::try_from(zeros).map_err(|_| {
        FormatError::bad_legacy(
            dir,
            format!("layer={layer} derived shape {zeros} exceeds u32"),
        )
    })?;

    if entries % zeros != 0 {
        return Err(FormatError::bad_legacy(
            dir,
            format!("layer={layer} has {entries} entries, not a multiple of derived shape {shape}"),
        ));
    }
    let layer_size = entries / zeros;
    if layer_size < 2 {
        return Err(FormatError::bad_legacy(
            dir,
            format!("layer={layer} holds only {layer_size} states, need at least 2"),
        ));
    }

    // Row 1 must be all ones.  The read above already consumed its first
    // entry, which is the value that terminated the zero run.
    if u32::from_le_bytes(word) != 1 {
        return Err(FormatError::bad_legacy(
            dir,
            format!(
                "layer={layer} row 1 starts with {}, expected 1",
                u32::from_le_bytes(word)
            ),
        ));
    }
    for c in 1..zeros {
        reader
            .read_exact(&mut word)
            .map_err(|e| FormatError::io(path, e))?;
        let value = u32::from_le_bytes(word);
        if value != 1 {
            return Err(FormatError::bad_legacy(
                dir,
                format!("layer={layer} row 1 entry {c} is {value}, expected 1"),
            ));
        }
    }

    drop(reader);
    Ok((shape, layer_size))
}

/// Open one layer file positioned at its first ordinary row (row 2).
pub fn open_layer_at_row(path: &Path, shape: u32, row: u64) -> Result<BufReader<File>> {
    let mut file = File::open(path).map_err(|e| FormatError::io(path, e))?;
    let offset = row * u64::from(shape) * 4;
    file.seek(SeekFrom::Start(offset))
        .map_err(|e| FormatError::io(path, e))?;
    Ok(BufReader::with_capacity(CHUNK, file))
}
