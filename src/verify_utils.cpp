// verify_utils.cpp

#include "verify_utils.h"

#include <format>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "DFAUtil.h"

void verify_losing_position(const Game& game, int side_to_move, const DFAString& position, shared_dfa_ptr winning_prev, shared_dfa_ptr lost)
{
  // lost probably hits less often but will be better cached
  if(lost->contains(position))
    {
      // lost position
      return;
    }

  std::vector<DFAString> moves = game.validate_moves(side_to_move, position);
  for(const DFAString& move : moves)
    {
      if(!winning_prev->contains(move))
        {
          // found move that may not lose
          throw std::runtime_error("position is not lost and has moves to not known losing positions");
        }
    }
}

void verify_losing_sound(const Game& game, int side_to_move, shared_dfa_ptr losing_curr, shared_dfa_ptr winning_prev)
{
  std::cout << "VERIFYING " << losing_curr->size() << " LOSING POSITIONS" << std::endl;

  shared_dfa_ptr lost = game.get_positions_lost(side_to_move);
  
  uint64_t verified_count = 0;
  try
    {
      verified_count =
	DFAUtil::for_each_position(losing_curr, [&](const DFAString& position)
	{
          try
            {
              verify_losing_position(game, side_to_move, position, winning_prev, lost);
            }
          catch(const std::runtime_error& e)
            {
              throw std::runtime_error(game.position_to_string(position) + "\n" + e.what());
            }
	});
    }
  catch(const std::runtime_error& e)
    {
      std::cerr << e.what() << std::endl;
      throw std::runtime_error("position not losing");
    }

  std::cout << "VERIFIED " << verified_count << " / " << losing_curr->size() << " LOSING POSITIONS" << std::endl;

  if(verified_count != uint64_t(losing_curr->size()))
    {
      throw std::runtime_error("verified count does not match DFA size");
    }
}

void verify_lost_position(const Game& game, int side_to_move, const DFAString& position)
{
  std::vector<DFAString> moves = game.validate_moves(side_to_move, position);
  bool moves_mismatch = moves.size() > 0;

  std::optional<int> result_actual = game.validate_result(side_to_move, position);
  bool result_mismatch = !result_actual || (*result_actual != -1);

  if(result_mismatch || moves_mismatch)
    {
      // built up instead of printed so that the caller does the writing.
      // for_each_position may run this on many positions at once, and
      // interleaved reports would be unreadable.

      std::ostringstream report;

      report << game.position_to_string(position) << "\n";

      if(!result_actual)
        {
          report << "# RESULT MISMATCH: not terminal" << "\n";
        }
      else if(result_mismatch)
        {
          report << "# RESULT MISMATCH: expected lost (-1), actual " << *result_actual << "\n";
        }

      if(moves_mismatch)
        {
          report << "# MOVES MISMATCH: expected some, actual " << moves.size() << "\n";
        }

      report << "\n";

      throw std::runtime_error(report.str());
    }
}

void verify_lost_sound(const Game& game, int side_to_move, shared_dfa_ptr positions)
{
  std::cout << "VERIFYING " << positions->size() << " LOST POSITIONS" << std::endl;
  
  uint64_t verified_count = 0;
  try
    {
      verified_count =
	DFAUtil::for_each_position(positions, [&](const DFAString& position)
	{
	  verify_lost_position(game, side_to_move, position);
	});
    }
  catch(const std::runtime_error& e)
    {
      std::cerr << e.what() << std::endl;
      throw std::runtime_error("position not lost");
    }

  std::cout << "VERIFIED " << verified_count << " / " << positions->size() << " LOST POSITIONS" << std::endl;

  if(verified_count != uint64_t(positions->size()))
    {
      throw std::runtime_error("verified count does not match DFA size");
    }
}

