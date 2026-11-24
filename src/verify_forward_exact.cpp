// verify_forward_exact.cpp

// Verifies a forward solve's sets are exactly the reachable positions:
// forward,ply=000 is the initial position alone, and each forward,ply=NNN
// below PLY_MAX is followed by exactly the positions its moves reach (see
// verify_forward_exact below). Memory is one bit per position
// of the next ply plus a count per DFA state.

#include <atomic>
#include <bit>
#include <format>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "game_utils.h"
#include "verify_utils.h"

// The rank of each position of a DFA in its lexicographic order, from the
// number of accepted suffixes of every state, computed from the transition
// rows alone. States 0 (reject) and 1 (accept) have rows too, so they need
// no special case.
class VerifyRanks
{
  shared_dfa_ptr dfa;
  // suffixes[layer][state]: accepted strings from state at layer
  std::vector<std::vector<uint64_t>> suffixes;

public:

  explicit VerifyRanks(shared_dfa_ptr dfa_in)
    : dfa(dfa_in)
  {
    int ndim = dfa->get_shape_size();
    suffixes.resize(size_t(ndim + 1));
    suffixes[size_t(ndim)] = {0, 1};
    for(int layer = ndim - 1; layer >= 0; --layer)
      {
        size_t layer_size = dfa->get_layer_size(layer);
        int layer_shape = dfa->get_layer_shape(layer);
        const std::vector<uint64_t>& next_suffixes = suffixes[size_t(layer + 1)];
        std::vector<uint64_t>& curr_suffixes = suffixes[size_t(layer)];
        curr_suffixes.resize(layer_size);
        for(size_t state = 0; state < layer_size; ++state)
          {
            DFATransitionsReference transitions = dfa->get_transitions(layer, state);
            uint64_t total = 0;
            for(int c = 0; c < layer_shape; ++c)
              {
                uint64_t add = next_suffixes.at(size_t(transitions[c]));
                if(total + add < total)
                  {
                    throw std::overflow_error("DFA has more than 2^64 positions");
                  }
                total += add;
              }
            curr_suffixes[state] = total;
          }
      }
  }

  uint64_t size() const
  {
    return suffixes[0].at(size_t(dfa->get_initial_state()));
  }

  // The rank of position, or none if the DFA does not accept it.
  std::optional<uint64_t> rank(const DFAString& position) const
  {
    int ndim = dfa->get_shape_size();
    uint64_t output = 0;
    size_t state = size_t(dfa->get_initial_state());
    for(int layer = 0; layer < ndim; ++layer)
      {
        DFATransitionsReference transitions = dfa->get_transitions(layer, state);
        const std::vector<uint64_t>& next_suffixes = suffixes[size_t(layer + 1)];
        for(int c = 0; c < position[layer]; ++c)
          {
            output += next_suffixes[size_t(transitions[c])];
          }
        state = size_t(transitions[position[layer]]);
        if(state == 0)
          {
            return std::optional<uint64_t>();
          }
      }

    return output;
  }
};

