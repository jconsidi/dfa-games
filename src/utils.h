// utils.h

#ifndef CHESS_H
#define CHESS_H

#include "Board.h"

// utility functions

uint64_t perft(const Board& board, int depth);

template<class T>
void write_buffer(int fildes, const T *buffer, size_t elements);

#endif