int verify_parse_side_to_move(std::string dfa_name)
{
  std::vector<std::string> templates = {
    ",side_to_move={:d}",
    ",side={:d}"
  };

  for(const std::string& t : templates)
    {
      for(int side_to_move = 0; side_to_move < 2; ++side_to_move)
        {
          if(dfa_name.find(std::vformat(t, std::make_format_args(side_to_move))) != std::string::npos)
            {
              return side_to_move;
            }
        }
    }

  throw std::runtime_error("parsing side_to_move failed");
}

void verify_winning_position(const Game& game, int side_to_move, const DFAString& position, shared_dfa_ptr losing_prev, shared_dfa_ptr won)
{
  // won probably hits less often but will be better cached
  if(won->contains(position))
    {
      // won position
      return;
    }

  std::vector<DFAString> moves = game.validate_moves(side_to_move, position);
  for(const DFAString& move : moves)
    {
      if(losing_prev->contains(move))
        {
          // found winning move
          return;
        }
    }

  throw std::runtime_error("position is not won and no moves to losing positions");
}

void verify_winning_sound(const Game& game, int side_to_move, shared_dfa_ptr winning_curr, shared_dfa_ptr losing_prev)
{
  std::cout << "VERIFYING " << winning_curr->size() << " WINNING POSITIONS" << std::endl;

  shared_dfa_ptr won = game.get_positions_won(side_to_move);
  
  uint64_t verified_count = 0;
  try
    {
      verified_count =
	DFAUtil::for_each_position(winning_curr, [&](const DFAString& position)
	{
          try
            {
              verify_winning_position(game, side_to_move, position, losing_prev, won);
            }
          catch(const std::runtime_error& e)
            {
              throw std::runtime_error(game.position_to_string(position) + "\n" + e.what());
            }
	});
    }
  catch(const std::runtime_error& e)
    {
      std::cerr << e.what() << std::endl;
      throw std::runtime_error("position not winning");
    }

  std::cout << "VERIFIED " << verified_count << " / " << winning_curr->size() << " WINNING POSITIONS" << std::endl;

  if(verified_count != uint64_t(winning_curr->size()))
    {
      throw std::runtime_error("verified count does not match DFA size");
    }
}

void verify_won_position(const Game& game, int side_to_move, const DFAString& position)
{
  std::vector<DFAString> moves = game.validate_moves(side_to_move, position);
  bool moves_mismatch = moves.size() > 0;

  std::optional<int> result_actual = game.validate_result(side_to_move, position);
  bool result_mismatch = !result_actual || (*result_actual != 1);

  if(result_mismatch || moves_mismatch)
    {
      // built up instead of printed so that the caller does the writing.
      // for_each_position may run this on many positions at once, and
      // interleaved reports would be unreadable.

      std::ostringstream report;

      report << game.position_to_string(position) << "\n";

      if(!result_actual)
        {
          report << "# RESULT MISMATCH: not terminal" << "\n";
        }
      else if(result_mismatch)
        {
          report << "# RESULT MISMATCH: expected won (1), actual " << *result_actual << "\n";
        }

      if(moves_mismatch)
        {
          report << "# MOVES MISMATCH: expected some, actual " << moves.size() << "\n";
        }

      report << "\n";

      throw std::runtime_error(report.str());
    }
}

void verify_won_sound(const Game& game, int side_to_move, shared_dfa_ptr positions)
{
  std::cout << "VERIFYING " << positions->size() << " WON POSITIONS" << std::endl;
  
  uint64_t verified_count = 0;
  try
    {
      verified_count =
	DFAUtil::for_each_position(positions, [&](const DFAString& position)
	{
	  verify_won_position(game, side_to_move, position);
	});
    }
  catch(const std::runtime_error& e)
    {
      std::cerr << e.what() << std::endl;
      throw std::runtime_error("position not won");
    }

  std::cout << "VERIFIED " << verified_count << " / " << positions->size() << " WON POSITIONS" << std::endl;

  if(verified_count != uint64_t(positions->size()))
    {
      throw std::runtime_error("verified count does not match DFA size");
    }
}
