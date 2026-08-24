// DFAFormat.h

// Byte layout of the single file DFA format. See FORMAT-DFA.md.
//
// This is the C++ counterpart of rust/dfa-format/src/layout.rs and must agree
// with it exactly: the two write files into the same content addressed store,
// so a disagreement about where a byte goes becomes a disagreement about the
// digest, and the same automaton would land under two names.

#ifndef DFA_FORMAT_H
#define DFA_FORMAT_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace dfa_format
{
  // "DFA1" followed by CR LF SUB LF, so a text mode transfer corrupts it
  // visibly rather than subtly.
  const uint8_t magic[8] = {0x44, 0x46, 0x41, 0x31, 0x0D, 0x0A, 0x1A, 0x0A};

  const uint16_t version_major = 1;
  const uint16_t version_minor = 0;
  const uint32_t header_bytes = 64;

  const size_t off_magic = 0;
  const size_t off_version_major = 8;
  const size_t off_version_minor = 10;
  const size_t off_header_bytes = 12;
  const size_t off_digest = 16;
  const size_t off_ndim = 48;
  const size_t off_flags = 52;
  const size_t off_initial_state = 56;
  const size_t off_tables = 64;

  const size_t digest_length = 32;

  // The digest covers every byte from here to EOF, which is everything after
  // the digest field itself. flags lives at 52, inside that range, so it has
  // to be settled before the digest can be computed.
  const size_t digest_coverage_start = 48;

  // flags bit 0 asserts canonical state numbering (spec section 8).
  const uint32_t flag_canonical = 1;

  // Index 0 rejects every continuation, index 1 accepts every continuation.
  const uint32_t state_reject = 0;
  const uint32_t state_accept = 1;

  // The terminal pseudo-layer has exactly the two reserved states and no
  // transition block.
  const uint64_t terminal_layer_size = 2;

  uint64_t align8(uint64_t);

  // Smallest width in {1, 2, 4, 8} with 256 ** width >= next_layer_size.
  // Computed by comparison rather than by evaluating the power, which
  // overflows at width 8.
  int width_for(uint64_t next_layer_size);

  // Where every byte of one file goes, derived from the two tables that
  // determine it. Throws if the described automaton is too large to address.
  class Layout
  {
    std::vector<int> shape;
    std::vector<uint64_t> layer_size;
    std::vector<int> width;
    std::vector<uint64_t> block_bytes;
    std::vector<uint64_t> layer_offset;
    uint64_t tables_end_value;
    uint64_t file_length;

  public:

    Layout(const std::vector<int>&, const std::vector<uint64_t>&);

    int get_ndim() const {return int(shape.size());}
    int get_shape(int layer) const {return shape.at(layer);}
    uint64_t get_layer_size(int layer) const {return layer_size.at(layer);}
    int get_width(int layer) const {return width.at(layer);}
    uint64_t get_block_bytes(int layer) const {return block_bytes.at(layer);}
    uint64_t get_layer_offset(int layer) const {return layer_offset.at(layer);}

    // First byte after the shape table, before alignment padding.
    uint64_t tables_end() const {return tables_end_value;}
    uint64_t file_len() const {return file_length;}

    // States in the next layer, treating ndim as the terminal pseudo-layer.
    // Every stored entry in this layer must be less than this.
    uint64_t next_layer_size(int layer) const;

    uint64_t row_bytes(int layer) const;
    uint64_t row_offset(int layer, uint64_t row) const;
  };

  // Decode and encode one entry of the given width, little endian.
  inline uint64_t decode_entry(const uint8_t *bytes, int width)
  {
    switch(width)
      {
      case 1:
	return bytes[0];
      case 2:
	return uint64_t(bytes[0]) | (uint64_t(bytes[1]) << 8);
      case 4:
	return (uint64_t(bytes[0]) |
		(uint64_t(bytes[1]) << 8) |
		(uint64_t(bytes[2]) << 16) |
		(uint64_t(bytes[3]) << 24));
      default:
	{
	  uint64_t value = 0;
	  for(int i = 0; i < width; ++i)
	    {
	      value |= uint64_t(bytes[i]) << (8 * i);
	    }
	  return value;
	}
      }
  }

  inline void encode_entry(uint64_t value, int width, uint8_t *bytes_out)
  {
    for(int i = 0; i < width; ++i)
      {
	bytes_out[i] = uint8_t((value >> (8 * i)) & 0xFF);
      }
  }
}

#endif
