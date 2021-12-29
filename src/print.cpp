// print.cpp

#include <iostream>

#include "chess.h"

int main(int argc, char **argv)
{
  const char *fen = (argc > 1) ? argv[1] : INITIAL_FEN;
  Board board(fen);
  std::cout << board;

  return 0;
}
