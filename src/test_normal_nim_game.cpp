// test_normal_nim_game.cpp

#include <iostream>
#include <vector>

#include "DFAUtil.h"
#include "NormalNimGame.h"
#include "test_utils.h"

int test(int num_heaps)
{
  std::cout << "TESTING " << num_heaps << std::endl;

  int heap_max = 15;

  NormalNimGame nim(num_heaps, heap_max);
  dfa_shape_t shape = nim.get_shape();

  // calculate maximum number of moves

  int max_moves = num_heaps * heap_max;

  // run generic tests

  test_backward(nim, max_moves, num_heaps % 2);

  // run analytical tests

  auto positions_losing = nim.get_positions_losing(0, max_moves);
  auto positions_winning = nim.get_positions_winning(0, max_moves);

  auto positions_shared = DFAUtil::get_intersection(positions_losing, positions_winning);
  assert(positions_shared->is_constant(false));

  auto positions_combined = DFAUtil::get_union(positions_losing, positions_winning);
  assert(positions_combined->is_constant(true));

  size_t shape_product = 1;
  for(int layer = 0; layer < num_heaps; ++layer)
    {
      shape_product *= shape[layer];
    }

  for(size_t i = 0; i < shape_product; ++i)
    {
      size_t temp = i;
      std::vector<int> position_characters;
      for(int layer = 0; layer < num_heaps; ++layer)
	{
	  position_characters.push_back(temp % shape[layer]);
	  temp /= shape[layer];
	}
      DFAString position = DFAString(shape, position_characters);

      int characters_xored = 0;
      for(int layer = 0; layer < num_heaps; ++layer)
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
  test(1);
  test(2);
  test(3);
  test(4);

  return 0;
}
