// build_chess_database.cpp

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>

#include "ChessDFAParams.h"
#include "ChessGame.h"
#include "DFA.h"
#include "DFAUtil.h"

void print_stuff(const ChessGame& game, std::string name, shared_dfa_ptr positions)
{
  std::cout << "############################################################" << std::endl;
  std::cout << name << ": " << DFAUtil::quick_stats(positions) << std::endl;

  for(auto iter = positions->cbegin();
      iter < positions->cend();
      ++iter)
    {
      std::cout << game.position_to_string(*iter) << std::endl;
      break;
    }  
  std::cout << "############################################################" << std::endl;
}

shared_dfa_ptr build_legal(const ChessGame& game, int side_to_move, int pieces_max)
{
  std::ostringstream name_builder;
  name_builder << "legal_positions,side_to_move=" << std::to_string(side_to_move);
  name_builder << ",pieces_max=" << std::to_string(pieces_max);
  
  return game.load_or_build(name_builder.str(), [&]()
  {
    std::vector<shared_dfa_ptr> conditions;
    conditions.push_back(game.get_positions_legal(side_to_move));
    conditions.push_back(DFAUtil::get_count_character(game.get_shape(),
						      DFA_BLANK,
						      64 - pieces_max, 64,
						      CHESS_SQUARE_OFFSET));

    return DFAUtil::get_intersection_vector(game.get_shape(), conditions);
  });
}

shared_dfa_ptr build_database(const ChessGame& game, int pieces_max, int ply_max)
{
  assert(pieces_max >= 2);
  assert(ply_max >= 0);

  int side_to_move = ply_max % 2;

  std::ostringstream name_builder;
  if(ply_max == 0)
    {
      name_builder << game.get_name_lost(side_to_move);
    }
  else if(ply_max % 2)
    {
      name_builder << game.get_name_winning(side_to_move, ply_max);
    }
  else
    {
      name_builder << game.get_name_losing(side_to_move, ply_max);
    }
  name_builder << ",pieces_max=" << std::to_string(pieces_max);

  std::string name = name_builder.str();

  return game.load_or_build(name, [&]()
  {
    // build new database

    std::cout << "building " << name << std::endl;

    shared_dfa_ptr legal = build_legal(game, side_to_move, pieces_max);

    if(ply_max == 0)
      {
	// lost: in check and cannot move

	shared_dfa_ptr check = game.get_positions_check(side_to_move);
	print_stuff(game, "check", check);
	shared_dfa_ptr check_legal = DFAUtil::get_intersection(check, legal);
	print_stuff(game, "check_legal", check_legal);

	shared_dfa_ptr post_move = game.get_moves_forward(side_to_move, check_legal);
	shared_dfa_ptr pre_post_move = game.get_moves_backward(side_to_move, post_move);
	shared_dfa_ptr output = DFAUtil::get_difference(check_legal, pre_post_move);
	print_stuff(game, "output", output);

	return output;
      }
    else if(ply_max % 2)
      {
	// winning

	shared_dfa_ptr losing = build_database(game, pieces_max, ply_max - 1);
	shared_dfa_ptr winning = game.get_moves_backward(side_to_move, losing);
	// join to legal to reapply pieces_max constraint
	return DFAUtil::get_intersection(winning, legal);
      }
    else
      {
	// losing

	throw std::logic_error("losing not implemented");
      }

    return shared_dfa_ptr(0);
  });
}

int main(int argc, char **argv)
{
  if(argc != 3)
    {
      std::cerr << "usage: build_chess_database PIECES_MAX PLY_MAX" << std::endl;
      return 1;
    }

  int pieces_max = atoi(argv[1]);
  if(pieces_max < 2)
    {
      std::cerr << "PIECES_MAX must be at least two." << std::endl;
      return 1;
    }
  
  int ply_max = atoi(argv[2]);
  if(ply_max < 0)
    {
      std::cerr << "PLY_MAX must be non-negative." << std::endl;
      return 1;
    }

  const ChessGame game;
  assert(game.get_shape().size() == 64 + CHESS_SQUARE_OFFSET);

  shared_dfa_ptr database = build_database(game, pieces_max, ply_max);
  assert(database);
  std::cout << "database has " << database->states() << " states." << std::endl;
  std::cout << "database has " << database->size() << " positions." << std::endl;

  return 0;
}
