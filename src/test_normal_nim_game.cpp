// test_normal_nim_game.cpp

#include <iostream>
#include <vector>

#include "DFAUtil.h"
#include "NormalNimGame.h"
#include "test_utils.h"

void test_states(const NormalNimGame& game, shared_dfa_ptr results_dfa)
{
  dfa_shape_t shape = game.get_shape();

  if(shape.size() == 0)
    {
      // degenerate case
      return;
    }

  // sanity check shape.
  for(int layer = 0; layer < shape.size(); ++layer)
    {
      // each heap can be non-empty
      assert(shape[layer] > 0);

      // max heap size is shared
      assert(shape[layer] == shape[0]);
    }

  // confirm layer sizes

  std::vector<dfa_state_t> non_constant_layer_sizes;
  for(int layer = 0; layer < shape.size(); ++layer)
    {
      // filter out constants
      assert(results_dfa->get_layer_size(layer) >= 2);
      non_constant_layer_sizes.push_back(results_dfa->get_layer_size(layer) - 2);
    }

  // heap sizes

  dfa_state_t heap_max = shape[0] - 1;
  int lg_heap_max = 0;
  while(dfa_state_t(1) << (lg_heap_max + 1) <= heap_max)
    {
      ++lg_heap_max;
    }
  assert(dfa_state_t(1) << lg_heap_max <= heap_max);

  // layer 0
  assert(non_constant_layer_sizes[0] == 1);
  if(shape.size() == 1)
    {
      return;
    }

  // layer 1
  assert(non_constant_layer_sizes[1] == heap_max + 1);
  if(shape.size() == 2)
    {
      return;
    }

  // layers 2 to last-1

  for(int layer = 2; layer < shape.size() - 1; ++layer)
    {
      // shared limits. need to rewrite this check if this changes.
      assert(shape[layer] == shape[0]);

      assert(non_constant_layer_sizes[layer] == dfa_state_t(1) << (lg_heap_max + 1));
    }

  // layer last
  // limited again by what can values can XOR to zero.
  assert(non_constant_layer_sizes[shape.size() - 1] == heap_max + 1);
}

void test(int num_heaps, int heap_max)
{
  std::cout << "TESTING " << num_heaps << "x" << heap_max << std::endl;

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

  // confirm state analysis

  test_states(nim, positions_losing);
  test_states(nim, positions_winning);
}

int main()
{
  for(int num_heaps = 1; num_heaps <= 4; ++num_heaps)
    {
      for(int heap_max = 1; heap_max <= 4; ++heap_max)
	{
	  test(num_heaps, heap_max);
	}

      test(num_heaps, 7);
      test(num_heaps, 8);
      test(num_heaps, 15);
      test(num_heaps, 16);
    }

  return 0;
}
