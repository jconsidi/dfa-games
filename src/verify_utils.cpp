// verify_utils.cpp

#include "verify_utils.h"

#include <chrono>
#include <format>
#include <functional>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <variant>
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

// Calls check(position) for every position of positions, in parallel, and
// returns how many there were. Prints a VERIFYING line before and a
// VERIFIED line after, calling the positions label. A check that throws
// std::runtime_error fails the whole call: the position and the error are
// printed, and std::runtime_error("position not " + failure) is thrown. The
// count must match the DFA's size.
//
// for_each_position only mmaps positions, the DFA it enumerates, while check
// may call ->contains() on others from many threads. DFA::mmap() is not
// safe to call concurrently the first time (it does a one-time digest
// check), so the DFAs in lookups are mapped here, serially, first.
static uint64_t verify_each_position(const Game& game,
                                     shared_dfa_ptr positions,
                                     const std::string& label,
                                     const std::string& failure,
                                     const std::vector<shared_dfa_ptr>& lookups,
                                     const std::function<void(const DFAString&)>& check)
{
  std::cout << "VERIFYING " << positions->size() << " " << label << " POSITIONS" << std::endl;

  for(const shared_dfa_ptr& lookup : lookups)
    {
      lookup->mmap();
    }

  uint64_t verified_count = 0;
  try
    {
      verified_count =
	DFAUtil::for_each_position(positions, [&](const DFAString& position)
	{
          try
            {
              check(position);
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
      throw std::runtime_error("position not " + failure);
    }

  std::cout << "VERIFIED " << verified_count << " / " << positions->size() << " " << label << " POSITIONS" << std::endl;

  if(verified_count != uint64_t(positions->size()))
    {
      throw std::runtime_error("verified count does not match DFA size");
    }

  return verified_count;
}

void verify_losing_position(const Game& game, int side_to_move, const DFAString& position, shared_dfa_ptr winning_prev)
{
  ValidatedPosition validated = game.validate_position(side_to_move, position);
  if(const GameOver *over = std::get_if<GameOver>(&validated))
    {
      if(over->result == -1)
        {
          // lost position
          return;
        }

      throw std::runtime_error("position is terminal with outcome " + std::to_string(over->result) + ", not lost (-1)");
    }

  for(const DFAString& move : std::get<std::vector<DFAString>>(validated))
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
  verify_each_position(game, losing_curr, "LOSING", "losing", {winning_prev}, [&](const DFAString& position)
  {
    verify_losing_position(game, side_to_move, position, winning_prev);
  });
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
  ValidatedPosition validated = game.validate_position(side_to_move, position);
  const GameOver *over = std::get_if<GameOver>(&validated);
  if(over && (over->result == -1))
    {
      return;
    }

  // built up instead of printed so that the caller does the writing.
  // for_each_position may run this on many positions at once, and
  // interleaved reports would be unreadable. The caller adds the position
  // itself.

  std::ostringstream report;

  if(!over)
    {
      report << "# RESULT MISMATCH: not terminal" << "\n";
      report << "# MOVES MISMATCH: expected none, actual " << std::get<std::vector<DFAString>>(validated).size() << "\n";
    }
  else
    {
      report << "# RESULT MISMATCH: expected lost (-1), actual " << over->result << "\n";
    }

  report << "\n";

  throw std::runtime_error(report.str());
}

void verify_lost_sound(const Game& game, int side_to_move, shared_dfa_ptr positions)
{
  verify_each_position(game, positions, "LOST", "lost", {}, [&](const DFAString& position)
  {
    verify_lost_position(game, side_to_move, position);
  });
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

  // A part that is the whole itself, as verify_proof passes, partitions it
  // exactly when that part is listed once and every other part is empty.
  // That needs one walk of the whole, for its count, rather than two in
  // step.
  std::vector<size_t> whole_parts;
  for(size_t i = 0; i < num_parts; ++i)
    {
      if(parts[i].get() == whole.get())
        {
          whole_parts.push_back(i);
        }
    }
  if(whole_parts.size() > 0)
    {
      uint64_t whole_count = 0;
      auto whole_end = whole->cend();
      for(auto whole_iter = whole->cbegin(); whole_iter < whole_end; ++whole_iter)
        {
          if((whole_count == 0) && (whole_parts.size() > 1))
            {
              fail(*whole_iter, "position of the whole is in parts " + std::to_string(whole_parts[0]) + " and " + std::to_string(whole_parts[1]));
            }
          ++whole_count;
        }

      for(size_t i = 0; i < num_parts; ++i)
        {
          if(parts[i].get() == whole.get())
            {
              continue;
            }
          auto part_iter = parts[i]->cbegin();
          if(part_iter < parts[i]->cend())
            {
              DFAString position = *part_iter;
              if(whole->contains(position))
                {
                  fail(position, "position of the whole is in parts " + std::to_string(whole_parts[0]) + " and " + std::to_string(i));
                }
              fail(position, "position of part " + std::to_string(i) + " is not in the whole");
            }
        }

      std::cout << "VERIFIED " << whole_count << " / " << whole->size() << " POSITIONS OF THE WHOLE, ALL IN PART " << whole_parts[0] << " (THE WHOLE ITSELF), EVERY OTHER PART EMPTY" << std::endl;
      if(whole_count != uint64_t(whole->size()))
        {
          throw std::runtime_error("verified count does not match the whole's DFA size");
        }

      std::cout << "VERIFIED PARTITION: " << whole_count << " positions in " << num_parts << " parts" << std::endl;

      return whole_count;
    }
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
  ValidatedPosition validated = game.validate_position(side_to_move, position);
  if(const GameOver *over = std::get_if<GameOver>(&validated))
    {
      if(over->result == 1)
        {
          // won position
          return;
        }

      throw std::runtime_error("position is terminal with outcome " + std::to_string(over->result) + ", not won (1)");
    }

  for(const DFAString& move : std::get<std::vector<DFAString>>(validated))
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
  verify_each_position(game, winning_curr, "WINNING", "winning", {losing_prev}, [&](const DFAString& position)
  {
    verify_winning_position(game, side_to_move, position, losing_prev);
  });
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
  ValidatedPosition validated = game.validate_position(side_to_move, position);
  const GameOver *over = std::get_if<GameOver>(&validated);
  if(over && (over->result == 1))
    {
      return;
    }

  // built up instead of printed so that the caller does the writing.
  // for_each_position may run this on many positions at once, and
  // interleaved reports would be unreadable. The caller adds the position
  // itself.

  std::ostringstream report;

  if(!over)
    {
      report << "# RESULT MISMATCH: not terminal" << "\n";
      report << "# MOVES MISMATCH: expected none, actual " << std::get<std::vector<DFAString>>(validated).size() << "\n";
    }
  else
    {
      report << "# RESULT MISMATCH: expected won (1), actual " << over->result << "\n";
    }

  report << "\n";

  throw std::runtime_error(report.str());
}

void verify_won_sound(const Game& game, int side_to_move, shared_dfa_ptr positions)
{
  verify_each_position(game, positions, "WON", "won", {}, [&](const DFAString& position)
  {
    verify_won_position(game, side_to_move, position);
  });
}

void verify_won_sound(const Game& game, int side_to_move, std::string won_name)
{
  std::cout << "VERIFYING " << won_name << std::endl;
  shared_dfa_ptr won_positions = verify_load(game, won_name);

  verify_won_sound(game, side_to_move, won_positions);
}

// A position of a ply's unknown set, against the next ply's winning and
// unknown sets: tied, or not over with every move in one of them and at
// least one in the unknown set.
static void verify_unknown_position(const Game& game, int side_to_move, const DFAString& position, shared_dfa_ptr winning_next, shared_dfa_ptr unknown_next)
{
  ValidatedPosition validated = game.validate_position(side_to_move, position);
  if(const GameOver *over = std::get_if<GameOver>(&validated))
    {
      if(over->result == 0)
        {
          // tied position
          return;
        }

      throw std::runtime_error("position is terminal with outcome " + std::to_string(over->result) + ", not tied (0) or undecided");
    }

  bool unknown_move = false;
  for(const DFAString& move : std::get<std::vector<DFAString>>(validated))
    {
      if(winning_next->contains(move))
        {
          continue;
        }
      if(unknown_next->contains(move))
        {
          unknown_move = true;
          continue;
        }

      throw std::runtime_error("position is unknown but has a move outside the next winning and unknown sets");
    }

  if(!unknown_move)
    {
      throw std::runtime_error("position is unknown but has no move into the next unknown set");
    }
}

// A position of the last ply's unknown set: tied, or not over.
static void verify_unknown_last_position(const Game& game, int side_to_move, const DFAString& position)
{
  // validate_position guarantees a position that is not over has a move.
  ValidatedPosition validated = game.validate_position(side_to_move, position);
  if(const GameOver *over = std::get_if<GameOver>(&validated))
    {
      if(over->result != 0)
        {
          throw std::runtime_error("position is terminal with outcome " + std::to_string(over->result) + ", not tied (0) or undecided");
        }
    }
}

// A position of a ply's winning set, against the next ply's losing set and
// domain: won, or a move into the losing set, and every move in the domain.
// A move found in the losing set needs no domain lookup, since the next
// ply's partition check shows the losing set is inside the domain.
static void verify_winning_closed_position(const Game& game, int side_to_move, const DFAString& position, shared_dfa_ptr losing_next, shared_dfa_ptr domain_next)
{
  ValidatedPosition validated = game.validate_position(side_to_move, position);
  if(const GameOver *over = std::get_if<GameOver>(&validated))
    {
      if(over->result == 1)
        {
          // won position
          return;
        }

      throw std::runtime_error("position is terminal with outcome " + std::to_string(over->result) + ", not won (1)");
    }

  bool losing_move = false;
  for(const DFAString& move : std::get<std::vector<DFAString>>(validated))
    {
      if(!losing_move && losing_next->contains(move))
        {
          losing_move = true;
          continue;
        }
      if(!domain_next->contains(move))
        {
          throw std::runtime_error("position has a move outside the next ply's domain");
        }
    }

  if(!losing_move)
    {
      throw std::runtime_error("position is not won and no moves to losing positions");
    }
}

std::vector<VerifyPlyStats> verify_solve(const Game& game, int ply_max,
                                         const std::function<VerifyPlySets(int)>& sets,
                                         VerifyClosure closure)
{
  if(ply_max < 0)
    {
      throw std::invalid_argument("verify_solve: ply_max must be at least 0");
    }

  std::string closure_description = (closure == VerifyClosure::every_move)
    ? "every move"
    : "losing positions' moves only";
  std::cout << "VERIFYING SOLVE PLIES 0-" << ply_max << ", CLOSURE: " << closure_description << std::endl;

  auto get_sets = [&](int ply)
  {
    VerifyPlySets ply_sets = sets(ply);
    if(!ply_sets.domain || !ply_sets.winning || !ply_sets.losing || !ply_sets.unknown)
      {
        throw std::logic_error("verify_solve: sets(" + std::to_string(ply) + ") left a DFA out");
      }
    return ply_sets;
  };

  std::vector<VerifyPlyStats> stats(size_t(ply_max + 1));

  auto finish_ply = [&](int ply, const VerifyPlySets& ply_sets, uint64_t winning, uint64_t losing, uint64_t unknown, std::chrono::steady_clock::time_point start)
  {
    std::cout << "PLY " << ply << ": CHECKING PARTITION" << std::endl;
    uint64_t positions = verify_partition(game, ply_sets.domain, {ply_sets.winning, ply_sets.losing, ply_sets.unknown});
    if(positions != winning + losing + unknown)
      {
        throw std::logic_error("ply " + std::to_string(ply) + " partition holds " + std::to_string(positions) + " positions, but its sets were checked with " + std::to_string(winning + losing + unknown));
      }

    std::chrono::duration<double> elapsed = std::chrono::steady_clock::now() - start;
    stats[size_t(ply)] = VerifyPlyStats{ply, positions, winning, losing, unknown, elapsed.count()};
  };

  // the last ply: results only
  VerifyPlySets next = get_sets(ply_max);
  {
    auto start = std::chrono::steady_clock::now();
    int side_to_move = ply_max % 2;
    std::cout << "PLY " << ply_max << " SIDE " << side_to_move << ": LAST PLY, CHECKING RESULTS" << std::endl;

    uint64_t losing = verify_each_position(game, next.losing, "LOST", "lost", {}, [&](const DFAString& position)
    {
      verify_lost_position(game, side_to_move, position);
    });
    uint64_t unknown = verify_each_position(game, next.unknown, "UNDECIDED", "undecided", {}, [&](const DFAString& position)
    {
      verify_unknown_last_position(game, side_to_move, position);
    });
    uint64_t winning = verify_each_position(game, next.winning, "WON", "won", {}, [&](const DFAString& position)
    {
      verify_won_position(game, side_to_move, position);
    });

    finish_ply(ply_max, next, winning, losing, unknown, start);
  }

  for(int ply = ply_max - 1; ply >= 0; --ply)
    {
      VerifyPlySets curr = get_sets(ply);
      auto start = std::chrono::steady_clock::now();
      int side_to_move = ply % 2;
      std::cout << "PLY " << ply << " SIDE " << side_to_move << ": CHECKING AGAINST PLY " << (ply + 1) << ", CLOSURE: " << closure_description << std::endl;

      uint64_t losing = verify_each_position(game, curr.losing, "LOSING", "losing", {next.winning}, [&](const DFAString& position)
      {
        verify_losing_position(game, side_to_move, position, next.winning);
      });

      uint64_t unknown = verify_each_position(game, curr.unknown, "UNKNOWN", "unknown", {next.winning, next.unknown}, [&](const DFAString& position)
      {
        verify_unknown_position(game, side_to_move, position, next.winning, next.unknown);
      });

      uint64_t winning = 0;
      if(closure == VerifyClosure::every_move)
        {
          winning = verify_each_position(game, curr.winning, "WINNING", "winning", {next.losing, next.domain}, [&](const DFAString& position)
          {
            verify_winning_closed_position(game, side_to_move, position, next.losing, next.domain);
          });
        }
      else
        {
          winning = verify_each_position(game, curr.winning, "WINNING", "winning", {next.losing}, [&](const DFAString& position)
          {
            verify_winning_position(game, side_to_move, position, next.losing);
          });
        }

      finish_ply(ply, curr, winning, losing, unknown, start);

      next = std::move(curr);
    }

  // next is now ply 0
  shared_dfa_ptr initial_positions = game.get_positions_initial();
  if(initial_positions->size() != 1)
    {
      throw std::logic_error(std::to_string(initial_positions->size()) + " initial positions, but verify_solve expects exactly one");
    }
  DFAString initial = *(initial_positions->cbegin());
  if(!next.domain->contains(initial))
    {
      std::cerr << game.position_to_string(initial) << std::endl;
      throw std::runtime_error("ply 0's domain does not contain the initial position");
    }
  std::string initial_result = next.winning->contains(initial)
    ? "WINNING"
    : next.losing->contains(initial) ? "LOSING" : "UNKNOWN";

  std::cout << "VERIFIED SOLVE PLIES 0-" << ply_max << ", CLOSURE: " << closure_description << std::endl;
  std::cout << std::format("{:>4} {:>16} {:>16} {:>16} {:>16} {:>9}", "ply", "positions", "winning", "losing", "unknown", "seconds") << std::endl;
  for(const VerifyPlyStats& ply_stats : stats)
    {
      std::cout << std::format("{:>4} {:>16} {:>16} {:>16} {:>16} {:>9.0f}", ply_stats.ply, ply_stats.positions, ply_stats.winning, ply_stats.losing, ply_stats.unknown, ply_stats.seconds) << std::endl;
    }
  std::cout << "INITIAL POSITION " << initial_result << " for side 0" << std::endl;

  return stats;
}
