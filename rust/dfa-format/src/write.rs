//! Writer: an in-memory automaton in, one conforming `.dfa` file out.
//!
//! The output is still built through a fixed size buffer rather than assembled
//! whole, so the memory used does not scale with the file being written.

use std::fs::{self, File, OpenOptions};
use std::io::{BufWriter, ErrorKind, Read, Seek, SeekFrom, Write};
use std::path::{Path, PathBuf};

use sha2::{Digest, Sha256};

use crate::bitset::Bitset;
use crate::error::{FormatError, Result};
use crate::hex;
use crate::layout::{self, Layout};
use crate::read::{self, ValidateOptions};
use crate::Automaton;

const CHUNK: usize = 8 << 20;

/// Why the source does not qualify for `flags` bit 0, for the diagnostic.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CanonicalBreak {
    pub layer: usize,
    pub row: u64,
    pub reason: String,
}

impl std::fmt::Display for CanonicalBreak {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "layer {} row {} {}", self.layer, self.row, self.reason)
    }
}

/// Decides whether the source qualifies for `flags` bit 0.
///
/// Spec 8 ties canonical numbering to minimality -- the ordering is only well
/// defined if no two ordinary states in a layer share a row, and setting the
/// bit therefore also asserts that the automaton is free of unreachable and
/// dead states.  All of it is checked here, in the same pass that copies the
/// rows, so the converter never makes a claim `dfa-validate` would reject:
///
/// * rows `2..layer_size[i]` strictly ascending as tuples of next-state
///   indices,
/// * every ordinary state reachable from `initial_state`,
/// * no ordinary row that merely repeats a reserved state.
///
/// Rows arrive in order, layer by layer, so reachability needs only the bitset
/// for the current layer and the one being filled for the next.
struct CanonicalTracker {
    broke: Option<CanonicalBreak>,
    current: Bitset,
    next: Bitset,
    prev_row: Vec<u64>,
    have_prev: bool,
}

impl CanonicalTracker {
    fn new(lay: &Layout, initial_state: u32) -> CanonicalTracker {
        let mut current = Bitset::new(lay.layer_size()[0]);
        current.set(u64::from(initial_state));
        CanonicalTracker {
            broke: None,
            current,
            next: Bitset::new(0),
            prev_row: Vec::new(),
            have_prev: false,
        }
    }

    /// False once the answer is known, so the remaining rows skip the work.
    fn active(&self) -> bool {
        self.broke.is_none()
    }

    fn begin_layer(&mut self, lay: &Layout, layer: usize) {
        self.next = Bitset::new(lay.next_layer_size(layer));
        self.prev_row.clear();
        self.have_prev = false;
    }

    fn row(&mut self, layer: usize, row: u64, values: &[u64]) {
        if !self.active() || row < 2 {
            return;
        }

        if !self.current.get(row) {
            self.fail(layer, row, "is unreachable".to_string());
            return;
        }
        if self.have_prev && self.prev_row.as_slice() >= values {
            self.fail(layer, row, format!("does not sort after row {}", row - 1));
            return;
        }
        if values.iter().all(|&v| v == u64::from(layout::STATE_REJECT)) {
            self.fail(
                layer,
                row,
                "rejects everything, duplicating state 0".to_string(),
            );
            return;
        }
        if values.iter().all(|&v| v == u64::from(layout::STATE_ACCEPT)) {
            self.fail(
                layer,
                row,
                "accepts everything, duplicating state 1".to_string(),
            );
            return;
        }

        for &v in values {
            self.next.set(v);
        }
        self.prev_row.clear();
        self.prev_row.extend_from_slice(values);
        self.have_prev = true;
    }

    fn end_layer(&mut self) {
        self.current = std::mem::replace(&mut self.next, Bitset::new(0));
    }

    fn fail(&mut self, layer: usize, row: u64, reason: String) {
        self.broke = Some(CanonicalBreak { layer, row, reason });
    }

    fn verdict(self) -> Option<CanonicalBreak> {
        self.broke
    }
}

#[derive(Debug, Clone)]
pub struct Converted {
    pub path: PathBuf,
    pub digest: String,
    pub canonical: bool,
    pub canonical_break: Option<CanonicalBreak>,
    /// True when a file of that name was already present, so nothing was
    /// written.  Files are immutable once named (spec 10).
    pub already_existed: bool,
}

/// Write one automaton to `out_dir/<digest>.dfa`.
pub fn write_automaton(src: &Automaton, out_dir: &Path, verify: bool) -> Result<Converted> {
    let layer_size: Vec<u64> = (0..src.ndim()).map(|l| src.layer_size(l)).collect();
    let lay = Layout::new(src.shape().to_vec(), layer_size)?;

    fs::create_dir_all(out_dir).map_err(|e| FormatError::io(out_dir, e))?;
    let tmp = temp_path(out_dir);

    // Anything that fails from here on leaves a temp file behind, so clean up
    // on the way out rather than littering the store.
    let outcome = write_and_publish(src, &lay, &tmp, out_dir, verify);
    if outcome.is_err() {
        let _ = fs::remove_file(&tmp);
    }
    outcome
}

