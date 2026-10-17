// util.cpp

#include <algorithm>
#include <bit>
#include <cassert>
#include <cerrno>
#include <climits>
#include <format>
#include <stdexcept>
#include <system_error>
#include <unistd.h>

#include "utils.h"

std::string mask_to_square(BoardMask mask)
{
  // map mask to a square name. assumes only one bit is set in the
  // mask.

  assert(std::popcount(mask) == 1);

  int index = std::countr_zero(mask);
  assert(index < 64);

  int rank = index / 8;
  int file = index % 8;

  std::string output;
  output += char('a' + file);
  output += char('1' + (7 - rank));

  return output;
}

uint64_t perft(const Board& board, int depth)
{
  if(depth <= 0)
    return 1;

  Board moves[CHESS_MAX_MOVES];
  int num_moves = board.generate_moves(moves);
  if(depth == 1)
    return num_moves;

  uint64_t output = 0;
  for(uint64_t i = 0; i < num_moves; ++i)
    {
      output += perft(moves[i], depth - 1);
    }
  return output;
}

std::string uci_move(const Board& before, const Board& after)
{
  // approximate UCI move string. will fail for castling and promotion.

  BoardMask flipped = before.pieces_by_side[before.side_to_move] ^ after.pieces_by_side[before.side_to_move];
  assert(flipped);
  BoardMask from = flipped & before.pieces_by_side[before.side_to_move];
  assert(from);
  BoardMask to = flipped & after.pieces_by_side[before.side_to_move];
  assert(to);

  if(std::popcount(from) > 1)
    {
      // must be castling
      assert(from & before.pieces_by_side_type[before.side_to_move][PIECE_KING]);
      from &= before.pieces_by_side_type[before.side_to_move][PIECE_KING];

      to &= after.pieces_by_side_type[before.side_to_move][PIECE_KING];
      assert(to);
    }

  assert(std::popcount(from) == 1);
  assert(std::popcount(to) == 1);

  std::string promotion;
  if(from & before.pieces_by_side_type[before.side_to_move][PIECE_PAWN])
    {
      // pawn moved
      if(!(to & after.pieces_by_side_type[before.side_to_move][PIECE_PAWN]))
	{
	  // no longer a pawn

	  if(to & after.pieces_by_side_type[before.side_to_move][PIECE_QUEEN])
	    {
	      promotion = "q";
	    }
	  else if(to & after.pieces_by_side_type[before.side_to_move][PIECE_ROOK])
	    {
	      promotion = "r";
	    }
	  else if(to & after.pieces_by_side_type[before.side_to_move][PIECE_KNIGHT])
	    {
	      promotion = "n";
	    }
	  else if(to & after.pieces_by_side_type[before.side_to_move][PIECE_BISHOP])
	    {
	      promotion = "b";
	    }
	}
    }

  return mask_to_square(from) + mask_to_square(to) + promotion;
}

template<class T>
void write_buffer(int fildes, const T *buffer, size_t elements)
{
  const char *buffer_bytes = reinterpret_cast<const char *>(buffer);
  size_t bytes_wanted = elements * sizeof(T);
  size_t bytes_written = 0;
  while(bytes_written < bytes_wanted)
    {
      // write man page says nbyte parameter limited to INT_MAX
      size_t bytes_next = std::min(bytes_wanted - bytes_written, size_t(INT_MAX));

      ssize_t ret = write(fildes, buffer_bytes + bytes_written, bytes_next);
      if(ret < 0)
        {
          perror("write");
          throw std::runtime_error("write() failed");
        }
      assert(ret > 0);
      assert(ret <= bytes_wanted - bytes_written);

      bytes_written += ret;
    }

  assert(bytes_written == bytes_wanted);
}

