// test_chess_reverse.cpp

#include "ChessGame.h"
#include "Profile.h"
#include "test_utils.h"

int main()
{
  Profile profile("test_chess_game");
  ChessGame chess;

  profile.tic("test_backward");
  test_backward(chess, 2, false);

  profile.tic("done");
  return 0;
}
