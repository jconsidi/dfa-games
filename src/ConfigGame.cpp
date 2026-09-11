// ConfigGame.cpp

#include "ConfigGame.h"

#include <format>
#include <stdexcept>

#include "DFA.h"
#include "DFAUtil.h"
#include "ScratchConfig.h"

ConfigGameBase::ConfigGameBase(std::string name_in)
  : ConfigBase(name_in)
{
}

DFAString ConfigGameBase::get_position_initial() const
{
  std::vector<int> characters = get_game_config("initial_position").get<std::vector<int>>();

  return DFAString(get_shape(), characters);
}

ConfigExplicitOutcomeGame::ConfigExplicitOutcomeGame(std::string name_in)
  : ConfigGameBase(name_in),
    Game(name_in, get_shape_config(name_in))
{
}

MoveGraph ConfigExplicitOutcomeGame::build_move_graph(int side_to_move) const
{
  return ConfigGameBase::build_move_graph(*this, side_to_move);
}

shared_dfa_ptr ConfigExplicitOutcomeGame::build_positions_lost(int side_to_move) const
{
  const nlohmann::json components = get_component_config("components");

  std::string key = "lost,side_to_move=" + std::to_string(side_to_move);
  if(!components.contains(key))
    {
      throw std::runtime_error("no lost config for " + key);
      return DFAUtil::get_reject(get_shape());
    }

  return get_component(*this, key);
}

DFAString ConfigExplicitOutcomeGame::get_position_initial() const
{
  return ConfigGameBase::get_position_initial();
}

ConfigNormalPlayGame::ConfigNormalPlayGame(std::string name_in)
  : ConfigGameBase(name_in),
    NormalPlayGame(name_in, get_shape_config(name_in))
{
}

MoveGraph ConfigNormalPlayGame::build_move_graph(int side_to_move) const
{
  return ConfigGameBase::build_move_graph(*this, side_to_move);
}

DFAString ConfigNormalPlayGame::get_position_initial() const
{
  return ConfigGameBase::get_position_initial();
}
