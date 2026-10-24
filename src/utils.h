// utils.h

#ifndef CHESS_H
#define CHESS_H

#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include "Board.h"

class Profile;

// utility functions

uint64_t perft(const Board& board, int depth);

template<class T>
void write_buffer(int fildes, const T *buffer, size_t elements);

// Write or read exactly bytes at offset, whatever the file position. Short
// transfers continue where they stopped, each call moving at most
// bytes_per_call_max bytes (INT_MAX by default, the write man page's limit
// on nbyte). Any error throws, as does a write that makes no progress or a
// read that reaches end of file first.
void pwrite_bytes(int fildes, const void *buffer, size_t bytes, size_t offset, size_t bytes_per_call_max = INT_MAX);
void pread_bytes(int fildes, void *buffer, size_t bytes, size_t offset, size_t bytes_per_call_max = INT_MAX);

// Writes to one file from many threads at once, with no two writes in
// flight sharing a block of block_bytes. APFS on macOS 26 loses bytes when
// they do: while one pwrite that ends partway into an unwritten block is
// running, another writing the rest of that block can return the full count
// and later read back as zeros. pwrite writes the whole blocks of its range
// with pwrite_bytes in the calling thread, then the partial blocks at either
// end one write at a time under this file's lock, keeping each partial
// piece's offset, length and SHA-256 digest. verify reads back the partial
// pieces, where every lost byte found so far lay, and throws naming the
// first whose bytes differ.
//
// Once anything is written, each object must end in verify or abandon, or
// be destroyed by an exception unwinding past it; otherwise its destructor
// aborts.
class SharedFileWrites
{
public:

  static constexpr size_t block_bytes = 4096;

  // Opens path for reading and writing, with open_flags added (O_CREAT |
  // O_TRUNC for a new file), and sets its length to size if given. verify
  // and abandon close it, as does the destructor if neither ran.
  SharedFileWrites(std::string path_in, int open_flags, std::optional<size_t> size = std::nullopt);

  struct OpenPerWrite
  {
  };
  static constexpr OpenPerWrite open_per_write{};

  // Opens path for each write (creating it) and for verify, for callers
  // that cannot hold a descriptor per file.
  SharedFileWrites(std::string path_in, OpenPerWrite);

  ~SharedFileWrites();

  SharedFileWrites(const SharedFileWrites&) = delete;
  SharedFileWrites& operator=(const SharedFileWrites&) = delete;

  // Writes bytes at offset, like pwrite_bytes. Ranges written from
  // different threads must not overlap.
  void pwrite(const void *buffer, size_t bytes, size_t offset);

  // Reads back every partial piece, then closes the file, checking the
  // close. Call once, after every pwrite has returned. Also throws if two
  // pieces overlap. Returns the pieces checked.
  size_t verify();

  // Ends this object without verifying, for files being removed. Closes
  // the file without checking the close.
  void abandon();

private:

  struct Piece
  {
    size_t offset;
    size_t bytes;
    std::array<unsigned char, 32> digest;
  };

  bool per_write;
  // the open descriptor, or -1
  int fildes;
  std::string name;
  int uncaught_at_construction;
  std::mutex mutex;
  std::vector<Piece> pieces;
  bool written;
  bool finished;

  void check_block_size(const char *what, int statvfs_ret, unsigned long block) const;
  void close_checked();
  void pwrite_bytes_to_file(const void *buffer, size_t bytes, size_t offset);
};

// Write count elements of element_bytes bytes each, element i at offset +
// i * element_bytes, with fill(begin, n, out) writing elements [begin,
// begin + n) to out. The elements pass through one buffer of whole
// elements, at most buffer_bytes_max bytes or one element. Each buffer is
// filled by calls on disjoint pieces of it in parallel, then written with
// pwrite_bytes. An exception from fill is thrown once the buffer's calls
// have returned, before its write. With profile, filling and writing tic
// "chunk populate" and "chunk write".
void pwrite_filled(int fildes, size_t offset, size_t count, size_t element_bytes,
                   const std::function<void(size_t, size_t, uint8_t *)>& fill,
                   Profile *profile = nullptr,
                   size_t buffer_bytes_max = size_t(1) << 30);

#endif
