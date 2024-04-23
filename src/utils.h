// utils.h

#ifndef CHESS_H
#define CHESS_H

#include <vector>

#include "Board.h"

// utility functions

const std::vector<size_t>& get_iota(size_t);
std::string get_temp_filename(size_t);

uint64_t perft(const Board& board, int depth);

template<class T>
void write_buffer(int fildes, const T *buffer, size_t elements);

#endif
