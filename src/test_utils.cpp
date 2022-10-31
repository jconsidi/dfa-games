// test_utils.cpp

#include "test_utils.h"

#include <iostream>

#include "DFAUtil.h"

template<int ndim, int... shape_pack>
void test_backward(const Game<ndim, shape_pack...>& game_in, int moves_max, bool initial_win_expected)
{
  std::string log_prefix = "test_backward: ";

#if 0
  std::cout << log_prefix << "get_lost_positions()" << std::endl;

  for(int side_to_move = 0; side_to_move < 2; ++side_to_move)
    {
      game_in.get_lost_positions(side_to_move);
    }
#endif

  auto initial_positions = game_in.get_positions_initial();

  // first player should never lose via strategy stealing argument.

  std::cout << log_prefix << "get_positions_winning()" << std::endl;
  auto winning_positions = game_in.get_positions_winning(0, moves_max);
  auto initial_winning = DFAUtil<ndim, shape_pack...>::get_intersection(initial_positions, winning_positions);
  if(initial_win_expected)
    {
      assert(initial_winning->size() > 0);
    }
  else
    {
      // draws with perfect play
      assert(initial_winning->size() == 0);
      std::cout << "  rejected win" << std::endl;
    }
}

template<int ndim, int... shape_pack>
void test_forward(const Game<ndim, shape_pack...>& game_in, const std::vector<size_t>& positions_expected)
{
  assert(positions_expected.size() > 0);

  std::string log_prefix = "test_forward: ";

  std::cout << log_prefix << "get_positions_initial()" << std::endl;

  auto initial_positions = game_in.get_positions_initial();
  assert(initial_positions);

  for(auto iter = initial_positions->cbegin();
      iter < initial_positions->cend();
      ++iter)
    {
      std::cout << game_in.position_to_string(*iter) << std::endl;
    }
  assert(initial_positions->size() == positions_expected[0]);

  std::cout << log_prefix << "get_moves_forward()" << std::endl;

  auto current_positions = initial_positions;
  for(int depth = 0; depth + 1 < positions_expected.size(); ++depth)
    {
      int side_to_move = depth % 2;
      current_positions = game_in.get_moves_forward(side_to_move, current_positions);
      std::cout << log_prefix << "depth " << (depth + 1) << ": " << current_positions->states() << " states, " << current_positions->size() << " positions" << std::endl;

      assert(current_positions->size() == positions_expected.at(depth + 1));
    }
}

template<int ndim, int... shape_pack>
void test_game(const Game<ndim, shape_pack...>& game_in, const std::vector<size_t>& positions_expected, int moves_max, bool initial_win_expected)
{
  test_forward<ndim, shape_pack...>(game_in, positions_expected);
  test_backward<ndim, shape_pack...>(game_in, moves_max, initial_win_expected);
}

// template instantiations

#include "DFAParams.h"

#define INSTANTIATE_TEMPLATE(params)			       \
  template void test_game<params>(const Game<params>&, const std::vector<size_t>&, int, bool)

INSTANTIATE_TEMPLATE(CHESS_DFA_PARAMS);
INSTANTIATE_TEMPLATE(NORMALNIM1_DFA_PARAMS);
INSTANTIATE_TEMPLATE(NORMALNIM2_DFA_PARAMS);
INSTANTIATE_TEMPLATE(NORMALNIM3_DFA_PARAMS);
INSTANTIATE_TEMPLATE(NORMALNIM4_DFA_PARAMS);
INSTANTIATE_TEMPLATE(TICTACTOE2_DFA_PARAMS);
INSTANTIATE_TEMPLATE(TICTACTOE3_DFA_PARAMS);
INSTANTIATE_TEMPLATE(TICTACTOE4_DFA_PARAMS);
