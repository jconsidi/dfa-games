//! Byte-layout arithmetic for version 1 of the DFA file format.
//!
//! This module is the single authority on entry widths, block sizes and block
//! offsets.  Both the writer and the reader go through it, so the two cannot
//! drift apart: if this file says layer 7 starts at offset 4128, that is where
//! the writer puts it and where the reader looks for it.

use crate::error::{FormatError, Result};

/// "DFA1" followed by CR LF SUB LF, so a text-mode transfer corrupts it
/// visibly rather than subtly (spec 3.1).
pub const MAGIC: [u8; 8] = [0x44, 0x46, 0x41, 0x31, 0x0D, 0x0A, 0x1A, 0x0A];

pub const VERSION_MAJOR: u16 = 1;
pub const VERSION_MINOR: u16 = 0;
pub const HEADER_BYTES: u32 = 64;

pub const OFF_MAGIC: u64 = 0;
pub const OFF_VERSION_MAJOR: u64 = 8;
pub const OFF_VERSION_MINOR: u64 = 10;
pub const OFF_HEADER_BYTES: u64 = 12;
pub const OFF_DIGEST: u64 = 16;
pub const OFF_NDIM: u64 = 48;
pub const OFF_FLAGS: u64 = 52;
pub const OFF_INITIAL_STATE: u64 = 56;
pub const OFF_TABLES: u64 = 64;

pub const DIGEST_LEN: usize = 32;

/// The digest covers every byte from here to EOF, i.e. everything after the
/// digest field itself (spec 3.1).
pub const DIGEST_COVERAGE_START: u64 = 48;

/// `flags` bit 0 asserts canonical state numbering (spec 8).
pub const FLAG_CANONICAL: u32 = 1;
/// Every other flag bit is reserved and must be zero.
pub const FLAGS_RESERVED_MASK: u32 = !FLAG_CANONICAL;

/// Index 0 rejects everything continuing from it (spec 4).
pub const STATE_REJECT: u32 = 0;
/// Index 1 accepts everything continuing from it (spec 4).
pub const STATE_ACCEPT: u32 = 1;

/// The terminal pseudo-layer `ndim` has exactly the two reserved states and no
/// transition block (spec 3.2).
pub const TERMINAL_LAYER_SIZE: u64 = 2;

/// Round up to the next multiple of 8.
pub fn align8(x: u64) -> Result<u64> {
    x.checked_add(7)
        .map(|v| v & !7)
        .ok_or_else(|| FormatError::Overflow(format!("align8({x})")))
}

/// Smallest width in {1, 2, 4, 8} with `256**width >= next_layer_size`.
///
/// Computed by comparison rather than by evaluating `256u64.pow(width)`, which
/// overflows at width 8.
pub fn width_for(next_layer_size: u64) -> u8 {
    if next_layer_size <= 1 << 8 {
        1
    } else if next_layer_size <= 1 << 16 {
        2
    } else if next_layer_size <= 1 << 32 {
        4
    } else {
        8
    }
}

fn mul(a: u64, b: u64, what: &str) -> Result<u64> {
    a.checked_mul(b)
        .ok_or_else(|| FormatError::Overflow(format!("{what}: {a} * {b}")))
}

fn add(a: u64, b: u64, what: &str) -> Result<u64> {
    a.checked_add(b)
        .ok_or_else(|| FormatError::Overflow(format!("{what}: {a} + {b}")))
}

/// Everything about where the bytes of one file go.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Layout {
    shape: Vec<u32>,
    layer_size: Vec<u64>,
    width: Vec<u8>,
    block_bytes: Vec<u64>,
    layer_offset: Vec<u64>,
    tables_end: u64,
    file_len: u64,
}

