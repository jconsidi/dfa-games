//! Streaming writer: legacy directory in, one conforming `.dfa` file out.
//!
//! Nothing is ever loaded whole.  The largest DFA in the existing store is
//! about 28 GB, so every layer is copied through a fixed size buffer.

use std::fs::{self, File, OpenOptions};
use std::io::{BufWriter, ErrorKind, Read, Seek, SeekFrom, Write};
use std::path::{Path, PathBuf};

use sha2::{Digest, Sha256};

use crate::error::{FormatError, Result};
use crate::hex;
use crate::layout::{self, Layout};
use crate::legacy::LegacyDfa;
use crate::read::{self, ValidateOptions};

const CHUNK: usize = 8 << 20;

/// Where the canonical ordering first broke, for the diagnostic.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CanonicalBreak {
    pub layer: usize,
    pub row: u64,
}

impl std::fmt::Display for CanonicalBreak {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "layer {} row {} does not sort after row {}",
            self.layer,
            self.row,
            self.row - 1
        )
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

/// Convert one legacy directory into `out_dir/<digest>.dfa`.
pub fn convert(src: &LegacyDfa, out_dir: &Path, verify: bool) -> Result<Converted> {
    let lay = Layout::new(src.shape().to_vec(), src.layer_size().to_vec())?;

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
    src: &LegacyDfa,
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

    let mut canonical = true;
    let mut canonical_break = None;
    for layer in 0..lay.ndim() {
        let broke = write_layer(&mut writer, src, lay, layer, canonical)?;
        if canonical {
            if let Some(row) = broke {
                canonical = false;
                canonical_break = Some(CanonicalBreak { layer, row });
            }
        }
    }

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

/// Copy one layer, re-encoding entries to the derived width.
///
/// Returns `Some(row)` when the canonical ordering first fails in this layer.
/// `check_canonical` is false once some earlier layer has already broken it,
/// so the comparison work stops as soon as the answer is known.
fn write_layer(
    writer: &mut BufWriter<File>,
    src: &LegacyDfa,
    lay: &Layout,
    layer: usize,
    check_canonical: bool,
) -> Result<Option<u64>> {
    let path = &src.layer_paths()[layer];
    let shape = usize::try_from(lay.shape()[layer])
        .map_err(|_| FormatError::Overflow(format!("shape[{layer}] exceeds usize")))?;
    let width = lay.width()[layer];
    let bound = lay.next_layer_size(layer);
    let layer_size = lay.layer_size()[layer];

    let src_row_bytes = shape * 4;
    let rows_per_chunk = std::cmp::max(1, CHUNK / src_row_bytes);
    let mut in_buf = vec![0u8; rows_per_chunk * src_row_bytes];
    let mut out_buf: Vec<u8> = Vec::with_capacity(rows_per_chunk * shape * usize::from(width));
    let mut values: Vec<u64> = vec![0; shape];
    let mut prev: Vec<u64> = Vec::new();

    let mut file = File::open(path).map_err(|e| FormatError::io(path, e))?;
    let mut broke_at = None;
    let mut row: u64 = 0;

    while row < layer_size {
        let rows_now = std::cmp::min(rows_per_chunk as u64, layer_size - row) as usize;
        let want = rows_now * src_row_bytes;
        file.read_exact(&mut in_buf[..want])
            .map_err(|e| FormatError::io(path, e))?;

        out_buf.clear();
        for r in 0..rows_now {
            let index = row + r as u64;
            let bytes = &in_buf[r * src_row_bytes..(r + 1) * src_row_bytes];
            for (c, w) in bytes.chunks_exact(4).enumerate() {
                values[c] = u64::from(u32::from_le_bytes([w[0], w[1], w[2], w[3]]));
            }

            check_row(src, layer, index, &values, bound)?;

            if check_canonical && broke_at.is_none() && index >= 2 {
                if index >= 3 && prev.as_slice() >= values.as_slice() {
                    broke_at = Some(index);
                } else {
                    prev.clear();
                    prev.extend_from_slice(&values);
                }
            }

            for &v in &values {
                layout::encode_entry(v, width, &mut out_buf);
            }
        }
        writer.write_all(&out_buf)?;
        row += rows_now as u64;
    }

    // Blocks start on an 8 byte boundary; the last one is followed by EOF.
    let end = lay.layer_offset()[layer] + lay.block_bytes()[layer];
    let pad = if layer + 1 < lay.ndim() {
        lay.layer_offset()[layer + 1] - end
    } else {
        0
    };
    write_zeros(writer, pad)?;

    Ok(broke_at)
}

fn check_row(src: &LegacyDfa, layer: usize, index: u64, values: &[u64], bound: u64) -> Result<()> {
    for (c, &v) in values.iter().enumerate() {
        if v >= bound {
            return Err(FormatError::bad_legacy(
                src.dir(),
                format!(
                    "layer={layer} row {index} entry {c} is {v}, \
                     but the next layer has only {bound} states"
                ),
            ));
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
            return Err(FormatError::bad_legacy(
                src.dir(),
                format!("layer={layer} reserved row {index} entry {c} is {v}, expected {required}"),
            ));
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
