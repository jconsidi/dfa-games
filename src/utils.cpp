// util.cpp

#include <algorithm>
#include <bit>
#include <cassert>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <exception>
#include <format>
#include <iostream>
#include <stdexcept>
#include <system_error>
#include <unistd.h>
#include <vector>

#include <fcntl.h>
#include <openssl/evp.h>
#include <sys/statvfs.h>

#include "Profile.h"
#include "parallel.h"
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

static std::array<unsigned char, 32> sha256_digest(const void *buffer, size_t bytes)
{
  std::array<unsigned char, 32> digest;
  static const EVP_MD *hash_implementation = EVP_sha256();
  EVP_MD_CTX *hash_context = EVP_MD_CTX_create();
  unsigned int digest_bytes = 0;
  bool ok = (hash_context != nullptr) &&
    (EVP_DigestInit_ex(hash_context, hash_implementation, NULL) == 1) &&
    (EVP_DigestUpdate(hash_context, buffer, bytes) == 1) &&
    (EVP_DigestFinal_ex(hash_context, digest.data(), &digest_bytes) == 1);
  EVP_MD_CTX_destroy(hash_context);
  if(!ok || (digest_bytes != digest.size()))
    {
      throw std::runtime_error("SHA-256 digest failed");
    }
  return digest;
}

void SharedFileWrites::check_block_size(const char *what, int statvfs_ret, unsigned long block) const
{
  if(statvfs_ret != 0)
    {
      throw std::system_error(errno, std::generic_category(), std::format("SharedFileWrites: {:s} {:s}", what, name));
    }
  if((block == 0) || (block_bytes % block != 0))
    {
      throw std::runtime_error(std::format("SharedFileWrites: {:s} is on a file system with {:d} byte blocks, which do not divide {:d}", name, block, block_bytes));
    }
}

SharedFileWrites::SharedFileWrites(std::string path_in, int open_flags, std::optional<size_t> size)
  : per_write(false),
    fildes(-1),
    name(path_in),
    uncaught_at_construction(std::uncaught_exceptions()),
    written(false),
    finished(false)
{
  fildes = ::open(name.c_str(), O_RDWR | open_flags, 0600);
  if(fildes < 0)
    {
      throw std::system_error(errno, std::generic_category(), "SharedFileWrites: open " + name);
    }
  try
    {
      struct statvfs file_system;
      int ret = ::fstatvfs(fildes, &file_system);
      check_block_size("fstatvfs", ret, ret == 0 ? file_system.f_frsize : 0);
      if(size && (::ftruncate(fildes, off_t(*size)) != 0))
        {
          throw std::system_error(errno, std::generic_category(), std::format("SharedFileWrites: ftruncate {:s} to {:d} bytes", name, *size));
        }
    }
  catch(...)
    {
      ::close(fildes);
      throw;
    }
}

SharedFileWrites::SharedFileWrites(std::string path_in, OpenPerWrite)
  : per_write(true),
    fildes(-1),
    name(path_in),
    uncaught_at_construction(std::uncaught_exceptions()),
    written(false),
    finished(false)
{
  size_t slash = name.find_last_of('/');
  std::string dir = (slash == std::string::npos) ? "." : ((slash == 0) ? "/" : name.substr(0, slash));
  struct statvfs file_system;
  int ret = ::statvfs(dir.c_str(), &file_system);
  check_block_size("statvfs", ret, ret == 0 ? file_system.f_frsize : 0);
}

SharedFileWrites::~SharedFileWrites()
{
  if(written && !finished && (std::uncaught_exceptions() <= uncaught_at_construction))
    {
      std::cerr << "SharedFileWrites: " << name << " was written but never verified or abandoned" << std::endl;
      std::abort();
    }
  if(fildes >= 0)
    {
      ::close(fildes);
    }
}

void SharedFileWrites::close_checked()
{
  int fd = fildes;
  fildes = -1;
  if(::close(fd) != 0)
    {
      throw std::system_error(errno, std::generic_category(), "SharedFileWrites: close " + name);
    }
}

void SharedFileWrites::pwrite_bytes_to_file(const void *buffer, size_t bytes, size_t offset)
{
  if(!per_write)
    {
      pwrite_bytes(fildes, buffer, bytes, offset);
      return;
    }
  int fd = ::open(name.c_str(), O_WRONLY | O_CREAT, 0600);
  if(fd < 0)
    {
      throw std::system_error(errno, std::generic_category(), "SharedFileWrites: open " + name);
    }
  try
    {
      pwrite_bytes(fd, buffer, bytes, offset);
    }
  catch(...)
    {
      ::close(fd);
      throw;
    }
  if(::close(fd) != 0)
    {
      throw std::system_error(errno, std::generic_category(), "SharedFileWrites: close " + name);
    }
}

