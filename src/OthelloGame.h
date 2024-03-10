// OthelloGame.h

#ifndef OTHELLO_GAME_H
#define OTHELLO_GAME_H

#include <string>

#include "Game.h"

class OthelloGame : public Game
{
private:

  int width;
  int height;

public:

  OthelloGame(int, int);

private:

  virtual MoveGraph build_move_graph(int) const;

  //static shared_dfa_ptr get_score_positions(int) const;

  std::string get_name_flip(int, int, int, int) const;
  std::string get_name_move(int, int) const;

  shared_dfa_ptr get_positions_can_flip(int, int, int, int, int) const;
  shared_dfa_ptr get_positions_can_place(int) const;
  shared_dfa_ptr get_positions_can_place(int, int, int) const;
  shared_dfa_ptr get_positions_end() const;
  shared_dfa_ptr get_positions_legal() const;

public:

  virtual shared_dfa_ptr build_positions_lost(int) const; // side to move has lost, no moves available
  virtual shared_dfa_ptr build_positions_won(int) const; // side to move has won, no moves available

  virtual DFAString get_position_initial() const;

  virtual std::string position_to_string(const DFAString&) const;
};

#endif