fn write_and_publish(
    src: &Automaton,
    lay: &Layout,
    tmp: &Path,
    out_dir: &Path,
    verify: bool,
) -> Result<Converted> {
    let file = OpenOptions::new()
        .read(true)
        .write(true)
        .create_new(true)
        .open(tmp)
        .map_err(|e| FormatError::io(tmp, e))?;

    let mut writer = BufWriter::with_capacity(CHUNK, file);
    write_header_and_tables(&mut writer, lay, src.initial_state())?;

    let mut tracker = CanonicalTracker::new(lay, src.initial_state());
    for layer in 0..lay.ndim() {
        write_layer(&mut writer, src, lay, layer, &mut tracker)?;
    }
    let canonical_break = tracker.verdict();
    let canonical = canonical_break.is_none();

    writer.flush().map_err(|e| FormatError::io(tmp, e))?;
    let mut file = writer
        .into_inner()
        .map_err(|e| FormatError::io(tmp, e.into_error()))?;

    // `flags` lives at offset 52, inside the digest's coverage of [48, EOF),
    // so it has to be settled before the digest is computed.
    if canonical {
        file.seek(SeekFrom::Start(layout::OFF_FLAGS))
            .map_err(|e| FormatError::io(tmp, e))?;
        file.write_all(&layout::FLAG_CANONICAL.to_le_bytes())
            .map_err(|e| FormatError::io(tmp, e))?;
    }

    let digest = digest_file(&mut file, tmp)?;
    file.seek(SeekFrom::Start(layout::OFF_DIGEST))
        .map_err(|e| FormatError::io(tmp, e))?;
    file.write_all(&digest)
        .map_err(|e| FormatError::io(tmp, e))?;
    file.sync_all().map_err(|e| FormatError::io(tmp, e))?;
    drop(file);

    if verify {
        let report = read::validate(tmp, &ValidateOptions::default())?;
        if !report.violations.is_empty() {
            let detail = report
                .violations
                .iter()
                .map(|v| v.to_string())
                .collect::<Vec<_>>()
                .join("; ");
            return Err(FormatError::invalid(
                tmp,
                format!("converted file failed validation: {detail}"),
            ));
        }
    }

    let digest_hex = hex(&digest);
    let final_path = out_dir.join(format!("{digest_hex}.dfa"));

    // link(2) fails atomically if the name is taken, which is exactly the
    // "skip, do not unlink and replace" rule of spec 10.  fs::rename would
    // silently clobber a file another reader may hold open.
    let already_existed = match fs::hard_link(tmp, &final_path) {
        Ok(()) => false,
        Err(e) if e.kind() == ErrorKind::AlreadyExists => true,
        Err(e) => return Err(FormatError::io(&final_path, e)),
    };
    fs::remove_file(tmp).map_err(|e| FormatError::io(tmp, e))?;

    // Make the new name durable, not just the bytes behind it.
    if let Ok(dir) = File::open(out_dir) {
        let _ = dir.sync_all();
    }

    Ok(Converted {
        path: final_path,
        digest: digest_hex,
        canonical,
        canonical_break,
        already_existed,
    })
}

fn write_header_and_tables(
    writer: &mut BufWriter<File>,
    lay: &Layout,
    initial_state: u32,
) -> Result<()> {
    let ndim = u32::try_from(lay.ndim())
        .map_err(|_| FormatError::Overflow(format!("ndim {} exceeds u32", lay.ndim())))?;

    let mut header = Vec::with_capacity(64);
    header.extend_from_slice(&layout::MAGIC);
    header.extend_from_slice(&layout::VERSION_MAJOR.to_le_bytes());
    header.extend_from_slice(&layout::VERSION_MINOR.to_le_bytes());
    header.extend_from_slice(&layout::HEADER_BYTES.to_le_bytes());
    header.extend_from_slice(&[0u8; layout::DIGEST_LEN]); // filled in at the end
    header.extend_from_slice(&ndim.to_le_bytes());
    header.extend_from_slice(&0u32.to_le_bytes()); // flags, settled at the end
    header.extend_from_slice(&u64::from(initial_state).to_le_bytes());
    debug_assert_eq!(header.len(), 64);
    writer.write_all(&header)?;

    let mut tables = Vec::new();
    for &n in lay.layer_size() {
        tables.extend_from_slice(&n.to_le_bytes());
    }
    for &o in lay.layer_offset() {
        tables.extend_from_slice(&o.to_le_bytes());
    }
    for &s in lay.shape() {
        tables.extend_from_slice(&s.to_le_bytes());
    }
    writer.write_all(&tables)?;

    let pad = lay.layer_offset()[0] - lay.tables_end();
    write_zeros(writer, pad)?;
    Ok(())
}