void pwrite_bytes(int fildes, const void *buffer, size_t bytes, size_t offset, size_t bytes_per_call_max)
{
  if(bytes_per_call_max == 0)
    {
      throw std::invalid_argument("pwrite_bytes: bytes_per_call_max must be positive");
    }

  const char *buffer_bytes = static_cast<const char *>(buffer);
  size_t bytes_written = 0;
  while(bytes_written < bytes)
    {
      size_t bytes_next = std::min({bytes - bytes_written, bytes_per_call_max, size_t(INT_MAX)});
      ssize_t ret = pwrite(fildes, buffer_bytes + bytes_written, bytes_next, off_t(offset + bytes_written));
      if(ret < 0)
        {
          throw std::system_error(errno, std::generic_category(), std::format("pwrite of {:d} bytes at offset {:d} failed", bytes_next, offset + bytes_written));
        }
      if(ret == 0)
        {
          throw std::runtime_error(std::format("pwrite of {:d} bytes at offset {:d} wrote nothing", bytes_next, offset + bytes_written));
        }
      bytes_written += size_t(ret);
    }
}

void pread_bytes(int fildes, void *buffer, size_t bytes, size_t offset, size_t bytes_per_call_max)
{
  if(bytes_per_call_max == 0)
    {
      throw std::invalid_argument("pread_bytes: bytes_per_call_max must be positive");
    }

  char *buffer_bytes = static_cast<char *>(buffer);
  size_t bytes_read = 0;
  while(bytes_read < bytes)
    {
      size_t bytes_next = std::min({bytes - bytes_read, bytes_per_call_max, size_t(INT_MAX)});
      ssize_t ret = pread(fildes, buffer_bytes + bytes_read, bytes_next, off_t(offset + bytes_read));
      if(ret < 0)
        {
          throw std::system_error(errno, std::generic_category(), std::format("pread of {:d} bytes at offset {:d} failed", bytes_next, offset + bytes_read));
        }
      if(ret == 0)
        {
          throw std::runtime_error(std::format("pread of {:d} bytes at offset {:d} reached end of file after {:d} of {:d} bytes", bytes_next, offset + bytes_read, bytes_read, bytes));
        }
      bytes_read += size_t(ret);
    }
}

// template instantiations

#include "BinaryDFA.h"
#include "flash_sort.h"

#define INSTANTIATE(T) template void write_buffer(int fildes, const T *buffer, size_t elements);

INSTANTIATE(BinaryDFATransitionsHashPlusIndex<false>);
INSTANTIATE(BinaryDFATransitionsHashPlusIndex<true>);
INSTANTIATE(double);
INSTANTIATE(int);
INSTANTIATE(long long unsigned int);
INSTANTIATE(long unsigned int);
INSTANTIATE(unsigned char);
INSTANTIATE(unsigned int);
INSTANTIATE(dfa_state_pair_t);
INSTANTIATE(dfa_state_pair_wide_t);

// A comma inside the macro argument would split it, so name the types.
typedef dfa_pair_output<dfa_state_pair_t, uint32_t> dfa_pair_output_narrow_narrow_t;
typedef dfa_pair_output<dfa_state_pair_t, uint64_t> dfa_pair_output_narrow_wide_t;
typedef dfa_pair_output<dfa_state_pair_wide_t, uint32_t> dfa_pair_output_wide_narrow_t;
typedef dfa_pair_output<dfa_state_pair_wide_t, uint64_t> dfa_pair_output_wide_wide_t;
INSTANTIATE(dfa_pair_output_narrow_narrow_t);
INSTANTIATE(dfa_pair_output_narrow_wide_t);
INSTANTIATE(dfa_pair_output_wide_narrow_t);
INSTANTIATE(dfa_pair_output_wide_wide_t);

// BinaryDFA's row sort records, as instantiated for MemoryMap.
#define INSTANTIATE_ROW_RANK_RECORDS(id_t, rank_t)			\
  template void write_buffer(int, const RowRankRecord<id_t, 1, rank_t> *, size_t); \
  template void write_buffer(int, const RowRankRecord<id_t, 2, rank_t> *, size_t); \
  template void write_buffer(int, const RowRankRecord<id_t, 3, rank_t> *, size_t); \
  template void write_buffer(int, const RowRankRecord<id_t, 4, rank_t> *, size_t);

INSTANTIATE_ROW_RANK_RECORDS(uint32_t, uint32_t)
INSTANTIATE_ROW_RANK_RECORDS(uint32_t, uint64_t)
INSTANTIATE_ROW_RANK_RECORDS(uint64_t, uint32_t)
INSTANTIATE_ROW_RANK_RECORDS(uint64_t, uint64_t)