void SharedFileWrites::pwrite(const void *buffer, size_t bytes, size_t offset)
{
  {
    std::lock_guard<std::mutex> lock(mutex);
    if(finished)
      {
        throw std::logic_error("SharedFileWrites: " + name + " written after verify or abandon");
      }
    written = true;
  }
  if(bytes == 0)
    {
      return;
    }

  const unsigned char *data = static_cast<const unsigned char *>(buffer);
  size_t end = offset + bytes;
  size_t whole_begin = (offset + block_bytes - 1) / block_bytes * block_bytes;
  size_t whole_end = end / block_bytes * block_bytes;

  // [begin, end) pieces of partial blocks
  std::vector<std::pair<size_t, size_t>> partial;
  if(whole_begin < whole_end)
    {
      pwrite_bytes_to_file(data + (whole_begin - offset), whole_end - whole_begin, whole_begin);
      if(offset < whole_begin)
        {
          partial.emplace_back(offset, whole_begin);
        }
      if(whole_end < end)
        {
          partial.emplace_back(whole_end, end);
        }
    }
  else
    {
      partial.emplace_back(offset, end);
    }

  std::vector<Piece> written_pieces;
  for(const auto& [piece_begin, piece_end] : partial)
    {
      written_pieces.push_back(Piece{piece_begin, piece_end - piece_begin, sha256_digest(data + (piece_begin - offset), piece_end - piece_begin)});
    }

  std::lock_guard<std::mutex> lock(mutex);
  for(const Piece& piece : written_pieces)
    {
      pwrite_bytes_to_file(data + (piece.offset - offset), piece.bytes, piece.offset);
      pieces.push_back(piece);
    }
}

size_t SharedFileWrites::verify()
{
  std::lock_guard<std::mutex> lock(mutex);
  if(finished)
    {
      throw std::logic_error("SharedFileWrites: " + name + " verified after verify or abandon");
    }
  finished = true;

  int fd = fildes;
  if(per_write && !pieces.empty())
    {
      fd = ::open(name.c_str(), O_RDONLY);
      if(fd < 0)
        {
          throw std::system_error(errno, std::generic_category(), "SharedFileWrites: open " + name);
        }
    }

  auto check = [&]()
  {
    std::sort(pieces.begin(), pieces.end(), [](const Piece& a, const Piece& b) {return a.offset < b.offset;});
    std::vector<unsigned char> back;
    for(size_t i = 0; i < pieces.size(); ++i)
      {
        const Piece& piece = pieces[i];
        if((i > 0) && (pieces[i - 1].offset + pieces[i - 1].bytes > piece.offset))
          {
            throw std::logic_error(std::format("SharedFileWrites: {:s} bytes [{:d}, {:d}) and [{:d}, {:d}) were both written", name, pieces[i - 1].offset, pieces[i - 1].offset + pieces[i - 1].bytes, piece.offset, piece.offset + piece.bytes));
          }
        back.resize(piece.bytes);
        pread_bytes(fd, back.data(), piece.bytes, piece.offset);
        if(sha256_digest(back.data(), back.size()) != piece.digest)
          {
            throw std::runtime_error(std::format("SharedFileWrites: {:s} bytes [{:d}, {:d}) read back differ from what was written", name, piece.offset, piece.offset + piece.bytes));
          }
      }
  };

  if(!per_write)
    {
      check();
      close_checked();
      return pieces.size();
    }
  if(fd < 0)
    {
      return 0;
    }
  try
    {
      check();
    }
  catch(...)
    {
      ::close(fd);
      throw;
    }
  if(::close(fd) != 0)
    {
      throw std::system_error(errno, std::generic_category(), "SharedFileWrites: close " + name);
    }
  return pieces.size();
}

void SharedFileWrites::abandon()
{
  std::lock_guard<std::mutex> lock(mutex);
  finished = true;
  if(fildes >= 0)
    {
      ::close(fildes);
      fildes = -1;
    }
}

void pwrite_filled(int fildes, size_t offset, size_t count, size_t element_bytes,
                   const std::function<void(size_t, size_t, uint8_t *)>& fill,
                   Profile *profile, size_t buffer_bytes_max)
{
  if(element_bytes == 0)
    {
      throw std::invalid_argument("pwrite_filled: element_bytes must be positive");
    }

  const size_t buffer_elements = std::min(count, std::max(buffer_bytes_max / element_bytes, size_t(1)));
  // Pieces of about 1MB, so a buffer splits into enough pieces to balance
  // across threads.
  const size_t piece_elements = std::max((size_t(1) << 20) / element_bytes, size_t(1));

  std::vector<uint8_t> buffer(buffer_elements * element_bytes);
  for(size_t buffer_start = 0; buffer_start < count; buffer_start += buffer_elements)
    {
      size_t buffer_count = std::min(buffer_elements, count - buffer_start);

      if(profile)
        {
          profile->tic("chunk populate");
        }
      size_t pieces = (buffer_count + piece_elements - 1) / piece_elements;
      parallel_for(pieces, [&](size_t k)
      {
        size_t piece_start = k * piece_elements;
        size_t piece_count = std::min(piece_elements, buffer_count - piece_start);
        fill(buffer_start + piece_start, piece_count, buffer.data() + piece_start * element_bytes);
      });

      if(profile)
        {
          profile->tic("chunk write");
        }
      pwrite_bytes(fildes, buffer.data(), buffer_count * element_bytes, offset + buffer_start * element_bytes);
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
