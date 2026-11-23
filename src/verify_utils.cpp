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

void verify_losing_position(const Game& game, int side_to_move, const DFAString& position, shared_dfa_ptr winning_prev)
{
  std::optional<int> outcome = game.validate_outcome(side_to_move, position);
  if(outcome)
    {
      if(*outcome == -1)
        {
          // lost position
          return;
        }

      throw std::runtime_error("position is terminal with outcome " + std::to_string(*outcome) + ", not lost (-1)");
    }

  std::vector<DFAString> moves = game.validate_moves(side_to_move, position);
  if(moves.size() == 0)
    {
      throw std::runtime_error("position has no moves, but validate_outcome says it is not terminal");
    }

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

  // for_each_position below only mmaps losing_curr, the DFA it enumerates
  // (see its own comment) -- DFA::mmap() is not safe to call concurrently
  // the first time (it does a one-time digest check), and
  // verify_losing_position calls ->contains() on winning_prev from inside
  // that parallel callback. Warm it up here, serially, before any worker
  // thread can reach it.
  winning_prev->mmap();

  uint64_t verified_count = 0;
  try
    {
      verified_count =
	DFAUtil::for_each_position(losing_curr, [&](const DFAString& position)
	{
          try
            {
              verify_losing_position(game, side_to_move, position, winning_prev);
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

  std::optional<int> result_actual = game.validate_outcome(side_to_move, position);
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

uint64_t verify_partition(const Game& game, shared_dfa_ptr whole, const std::vector<shared_dfa_ptr>& parts)
{
  // One merge-style pass. Every DFA of a shape enumerates its positions in
  // the same lexicographic order, so if parts partition whole, each position
  // of whole is the next position (the front) of exactly one part. Each step
  // checks that and advances that part. A front below the current position
  // of whole was never matched, so it is a position of its part outside
  // whole, as is any front left when whole runs out. Each stream's order is
  // checked as it goes, so an enumeration out of order fails as such rather
  // than as a discrepancy.

  std::cout << "VERIFYING " << whole->size() << " POSITIONS PARTITIONED BY " << parts.size() << " DFAS" << std::endl;

  for(size_t i = 0; i < parts.size(); ++i)
    {
      if(parts[i]->get_shape() != whole->get_shape())
        {
          throw std::runtime_error("part " + std::to_string(i) + " has a different shape from the whole");
        }
    }

  auto fail = [&](const DFAString& position, const std::string& message)
  {
    std::cerr << game.position_to_string(position) << "\n" << message << std::endl;
    throw std::runtime_error("parts do not partition the whole: " + message);
  };

  whole->mmap();
  for(const shared_dfa_ptr& part : parts)
    {
      part->mmap();
    }

  size_t num_parts = parts.size();
  std::vector<DFAIterator> part_iters;
  std::vector<DFAIterator> part_ends;
  std::vector<std::optional<DFAString>> fronts(num_parts);
  std::vector<uint64_t> part_counts(num_parts, 0);
  for(size_t i = 0; i < num_parts; ++i)
    {
      part_iters.push_back(parts[i]->cbegin());
      part_ends.push_back(parts[i]->cend());
      if(part_iters[i] < part_ends[i])
        {
          fronts[i] = *part_iters[i];
        }
    }

  auto advance = [&](size_t i)
  {
    DFAString previous = *fronts[i];
    ++part_iters[i];
    ++part_counts[i];
    fronts[i].reset();
    if(part_iters[i] < part_ends[i])
      {
        fronts[i] = *part_iters[i];
        if(!(previous < *fronts[i]))
          {
            fail(*fronts[i], "part " + std::to_string(i) + " enumerated out of order");
          }
      }
  };

  constexpr uint64_t progress_interval = 1000000000;
  uint64_t whole_count = 0;
  std::optional<DFAString> whole_previous;
  auto whole_end = whole->cend();
  for(auto whole_iter = whole->cbegin(); whole_iter < whole_end; ++whole_iter)
    {
      DFAString position = *whole_iter;
      if(whole_previous && !(*whole_previous < position))
        {
          fail(position, "whole enumerated out of order");
        }

      std::optional<size_t> match;
      for(size_t i = 0; i < num_parts; ++i)
        {
          if(!fronts[i])
            {
              continue;
            }
          if(*fronts[i] < position)
            {
              fail(*fronts[i], "position of part " + std::to_string(i) + " is not in the whole");
            }
          if(*fronts[i] == position)
            {
              if(match)
                {
                  fail(position, "position of the whole is in parts " + std::to_string(*match) + " and " + std::to_string(i));
                }
              match = i;
            }
        }
      if(!match)
        {
          fail(position, "position of the whole is in no part");
        }

      advance(*match);
      ++whole_count;
      whole_previous = std::move(position);

      if(whole_count % progress_interval == 0)
        {
          std::cout << "VERIFIED " << whole_count << " POSITIONS SO FAR" << std::endl;
        }
    }

  for(size_t i = 0; i < num_parts; ++i)
    {
      if(fronts[i])
        {
          fail(*fronts[i], "position of part " + std::to_string(i) + " is not in the whole (after the whole's last position)");
        }
    }

  std::cout << "VERIFIED " << whole_count << " / " << whole->size() << " POSITIONS OF THE WHOLE, EACH THE NEXT POSITION OF EXACTLY ONE PART" << std::endl;
  if(whole_count != uint64_t(whole->size()))
    {
      throw std::runtime_error("verified count does not match the whole's DFA size");
    }

  uint64_t parts_count = 0;
  for(size_t i = 0; i < num_parts; ++i)
    {
      std::cout << "VERIFIED " << part_counts[i] << " / " << parts[i]->size() << " POSITIONS OF PART " << i << ", ALL IN THE WHOLE" << std::endl;
      if(part_counts[i] != uint64_t(parts[i]->size()))
        {
          throw std::runtime_error("verified count does not match part " + std::to_string(i) + "'s DFA size");
        }
      parts_count += part_counts[i];
    }

  // Implied by the pass, and checked anyway.
  if(parts_count != whole_count)
    {
      throw std::runtime_error("parts hold " + std::to_string(parts_count) + " positions but the whole holds " + std::to_string(whole_count));
    }

  std::cout << "VERIFIED PARTITION: " << whole_count << " positions in " << num_parts << " parts" << std::endl;

  return whole_count;
}

uint64_t verify_partition(const Game& game, std::string whole_name, const std::vector<std::string>& part_names)
{
  std::cout << "VERIFYING " << whole_name << " PARTITIONED BY";
  for(const std::string& part_name : part_names)
    {
      std::cout << " " << part_name;
    }
  std::cout << std::endl;

  shared_dfa_ptr whole = verify_load(game, whole_name);
  std::vector<shared_dfa_ptr> parts;
  for(const std::string& part_name : part_names)
    {
      parts.push_back(verify_load(game, part_name));
    }

  return verify_partition(game, whole, parts);
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

void verify_winning_position(const Game& game, int side_to_move, const DFAString& position, shared_dfa_ptr losing_prev)
{
  std::optional<int> outcome = game.validate_outcome(side_to_move, position);
  if(outcome)
    {
      if(*outcome == 1)
        {
          // won position
          return;
        }

      throw std::runtime_error("position is terminal with outcome " + std::to_string(*outcome) + ", not won (1)");
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

  // See the matching comment in verify_losing_sound: for_each_position only
  // mmaps winning_curr, but verify_winning_position calls ->contains() on
  // losing_prev from inside its parallel callback.
  losing_prev->mmap();

  uint64_t verified_count = 0;
  try
    {
      verified_count =
	DFAUtil::for_each_position(winning_curr, [&](const DFAString& position)
	{
          try
            {
              verify_winning_position(game, side_to_move, position, losing_prev);
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

  std::optional<int> result_actual = game.validate_outcome(side_to_move, position);
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
          report << "# MOVES MISMATCH: expected none, actual " << moves.size() << "\n";
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
