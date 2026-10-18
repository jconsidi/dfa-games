// utils.h

#ifndef CHESS_H
#define CHESS_H

#include <climits>
#include <cstddef>
#include <cstdint>
#include <functional>

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
