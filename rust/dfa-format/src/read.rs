//! Reading and validating a `.dfa` file.
//!
//! `dfa-validate` runs [`validate`] over a published file; `dfa-convert
//! --verify` runs the same function over its temp file before publishing it.
//! There is deliberately no second, weaker opinion about what conformance
//! means.

use std::fs::File;
use std::path::{Path, PathBuf};

use memmap2::Mmap;
use sha2::{Digest, Sha256};

use crate::bitset::Bitset;
use crate::error::{FormatError, Result, Violation};
use crate::hex;
use crate::layout::{self, Layout};

/// Which of the optional checks from spec section 7 to run.  All default to
/// on: they cost one sequential pass and catch the encoding faults that would
/// otherwise leave a structurally valid file denoting the wrong set.
#[derive(Debug, Clone, Copy)]
pub struct ValidateOptions {
    pub digest: bool,
    pub reserved_rows: bool,
    pub entry_bounds: bool,
    /// Verify canonical numbering and minimality when `flags` bit 0 is set.
    pub canonical: bool,
    /// Verify that a file stored as `<digest>.dfa` is named after its own
    /// digest.  Not part of the format, which says nothing about file names,
    /// but content addressing is only worth anything if the name is true.
    pub filename: bool,
}

impl Default for ValidateOptions {
    fn default() -> Self {
        ValidateOptions {
            digest: true,
            reserved_rows: true,
            entry_bounds: true,
            canonical: true,
            filename: true,
        }
    }
}

impl ValidateOptions {
    /// Only the checks a reader *must* perform before using a file.
    pub fn required_only() -> Self {
        ValidateOptions {
            digest: false,
            reserved_rows: false,
            entry_bounds: false,
            canonical: false,
            filename: false,
        }
    }
}

#[derive(Debug, Clone)]
pub struct Header {
    pub version_major: u16,
    pub version_minor: u16,
    pub header_bytes: u32,
    pub digest: [u8; layout::DIGEST_LEN],
    pub ndim: u32,
    pub flags: u32,
    pub initial_state: u64,
}

impl Header {
    pub fn canonical(&self) -> bool {
        self.flags & layout::FLAG_CANONICAL != 0
    }
}

#[derive(Debug, Clone)]
pub struct Report {
    pub path: PathBuf,
    pub file_len: u64,
    pub header: Option<Header>,
    pub layout: Option<Layout>,
    pub violations: Vec<Violation>,
}

impl Report {
    pub fn ok(&self) -> bool {
        self.violations.is_empty()
    }
}

/// An opened, structurally valid file.
pub struct Dfa {
    map: Mmap,
    header: Header,
    layout: Layout,
}

impl Dfa {
    /// Open a file after running every *required* check.  Optional checks are
    /// left to [`validate`], which is what `dfa-validate` calls.
    pub fn open(path: &Path) -> Result<Dfa> {
        let (map, file_len) = map_file(path)?;
        let mut violations = Vec::new();
        let parsed = parse(&map, file_len, &mut violations);
        match parsed {
            Some((header, layout)) if violations.is_empty() => Ok(Dfa {
                map,
                header,
                layout,
            }),
            _ => {
                let message = violations
                    .first()
                    .map(|v| v.to_string())
                    .unwrap_or_else(|| "unparseable".to_string());
                Err(FormatError::invalid(path, message))
            }
        }
    }

    pub fn header(&self) -> &Header {
        &self.header
    }

    pub fn layout(&self) -> &Layout {
        &self.layout
    }

    /// Entry `c` of row `row` in layer `layer`.
    pub fn entry(&self, layer: usize, row: u64, c: u32) -> u64 {
        let width = self.layout.width()[layer];
        let offset = self.layout.entry_offset(layer, row, c) as usize;
        layout::decode_entry(&self.map[offset..offset + usize::from(width)], width)
    }

