// verify_utils.cpp

#include "verify_utils.h"

#include <format>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "DFAUtil.h"

shared_dfa_ptr verify_load(const Game& game, std::string dfa_name)
{
  // Callers dereference what they get back without checking, and the load
  // path only reports a missing file as "open() failed" with no indication
  // of which DFA was wanted.

  shared_dfa_ptr output;
  try
    {
      output = game.load(dfa_name);
    }
  catch(const std::runtime_error& e)
    {
      throw std::runtime_error("could not load DFA \"" + dfa_name +
			       "\" for game \"" + game.get_name() + "\": " + e.what());
    }

  if(!output)
    {
      throw std::runtime_error("no DFA named \"" + dfa_name +
			       "\" for game \"" + game.get_name() + "\"");
    }

  return output;
}

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

void verify_losing_sound(const Game& game, int side_to_move, std::string losing_curr_name, std::string winning_prev_name)
{
  std::cout << "VERIFYING " << losing_curr_name << " WITH " << winning_prev_name << std::endl;

  shared_dfa_ptr losing_curr = verify_load(game, losing_curr_name);
  shared_dfa_ptr winning_prev = verify_load(game, winning_prev_name);

  verify_losing_sound(game, side_to_move, losing_curr, winning_prev);
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
          report << "# MOVES MISMATCH: expected none, actual " << moves.size() << "\n";
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

void verify_lost_sound(const Game& game, int side_to_move, std::string lost_name)
{
  std::cout << "VERIFYING " << lost_name << std::endl;
  shared_dfa_ptr lost_positions = verify_load(game, lost_name);

  verify_lost_sound(game, side_to_move, lost_positions);
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

void verify_winning_sound(const Game& game, int side_to_move, std::string winning_curr_name, std::string losing_prev_name)
{
  std::cout << "VERIFYING " << winning_curr_name << " WITH " << losing_prev_name << std::endl;

  shared_dfa_ptr winning_curr = verify_load(game, winning_curr_name);
  shared_dfa_ptr losing_prev = verify_load(game, losing_prev_name);

  verify_winning_sound(game, side_to_move, winning_curr, losing_prev);
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

void verify_won_sound(const Game& game, int side_to_move, std::string won_name)
{
  std::cout << "VERIFYING " << won_name << std::endl;
  shared_dfa_ptr won_positions = verify_load(game, won_name);

  verify_won_sound(game, side_to_move, won_positions);
}
