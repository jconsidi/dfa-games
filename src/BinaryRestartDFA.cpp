// BinaryRestartDFA.cpp

#include "BinaryRestartDFA.h"

#include <algorithm>
#include <iostream>

#include "parallel.h"

BinaryRestartDFA::BinaryRestartDFA(const DFA& left_in,
                                   const DFA& right_in,
                                   const BinaryFunction& leaf_func_in)
  : BinaryDFA(left_in.get_shape(), leaf_func_in)
{
  // assume binarydfa directory has pairs from other DFAs cleared.

  int ndim = get_shape_size();

  // restart forward pass

  int forward_layer_min = -1;
  while(true)
    {
      try
        {
          int layer = forward_layer_min + 1;
          const MemoryMap<dfa_state_pair_t>& pairs = build_quadratic_read_pairs(layer);
          std::cout << "restart: layer=" << layer << " has " << pairs.size() << " pairs." << std::endl;

          size_t left_size = left_in.get_layer_size(layer);
          size_t right_size = right_in.get_layer_size(layer);
          TRY_PARALLEL_3(std::for_each, pairs.begin(), pairs.end(), [&](const dfa_state_pair_t& pair)
          {
            pair.check(left_size, right_size);
          });

          forward_layer_min = layer;
        }
      catch(...)
        {
          break;
        }
    }

  if(forward_layer_min < 0)
    {
      throw std::logic_error("previous pairs not found");
    }
  if(forward_layer_min > ndim)
    {
      throw std::logic_error("too many previous layers found");
    }

  build_quadratic_forward(left_in, right_in, forward_layer_min);

  // start backward pass from scratch

  build_quadratic_backward(left_in, right_in, ndim);
}