    /// Spec section 5.  The early returns on states 0 and 1 are the operative
    /// definition, not an optimization: a reader that instead walks all the
    /// way to the terminal pseudo-layer implements a different rule and
    /// diverges on malformed files.
    pub fn accepts(&self, s: &[u32]) -> Result<bool> {
        if s.len() != self.layout.ndim() {
            return Err(FormatError::Other(format!(
                "string has {} characters, expected {}",
                s.len(),
                self.layout.ndim()
            )));
        }
        for (i, &c) in s.iter().enumerate() {
            if c >= self.layout.shape()[i] {
                return Err(FormatError::Other(format!(
                    "character {c} at index {i} is outside the alphabet of size {}",
                    self.layout.shape()[i]
                )));
            }
        }

        let mut state = self.header.initial_state;
        for (i, &c) in s.iter().enumerate() {
            if state == u64::from(layout::STATE_REJECT) {
                return Ok(false);
            }
            if state == u64::from(layout::STATE_ACCEPT) {
                return Ok(true);
            }
            state = self.entry(i, state, c);
        }
        Ok(state == u64::from(layout::STATE_ACCEPT))
    }
}

/// Run every check selected by `opts`, collecting all failures.
pub fn validate(path: &Path, opts: &ValidateOptions) -> Result<Report> {
    let (map, file_len) = map_file(path)?;
    let mut violations = Vec::new();
    let parsed = parse(&map, file_len, &mut violations);

    let (header, lay) = match parsed {
        Some(v) => v,
        None => {
            return Ok(Report {
                path: path.to_path_buf(),
                file_len,
                header: None,
                layout: None,
                violations,
            })
        }
    };

    // The block scans below index into the mapping, so only run them once the
    // file is known to be long enough to hold every block.
    let blocks_present = file_len >= lay.file_len();

    if opts.digest {
        check_digest(&map, &header, &mut violations);
    }
    if opts.filename {
        check_filename(path, &header, &mut violations);
    }
    if blocks_present {
        check_padding(&map, &lay, &mut violations);
        if opts.reserved_rows || opts.entry_bounds || (opts.canonical && header.canonical()) {
            scan_blocks(&map, &header, &lay, opts, &mut violations);
        }
        if opts.canonical && header.canonical() {
            check_reachability(&map, &header, &lay, &mut violations);
        }
    }

    Ok(Report {
        path: path.to_path_buf(),
        file_len,
        header: Some(header),
        layout: Some(lay),
        violations,
    })
}

fn map_file(path: &Path) -> Result<(Mmap, u64)> {
    let file = File::open(path).map_err(|e| FormatError::io(path, e))?;
    let file_len = file.metadata().map_err(|e| FormatError::io(path, e))?.len();
    // SAFETY: a `.dfa` file is immutable once named (spec 10), so the mapping
    // cannot be changed underneath us by a conforming writer.  The converter
    // writes to a temp name and publishes with link(2), never in place.
    let map = unsafe { Mmap::map(&file) }.map_err(|e| FormatError::io(path, e))?;
    Ok((map, file_len))
}

fn u16_at(m: &[u8], off: u64) -> u16 {
    let i = off as usize;
    u16::from_le_bytes([m[i], m[i + 1]])
}

fn u32_at(m: &[u8], off: u64) -> u32 {
    let i = off as usize;
    u32::from_le_bytes([m[i], m[i + 1], m[i + 2], m[i + 3]])
}

fn u64_at(m: &[u8], off: u64) -> u64 {
    let i = off as usize;
    u64::from_le_bytes([
        m[i],
        m[i + 1],
        m[i + 2],
        m[i + 3],
        m[i + 4],
        m[i + 5],
        m[i + 6],
        m[i + 7],
    ])
}

