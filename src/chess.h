// chess.h

#ifndef CHESS_H
#define CHESS_H

#include "Board.h"

#define INITIAL_FEN "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1"

// utility functions

uint64_t perft(const Board& board, int depth);

#endif
