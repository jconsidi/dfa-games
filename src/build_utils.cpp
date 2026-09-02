// build_utils.cpp

#include "build_utils.h"

#include "DFAUtil.h"

build_triple build_backward(const Game& game,
                            int side_to_move,
                            shared_dfa_ptr target_curr,
                            std::string winning_curr_name,
                            std::string losing_curr_name,
                            std::string unknown_curr_name,
                            shared_dfa_ptr winning_next,
                            shared_dfa_ptr losing_next,
                            shared_dfa_ptr unknown_next,
                            shared_dfa_ptr winning_base,
                            shared_dfa_ptr losing_base)
{
  shared_dfa_ptr winning_curr =
    game.load_or_build(winning_curr_name, [&]()
    {
      shared_dfa_ptr will_win = game.get_moves_backward(side_to_move, losing_next);

      return DFAUtil::get_intersection(DFAUtil::get_union(winning_base, will_win),
                                       target_curr);
    });

  bool next_complete = unknown_next->is_constant(0);
  
  shared_dfa_ptr losing_curr =
    game.load_or_build(losing_curr_name, [&]()
    {
      if(next_complete)
        {
          // if next ply was completely solved, then we can just do
          // set subtraction.
          return DFAUtil::get_difference(target_curr, winning_curr);
        }

      shared_dfa_ptr could_lose = game.get_moves_backward(side_to_move, winning_next);
      shared_dfa_ptr wont_lose = game.get_moves_backward(side_to_move, DFAUtil::get_inverse(winning_next));
      // could_lose requirement is to make sure that they are forced
      // to move to a position where the other side is winning.
      shared_dfa_ptr will_lose = DFAUtil::get_difference(could_lose, wont_lose);

      // combine with "shorter" losses. usually terminal lost or the
      // pure backward losing positions.
      return DFAUtil::get_intersection(DFAUtil::get_union(losing_base, will_lose),
                                       target_curr);
    });

  shared_dfa_ptr unknown_curr =
    game.load_or_build(unknown_curr_name, [&]()
    {
      if(next_complete)
        {
          return DFAUtil::get_reject(game.get_shape());
        }

      shared_dfa_ptr winning_or_losing = DFAUtil::get_union(winning_curr, losing_curr);
      return DFAUtil::get_difference(target_curr, winning_or_losing);
    });

  if(next_complete)
    {
      // current layer should also be complete
      assert(unknown_curr->is_constant(false));
    }

  return build_triple(winning_curr, losing_curr, unknown_curr);
}