/// All the required checks of spec section 7.  Returns `None` when the file is
/// damaged badly enough that no further checking is meaningful.
fn parse(m: &[u8], file_len: u64, out: &mut Vec<Violation>) -> Option<(Header, Layout)> {
    if file_len < u64::from(layout::HEADER_BYTES) {
        out.push(Violation::new(format!(
            "file is {file_len} bytes, shorter than the {} byte header",
            layout::HEADER_BYTES
        )));
        return None;
    }

    if m[..8] != layout::MAGIC {
        out.push(Violation::at(
            layout::OFF_MAGIC,
            format!("magic is {:02x?}, expected {:02x?}", &m[..8], layout::MAGIC),
        ));
        return None;
    }

    let version_major = u16_at(m, layout::OFF_VERSION_MAJOR);
    let version_minor = u16_at(m, layout::OFF_VERSION_MINOR);
    if version_major != layout::VERSION_MAJOR {
        out.push(Violation::at(
            layout::OFF_VERSION_MAJOR,
            format!(
                "version_major is {version_major}, this reader implements {}",
                layout::VERSION_MAJOR
            ),
        ));
        return None;
    }

    let header_bytes = u32_at(m, layout::OFF_HEADER_BYTES);
    if header_bytes != layout::HEADER_BYTES {
        // Rather than guess at a header we do not understand, refuse it.
        out.push(Violation::at(
            layout::OFF_HEADER_BYTES,
            format!(
                "header_bytes is {header_bytes}, this reader understands only {}",
                layout::HEADER_BYTES
            ),
        ));
        return None;
    }

    let mut digest = [0u8; layout::DIGEST_LEN];
    digest.copy_from_slice(&m[layout::OFF_DIGEST as usize..][..layout::DIGEST_LEN]);

    let ndim = u32_at(m, layout::OFF_NDIM);
    let flags = u32_at(m, layout::OFF_FLAGS);
    let initial_state = u64_at(m, layout::OFF_INITIAL_STATE);

    let header = Header {
        version_major,
        version_minor,
        header_bytes,
        digest,
        ndim,
        flags,
        initial_state,
    };

    if flags & layout::FLAGS_RESERVED_MASK != 0 {
        out.push(Violation::at(
            layout::OFF_FLAGS,
            format!("reserved flag bits set: flags = 0x{flags:08x}"),
        ));
    }

    if ndim == 0 {
        out.push(Violation::at(layout::OFF_NDIM, "ndim is 0, must be >= 1"));
        return None;
    }
    let ndim_usize = ndim as usize;

    let tables_end = layout::OFF_TABLES + 20 * u64::from(ndim);
    if file_len < tables_end {
        out.push(Violation::new(format!(
            "file is {file_len} bytes, too short for the {tables_end} bytes of header and tables \
             implied by ndim = {ndim}"
        )));
        return None;
    }

    let size_base = layout::OFF_TABLES;
    let offset_base = size_base + 8 * u64::from(ndim);
    let shape_base = offset_base + 8 * u64::from(ndim);

    let layer_size: Vec<u64> = (0..ndim_usize)
        .map(|i| u64_at(m, size_base + 8 * i as u64))
        .collect();
    let stored_offsets: Vec<u64> = (0..ndim_usize)
        .map(|i| u64_at(m, offset_base + 8 * i as u64))
        .collect();
    let shape: Vec<u32> = (0..ndim_usize)
        .map(|i| u32_at(m, shape_base + 4 * i as u64))
        .collect();

    let mut fatal = false;
    for (i, &s) in shape.iter().enumerate() {
        if s == 0 {
            out.push(Violation::at(
                shape_base + 4 * i as u64,
                format!("shape[{i}] is 0, must be >= 1"),
            ));
            fatal = true;
        }
    }
    for (i, &n) in layer_size.iter().enumerate() {
        if n < 2 {
            out.push(Violation::at(
                size_base + 8 * i as u64,
                format!("layer_size[{i}] is {n}, must be >= 2"),
            ));
            fatal = true;
        }
    }
    if fatal {
        return None;
    }

    let lay = match Layout::new(shape, layer_size.clone()) {
        Ok(lay) => lay,
        Err(e) => {
            out.push(Violation::new(format!("cannot derive layout: {e}")));
            return None;
        }
    };

    for (i, (&stored, &derived)) in stored_offsets.iter().zip(lay.layer_offset()).enumerate() {
        if stored != derived {
            out.push(Violation::at(
                offset_base + 8 * i as u64,
                format!("layer_offset[{i}] is {stored}, but the layout implies {derived}"),
            ));
        }
    }

    if file_len != lay.file_len() {
        out.push(Violation::new(format!(
            "file is {file_len} bytes, but the layout implies {}",
            lay.file_len()
        )));
    }

    if initial_state >= layer_size[0] {
        out.push(Violation::at(
            layout::OFF_INITIAL_STATE,
            format!(
                "initial_state is {initial_state}, but layer 0 has only {} states",
                layer_size[0]
            ),
        ));
    }

    Some((header, lay))
}

