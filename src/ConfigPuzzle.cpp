// ConfigPuzzle.cpp

#include "ConfigPuzzle.h"

#include <stdexcept>

#include "DFAUtil.h"

ConfigPuzzle::ConfigPuzzle(std::string name_in)
  : ConfigBase(name_in),
    Puzzle(name_in, get_shape_config(name_in))  
{
}

MoveGraph ConfigPuzzle::build_move_graph(int side_to_move) const
{
  if(side_to_move != 0)
    {
      throw std::logic_error("ConfigPuzzle::build_move_graph called with non-zero side-to-move");
    }

  return ConfigBase::build_move_graph(*this, 0);
}

shared_dfa_ptr ConfigPuzzle::build_positions_won() const
{
  const nlohmann::json components = get_component_config("components");

  std::string key = "won";
  if(!components.contains(key))
    {
      throw std::runtime_error("no won config for " + key);
      return DFAUtil::get_reject(get_shape());
    }

  return get_component(*this, key);
}
