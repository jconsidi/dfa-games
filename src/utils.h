// utils.h

#ifndef CHESS_H
#define CHESS_H

#include <climits>
#include <cstddef>

#include "Board.h"

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

#endif