fn check_digest(m: &[u8], header: &Header, out: &mut Vec<Violation>) {
    let mut hasher = Sha256::new();
    hasher.update(&m[layout::DIGEST_COVERAGE_START as usize..]);
    let actual: [u8; layout::DIGEST_LEN] = hasher.finalize().into();
    if actual != header.digest {
        out.push(Violation::at(
            layout::OFF_DIGEST,
            format!(
                "digest is {}, but the contents hash to {}",
                hex(&header.digest),
                hex(&actual)
            ),
        ));
    }
}

/// A file stored as `<digest>.dfa` is making a claim about its own contents,
/// so check it.  Files named any other way -- a temp file mid-conversion, a
/// copy someone renamed -- make no such claim and are left alone.
///
/// This compares the name against the digest *field*.  Whether that field
/// matches the bytes is the separate business of `check_digest`, and a file
/// that fails both reports both, which is what tells you which half lied.
fn check_filename(path: &Path, header: &Header, out: &mut Vec<Violation>) {
    if path.extension().and_then(|e| e.to_str()) != Some("dfa") {
        return;
    }
    let Some(stem) = path.file_stem().and_then(|s| s.to_str()) else {
        return;
    };
    if !crate::is_hash(stem) {
        return;
    }

    let digest = hex(&header.digest);
    if stem != digest {
        out.push(Violation::at(
            layout::OFF_DIGEST,
            format!("file is named {stem}.dfa but its digest is {digest}"),
        ));
    }
}

/// Padding between the tables and block 0, and between blocks, must be zero.
fn check_padding(m: &[u8], lay: &Layout, out: &mut Vec<Violation>) {
    let mut check = |from: u64, to: u64, what: &str| {
        if to <= from {
            return;
        }
        let slice = &m[from as usize..to as usize];
        if let Some(i) = slice.iter().position(|&b| b != 0) {
            out.push(Violation::at(
                from + i as u64,
                format!("padding {what} is not zero"),
            ));
        }
    };

    check(lay.tables_end(), lay.layer_offset()[0], "before block 0");
    for i in 0..lay.ndim() - 1 {
        let end = lay.layer_offset()[i] + lay.block_bytes()[i];
        check(end, lay.layer_offset()[i + 1], &format!("after block {i}"));
    }
}