/// Copy one layer, re-encoding entries to the derived width, feeding each row
/// to the canonical/minimality tracker on the way past.
fn write_layer(
    writer: &mut BufWriter<File>,
    src: &Automaton,
    lay: &Layout,
    layer: usize,
    tracker: &mut CanonicalTracker,
) -> Result<()> {
    let shape = usize::try_from(lay.shape()[layer])
        .map_err(|_| FormatError::Overflow(format!("shape[{layer}] exceeds usize")))?;
    let width = lay.width()[layer];
    let bound = lay.next_layer_size(layer);
    let layer_size = lay.layer_size()[layer];

    let row_bytes = shape * usize::from(width);
    let rows_per_chunk = std::cmp::max(1, CHUNK / row_bytes);
    let mut out_buf: Vec<u8> = Vec::with_capacity(rows_per_chunk * row_bytes);
    let mut values: Vec<u64> = vec![0; shape];

    tracker.begin_layer(lay, layer);

    for index in 0..layer_size {
        let row = src.row(layer, index);

        // The layout was derived from `shape`, so a row of any other length
        // would be encoded into a block sized for something else.  `Automaton`
        // asserts this on the way in; the writer does not take that on trust,
        // because it is the one property every byte offset depends on.
        if row.len() != shape {
            return Err(FormatError::bad_source(format!(
                "layer={layer} row {index} has {} entries, but shape[{layer}] is {shape}",
                row.len()
            )));
        }
        for (c, &v) in row.iter().enumerate() {
            values[c] = u64::from(v);
        }

        check_row(layer, index, &values, bound)?;
        tracker.row(layer, index, &values);

        for &v in &values {
            layout::encode_entry(v, width, &mut out_buf);
        }

        if out_buf.len() >= CHUNK {
            writer.write_all(&out_buf)?;
            out_buf.clear();
        }
    }
    writer.write_all(&out_buf)?;

    // Blocks start on an 8 byte boundary; the last one is followed by EOF.
    let end = lay.layer_offset()[layer] + lay.block_bytes()[layer];
    let pad = if layer + 1 < lay.ndim() {
        lay.layer_offset()[layer + 1] - end
    } else {
        0
    };
    write_zeros(writer, pad)?;

    tracker.end_layer();
    Ok(())
}

fn check_row(layer: usize, index: u64, values: &[u64], bound: u64) -> Result<()> {
    for (c, &v) in values.iter().enumerate() {
        if v >= bound {
            return Err(FormatError::bad_source(format!(
                "layer={layer} row {index} entry {c} is {v}, \
                 but the next layer has only {bound} states"
            )));
        }
    }

    // Spec 4: these two rows carry fixed values, and a writer that stores
    // anything else produces a malformed file.
    let required = match index {
        0 => Some(u64::from(layout::STATE_REJECT)),
        1 => Some(u64::from(layout::STATE_ACCEPT)),
        _ => None,
    };
    if let Some(required) = required {
        if let Some((c, &v)) = values.iter().enumerate().find(|(_, &v)| v != required) {
            return Err(FormatError::bad_source(format!(
                "layer={layer} reserved row {index} entry {c} is {v}, expected {required}"
            )));
        }
    }
    Ok(())
}

fn write_zeros(writer: &mut BufWriter<File>, mut count: u64) -> Result<()> {
    const ZEROS: [u8; 64] = [0u8; 64];
    while count > 0 {
        let n = std::cmp::min(count, ZEROS.len() as u64) as usize;
        writer.write_all(&ZEROS[..n])?;
        count -= n as u64;
    }
    Ok(())
}

/// SHA-256 over bytes [48, EOF), read back from the file just written.
fn digest_file(file: &mut File, path: &Path) -> Result<[u8; layout::DIGEST_LEN]> {
    file.seek(SeekFrom::Start(layout::DIGEST_COVERAGE_START))
        .map_err(|e| FormatError::io(path, e))?;
    let mut hasher = Sha256::new();
    let mut buf = vec![0u8; CHUNK];
    loop {
        let n = file.read(&mut buf).map_err(|e| FormatError::io(path, e))?;
        if n == 0 {
            break;
        }
        hasher.update(&buf[..n]);
    }
    Ok(hasher.finalize().into())
}

fn temp_path(out_dir: &Path) -> PathBuf {
    use std::sync::atomic::{AtomicU64, Ordering};
    static COUNTER: AtomicU64 = AtomicU64::new(0);
    let n = COUNTER.fetch_add(1, Ordering::Relaxed);
    out_dir.join(format!(".tmp-{}-{}.dfa", std::process::id(), n))
}