// Checks next is exactly the positions reached by one move from curr, side
// to move to play: every move of every position of curr is in next, and
// every position of next is some such move. A position whose game is over
// has no moves. Each position of next is numbered by its rank in DFA order,
// and a bit per rank marks it reached, so the check needs memory for next's
// DFA and one bit per position of next, not for the moves. Throws on the
// first failure, naming the position. Returns the positions in next.
static uint64_t verify_forward_exact(const Game& game, int side_to_move, shared_dfa_ptr curr, shared_dfa_ptr next)
{
  std::cout << "VERIFYING " << next->size() << " POSITIONS ARE EXACTLY THE MOVES FROM " << curr->size() << " POSITIONS" << std::endl;

  if(curr->get_shape() != next->get_shape())
    {
      throw std::runtime_error("current and next plies have different shapes");
    }

  // VerifyRanks and the callbacks below read next's transitions from many
  // threads; map it first, serially.
  next->mmap();
  VerifyRanks ranks(next);
  uint64_t next_count = ranks.size();
  if(next_count != uint64_t(next->size()))
    {
      throw std::logic_error("ranked position count " + std::to_string(next_count) + " does not match the next ply's DFA size");
    }

  std::vector<std::atomic<uint64_t>> reached((next_count + 63) / 64);
  std::atomic<uint64_t> moves_count(0);

  uint64_t curr_count = verify_each_position(game, curr, "CURRENT", "matched by the next ply", {next}, [&](const DFAString& position)
  {
    std::vector<DFAString> moves = validated_moves(game.validate_position(side_to_move, position));
    for(const DFAString& move : moves)
      {
        std::optional<uint64_t> move_rank = ranks.rank(move);
        if(!move_rank)
          {
            throw std::runtime_error("position has a move outside the next ply:\n" + game.position_to_string(move));
          }
        reached[*move_rank / 64].fetch_or(uint64_t(1) << (*move_rank % 64), std::memory_order_relaxed);
      }
    moves_count.fetch_add(moves.size(), std::memory_order_relaxed);
  });

  // every position of next must have been reached
  for(size_t word = 0; word < reached.size(); ++word)
    {
      uint64_t expected = ~uint64_t(0);
      if((word == reached.size() - 1) && (next_count % 64 != 0))
        {
          expected = (uint64_t(1) << (next_count % 64)) - 1;
        }
      uint64_t bits = reached[word].load();
      if(bits != expected)
        {
          uint64_t missing_rank = word * 64 + uint64_t(std::countr_one(bits));
          uint64_t index = 0;
          for(auto iter = next->cbegin(); iter < next->cend(); ++iter, ++index)
            {
              if(index == missing_rank)
                {
                  std::cerr << game.position_to_string(*iter) << std::endl;
                  break;
                }
            }
          throw std::runtime_error("position " + std::to_string(missing_rank) + " of the next ply is not reached by any move");
        }
    }

  std::cout << "VERIFIED " << curr_count << " POSITIONS' " << moves_count.load() << " MOVES REACH EXACTLY THE " << next_count << " POSITIONS OF THE NEXT PLY" << std::endl;

  return next_count;
}

int main(int argc, char **argv)
{
  if(argc != 3)
    {
      std::cerr << "usage: verify_forward_exact GAME PLY_MAX" << std::endl;
      return 1;
    }

  std::string game_name(argv[1]);
  Game *game = get_game(game_name);

  int ply_max = atoi(argv[2]);
  if(ply_max < 0)
    {
      std::cerr << "PLY_MAX must be at least 0." << std::endl;
      return 1;
    }

  auto name = [](int ply)
  {
    return std::format("forward,ply={:03d}", ply);
  };

  shared_dfa_ptr curr = verify_load(*game, name(0));
  DFAString initial = game->get_position_initial();
  if((curr->size() != 1) || !curr->contains(initial))
    {
      throw std::runtime_error(name(0) + " has " + std::to_string(curr->size()) + " positions and is not exactly the initial position");
    }
  std::cout << "VERIFIED " << name(0) << " IS EXACTLY THE INITIAL POSITION" << std::endl;

  std::vector<uint64_t> counts = {1};
  for(int ply = 0; ply < ply_max; ++ply)
    {
      shared_dfa_ptr next = verify_load(*game, name(ply + 1));
      std::cout << "PLY " << ply << " SIDE " << (ply % 2) << ": " << name(ply) << " TO " << name(ply + 1) << std::endl;
      counts.push_back(verify_forward_exact(*game, ply % 2, curr, next));
      curr = next;
    }

  std::cout << "VERIFIED FORWARD PLIES 0-" << ply_max << " ARE EXACTLY THE REACHABLE POSITIONS" << std::endl;
  std::cout << std::format("{:>4} {:>16}", "ply", "positions") << std::endl;
  for(size_t ply = 0; ply < counts.size(); ++ply)
    {
      std::cout << std::format("{:>4} {:>16}", ply, counts[ply]) << std::endl;
    }

  return 0;
}