/// One sequential pass per block, doing every per-entry check that was asked
/// for.  This is the pass spec section 7 singles out: it detects index
/// truncation and similar faults that leave the file structurally valid but
/// denoting the wrong set.
fn scan_blocks(
    m: &[u8],
    header: &Header,
    lay: &Layout,
    opts: &ValidateOptions,
    out: &mut Vec<Violation>,
) {
    let check_canonical = opts.canonical && header.canonical();

    for layer in 0..lay.ndim() {
        let width = lay.width()[layer];
        let shape = lay.shape()[layer];
        let bound = lay.next_layer_size(layer);
        let layer_size = lay.layer_size()[layer];
        let row_bytes = lay.row_bytes(layer) as usize;
        let base = lay.layer_offset()[layer] as usize;

        let mut prev: Vec<u64> = Vec::new();
        let mut values: Vec<u64> = vec![0; shape as usize];
        let mut reported_bounds = false;
        let mut reported_order = false;
        let mut reported_uniform = false;

        for row in 0..layer_size {
            let start = base + (row as usize) * row_bytes;
            let bytes = &m[start..start + row_bytes];
            for (c, chunk) in bytes.chunks_exact(usize::from(width)).enumerate() {
                values[c] = layout::decode_entry(chunk, width);
            }

            if opts.entry_bounds && !reported_bounds {
                if let Some((c, &v)) = values.iter().enumerate().find(|(_, &v)| v >= bound) {
                    out.push(Violation::at(
                        (start + c * usize::from(width)) as u64,
                        format!(
                            "layer {layer} row {row} entry {c} is {v}, but layer {} has only \
                             {bound} states",
                            layer + 1
                        ),
                    ));
                    reported_bounds = true;
                }
            }

            if opts.reserved_rows && row < 2 {
                let required = row;
                if let Some((c, &v)) = values.iter().enumerate().find(|(_, &v)| v != required) {
                    out.push(Violation::at(
                        (start + c * usize::from(width)) as u64,
                        format!(
                            "layer {layer} reserved row {row} entry {c} is {v}, expected {required}"
                        ),
                    ));
                }
            }

            if check_canonical && row >= 2 {
                if row >= 3 && !reported_order && prev.as_slice() >= values.as_slice() {
                    out.push(Violation::at(
                        start as u64,
                        format!(
                            "flags bit 0 is set, but layer {layer} row {row} does not sort after \
                             row {}",
                            row - 1
                        ),
                    ));
                    reported_order = true;
                }
                // Spec 8: canonical numbering also asserts minimality, and a
                // uniform ordinary row simply repeats a reserved state.
                if !reported_uniform {
                    if values.iter().all(|&v| v == u64::from(layout::STATE_REJECT)) {
                        out.push(Violation::at(
                            start as u64,
                            format!(
                                "flags bit 0 is set, but layer {layer} row {row} rejects \
                                 everything, duplicating state 0"
                            ),
                        ));
                        reported_uniform = true;
                    } else if values.iter().all(|&v| v == u64::from(layout::STATE_ACCEPT)) {
                        out.push(Violation::at(
                            start as u64,
                            format!(
                                "flags bit 0 is set, but layer {layer} row {row} accepts \
                                 everything, duplicating state 1"
                            ),
                        ));
                        reported_uniform = true;
                    }
                }
                prev.clear();
                prev.extend_from_slice(&values);
            }
        }
    }
}

/// Forward pass marking which ordinary states can actually be entered.  Only
/// run when `flags` bit 0 is set, since that is what asserts minimality.
fn check_reachability(m: &[u8], header: &Header, lay: &Layout, out: &mut Vec<Violation>) {
    let mut current = Bitset::new(lay.layer_size()[0]);
    current.set(header.initial_state);

    for layer in 0..lay.ndim() {
        let width = lay.width()[layer];
        let shape = lay.shape()[layer];
        let layer_size = lay.layer_size()[layer];
        let row_bytes = lay.row_bytes(layer) as usize;
        let base = lay.layer_offset()[layer] as usize;

        if let Some(row) = (2..layer_size).find(|&row| !current.get(row)) {
            out.push(Violation::at(
                lay.row_offset(layer, row),
                format!("flags bit 0 is set, but layer {layer} state {row} is unreachable"),
            ));
        }

        let mut next = Bitset::new(lay.next_layer_size(layer));
        for row in 2..layer_size {
            if !current.get(row) {
                continue;
            }
            let start = base + (row as usize) * row_bytes;
            let bytes = &m[start..start + row_bytes];
            for chunk in bytes.chunks_exact(usize::from(width)).take(shape as usize) {
                next.set(layout::decode_entry(chunk, width));
            }
        }
        current = next;
    }
}
