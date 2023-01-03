// test_normal_nim_game.cpp

#include <iostream>
#include <vector>

#include "DFAUtil.h"
#include "NormalNimGame.h"
#include "test_utils.h"

int test(const dfa_shape_t& shape)
{
  int n = shape.size();
  std::cout << "TESTING " << n << std::endl;

  NormalNimGame nim(shape);

  // calculate maximum number of moves

  int max_moves = 0;
  for(int layer = 0; layer < n; ++layer)
    {
      max_moves += shape[layer];
    }

  // run generic tests

  test_backward(nim, max_moves, n % 2);

  // run analytical tests

  auto positions_losing = nim.get_positions_losing(0, max_moves);
  auto positions_winning = nim.get_positions_winning(0, max_moves);

  auto positions_shared = DFAUtil::get_intersection(positions_losing, positions_winning);
  assert(positions_shared->is_constant(false));

  auto positions_combined = DFAUtil::get_union(positions_losing, positions_winning);
  assert(positions_combined->is_constant(true));

  size_t shape_product = 1;
  for(int layer = 0; layer < n; ++layer)
    {
      shape_product *= shape[layer];
    }

  for(size_t i = 0; i < shape_product; ++i)
    {
      size_t temp = i;
      std::vector<int> position_characters;
      for(int layer = 0; layer < n; ++layer)
	{
	  position_characters.push_back(temp % shape[layer]);
	  temp /= shape[layer];
	}
      DFAString position = DFAString(shape, position_characters);

      int characters_xored = 0;
      for(int layer = 0; layer < n; ++layer)
	{
	  characters_xored ^= position_characters[layer];
	}

      bool losing_expected = characters_xored == 0;
      if(losing_expected)
	{
	  assert(positions_losing->contains(position));
	}
      else
	{
	  assert(positions_winning->contains(position));
	}
    }

  // done

  return 0;
}

int main()
{
  test(dfa_shape_t({NORMALNIM1_DFA_SHAPE}));
  test(dfa_shape_t({NORMALNIM2_DFA_SHAPE}));
  test(dfa_shape_t({NORMALNIM3_DFA_SHAPE}));
  test(dfa_shape_t({NORMALNIM4_DFA_SHAPE}));

  return 0;
}
