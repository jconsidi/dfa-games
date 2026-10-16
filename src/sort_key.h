// sort_key.h

#ifndef SORT_KEY_H
#define SORT_KEY_H

#include <bit>
#include <cassert>
#include <cstdint>

// A row of shape unsigned ids read as one key: the bit string of the ids
// concatenated most significant bit first, shape * 8 * sizeof(id_t) bits
// long. Keys compare as the rows read as numbers, id 0 first, so a record
// holding a row orders by its key the way it orders by the ids.
//
// FlashSort reads keys through these, by way of the records' own
// key_byte_at_bit and first_difference_bit.

// The 8 key bits starting at bit b, zero padded past the end of the key.
template <class id_t, int shape>
uint8_t row_key_byte_at_bit(const id_t *row, int b)
{
  static_assert(shape > 0);
  constexpr int id_bits = int(8 * sizeof(id_t));
  constexpr int key_bits = shape * id_bits;

  assert((0 <= b) && (b < key_bits));
  int t = b / id_bits;
  int available = id_bits - b % id_bits;
  uint64_t value = uint64_t(row[t]);
  if(available >= 8)
    {
      return uint8_t((value >> (available - 8)) & 0xFF);
    }

  uint64_t high = value & ((uint64_t(1) << available) - 1);
  uint64_t low = 0;
  if(t + 1 < shape)
    {
      low = uint64_t(row[t + 1]) >> (id_bits - (8 - available));
    }
  return uint8_t((high << (8 - available)) | low);
}

// First key bit at or after b where row and other differ, or the key's
// length if they agree from b on.
template <class id_t, int shape>
int row_first_difference_bit(const id_t *row, const id_t *other, int b)
{
  static_assert(shape > 0);
  constexpr int id_bits = int(8 * sizeof(id_t));
  constexpr int key_bits = shape * id_bits;

  assert((0 <= b) && (b <= key_bits));
  for(int t = b / id_bits; t < shape; ++t)
    {
      uint64_t difference = uint64_t(row[t] ^ other[t]);
      if(t == b / id_bits)
        {
          // only the bits from b on
          uint64_t id_mask = (id_bits == 64) ? ~uint64_t(0) : ((uint64_t(1) << id_bits) - 1);
          difference &= id_mask >> (b % id_bits);
        }
      if(difference != 0)
        {
          return t * id_bits + (std::countl_zero(difference) - (64 - id_bits));
        }
    }
  return key_bits;
}

#endif
