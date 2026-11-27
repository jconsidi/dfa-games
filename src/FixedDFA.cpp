// FixedDFA.cpp

#include "FixedDFA.h"

#include <format>
#include <stdexcept>

DFATable FixedDFA::build(const dfa_shape_t& shape_in, int fixed_square, int fixed_character)
{
  // 1 state until the fixed square, then a reject state and accept
  // state until the penultimate (mask) layer.

  int ndim = int(shape_in.size());
  if((fixed_square < 0) || (fixed_square >= ndim) || (fixed_character < 0) || (fixed_character >= shape_in[fixed_square]))
    {
      throw std::invalid_argument(std::format("FixedDFA: character {:d} at square {:d} is outside the shape", fixed_character, fixed_square));
    }

  DFATable table(shape_in);

  // fixed layer

  std::vector<dfa_state_t> fixed_row(shape_in[fixed_square], 0);
  fixed_row[fixed_character] = 1;
  dfa_state_t fixed_state_id = table.add_state(fixed_square, fixed_row);

  // layers before fixed square (if any)

  dfa_state_t next_state_id = fixed_state_id;
  for(int layer = fixed_square - 1; layer >= 0; --layer)
    {
      next_state_id = table.add_state(layer, std::vector<dfa_state_t>(shape_in[layer], next_state_id));
    }

  table.set_initial_state(next_state_id);
  return table;
}

FixedDFA::FixedDFA(const dfa_shape_t& shape_in, int fixed_square, int fixed_character)
  : TableDFA(build(shape_in, fixed_square, fixed_character))
{
}