impl Layout {
    /// Derive the full layout from the two tables that determine it.
    ///
    /// Rejects the shapes the format cannot express, so that everything below
    /// may assume `ndim >= 1`, `shape[i] >= 1` and `layer_size[i] >= 2`.
    pub fn new(shape: Vec<u32>, layer_size: Vec<u64>) -> Result<Layout> {
        let ndim = shape.len();
        if ndim == 0 {
            return Err(FormatError::Other("ndim must be at least 1".into()));
        }
        if layer_size.len() != ndim {
            return Err(FormatError::Other(format!(
                "shape has {ndim} entries but layer_size has {}",
                layer_size.len()
            )));
        }
        for (i, &s) in shape.iter().enumerate() {
            if s < 1 {
                return Err(FormatError::Other(format!("shape[{i}] is 0, must be >= 1")));
            }
        }
        for (i, &n) in layer_size.iter().enumerate() {
            if n < 2 {
                return Err(FormatError::Other(format!(
                    "layer_size[{i}] is {n}, must be >= 2"
                )));
            }
        }

        let ndim_u64 = u64::try_from(ndim)
            .map_err(|_| FormatError::Overflow(format!("ndim {ndim} does not fit in u64")))?;

        // header + layer_size[] + layer_offset[] + shape[] = 64 + 20 * ndim
        let tables_end = add(
            OFF_TABLES,
            mul(20, ndim_u64, "table bytes")?,
            "end of tables",
        )?;

        let mut width = Vec::with_capacity(ndim);
        let mut block_bytes = Vec::with_capacity(ndim);
        let mut layer_offset = Vec::with_capacity(ndim);

        let mut offset = align8(tables_end)?;
        for i in 0..ndim {
            let next = if i + 1 < ndim {
                layer_size[i + 1]
            } else {
                TERMINAL_LAYER_SIZE
            };
            let w = width_for(next);
            let entries = mul(layer_size[i], u64::from(shape[i]), "block entries")?;
            let bytes = mul(entries, u64::from(w), "block bytes")?;

            layer_offset.push(offset);
            width.push(w);
            block_bytes.push(bytes);

            let end = add(offset, bytes, "block end")?;
            offset = if i + 1 < ndim { align8(end)? } else { end };
        }

        Ok(Layout {
            shape,
            layer_size,
            width,
            block_bytes,
            layer_offset,
            tables_end,
            file_len: offset,
        })
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

    pub fn width(&self) -> &[u8] {
        &self.width
    }

    pub fn block_bytes(&self) -> &[u64] {
        &self.block_bytes
    }

    pub fn layer_offset(&self) -> &[u64] {
        &self.layer_offset
    }

    /// First byte after the shape table, before alignment padding.
    pub fn tables_end(&self) -> u64 {
        self.tables_end
    }

    pub fn file_len(&self) -> u64 {
        self.file_len
    }

    /// Number of states in layer `i`, treating `i == ndim` as the terminal
    /// pseudo-layer.  Every stored entry in layer `i` must be less than
    /// `next_layer_size(i)`.
    pub fn next_layer_size(&self, layer: usize) -> u64 {
        if layer + 1 < self.ndim() {
            self.layer_size[layer + 1]
        } else {
            TERMINAL_LAYER_SIZE
        }
    }

    /// Bytes occupied by one row of layer `layer`.
    pub fn row_bytes(&self, layer: usize) -> u64 {
        u64::from(self.shape[layer]) * u64::from(self.width[layer])
    }

    /// Absolute byte offset of row `row` of layer `layer`.
    pub fn row_offset(&self, layer: usize, row: u64) -> u64 {
        self.layer_offset[layer] + row * self.row_bytes(layer)
    }

    /// Absolute byte offset of entry `c` of row `row` of layer `layer`.
    pub fn entry_offset(&self, layer: usize, row: u64, c: u32) -> u64 {
        self.layer_offset[layer]
            + (row * u64::from(self.shape[layer]) + u64::from(c)) * u64::from(self.width[layer])
    }

    /// Total number of states across all layers, reserved states included.
    pub fn total_states(&self) -> u64 {
        self.layer_size.iter().sum()
    }
}

/// Decode one entry of `width` bytes, little-endian.
pub fn decode_entry(bytes: &[u8], width: u8) -> u64 {
    let mut value = 0u64;
    for (i, &b) in bytes.iter().take(usize::from(width)).enumerate() {
        value |= u64::from(b) << (8 * i);
    }
    value
}

/// Append one entry of `width` bytes, little-endian.
pub fn encode_entry(value: u64, width: u8, out: &mut Vec<u8>) {
    let bytes = value.to_le_bytes();
    out.extend_from_slice(&bytes[..usize::from(width)]);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn align8_rounds_up() {
        assert_eq!(align8(0).unwrap(), 0);
        assert_eq!(align8(1).unwrap(), 8);
        assert_eq!(align8(8).unwrap(), 8);
        assert_eq!(align8(9).unwrap(), 16);
        assert!(align8(u64::MAX).is_err());
    }

    #[test]
    fn width_boundaries_follow_the_spec() {
        // 256 ** width >= layer_size[i + 1], so 256 states still fit in 1 byte.
        assert_eq!(width_for(2), 1);
        assert_eq!(width_for(256), 1);
        assert_eq!(width_for(257), 2);
        assert_eq!(width_for(65536), 2);
        assert_eq!(width_for(65537), 4);
        assert_eq!(width_for(1 << 32), 4);
        assert_eq!(width_for((1 << 32) + 1), 8);
    }

    #[test]
    fn last_layer_is_always_one_byte_wide() {
        let layout = Layout::new(vec![3; 5], vec![1000; 5]).unwrap();
        assert_eq!(*layout.width().last().unwrap(), 1);
    }

    #[test]
    fn odd_ndim_pads_after_the_shape_table() {
        // 64 + 20 * 3 = 124, which is not a multiple of 8.
        let layout = Layout::new(vec![2, 2, 2], vec![2, 2, 2]).unwrap();
        assert_eq!(layout.tables_end(), 124);
        assert_eq!(layout.layer_offset()[0], 128);
    }

    #[test]
    fn even_ndim_needs_no_padding() {
        // 64 + 20 * 2 = 104, already aligned.
        let layout = Layout::new(vec![2, 2], vec![2, 2]).unwrap();
        assert_eq!(layout.tables_end(), 104);
        assert_eq!(layout.layer_offset()[0], 104);
    }

    #[test]
    fn blocks_are_padded_to_eight_but_the_file_is_not() {
        // layer 0: 2 states * 3 chars * 1 byte = 6 bytes, padded to 8.
        let layout = Layout::new(vec![3, 3], vec![2, 2]).unwrap();
        assert_eq!(layout.block_bytes(), &[6, 6]);
        assert_eq!(layout.layer_offset()[0], 104);
        assert_eq!(layout.layer_offset()[1], 112);
        // No trailing padding: the file ends at the end of the last block.
        assert_eq!(layout.file_len(), 118);
    }

    #[test]
    fn mixed_widths_in_one_file() {
        // widths come from the *next* layer's size.
        let layout = Layout::new(vec![70000, 2, 2, 2], vec![3, 70000, 300, 5]).unwrap();
        assert_eq!(layout.width(), &[4, 2, 1, 1]);
    }

    #[test]
    fn entry_offsets_are_row_major() {
        let layout = Layout::new(vec![3, 3], vec![4, 2]).unwrap();
        let base = layout.layer_offset()[0];
        assert_eq!(layout.entry_offset(0, 0, 0), base);
        assert_eq!(layout.entry_offset(0, 0, 2), base + 2);
        assert_eq!(layout.entry_offset(0, 1, 0), base + 3);
        assert_eq!(layout.row_offset(0, 3), base + 9);
    }

    #[test]
    fn degenerate_shapes_are_rejected() {
        assert!(Layout::new(vec![], vec![]).is_err());
        assert!(Layout::new(vec![0], vec![2]).is_err());
        assert!(Layout::new(vec![3], vec![1]).is_err());
        assert!(Layout::new(vec![3, 3], vec![2]).is_err());
    }

    #[test]
    fn entry_codec_round_trips() {
        for width in [1u8, 2, 4, 8] {
            let value = match width {
                1 => 0xABu64,
                2 => 0xBEEF,
                4 => 0xDEAD_BEEF,
                _ => 0x0123_4567_89AB_CDEF,
            };
            let mut buf = Vec::new();
            encode_entry(value, width, &mut buf);
            assert_eq!(buf.len(), usize::from(width));
            assert_eq!(decode_entry(&buf, width), value);
        }
    }
}
