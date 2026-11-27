// ConfigGame.cpp

#include "ConfigGame.h"

#include <format>
#include <stdexcept>

#include "DFA.h"
#include "DFAUtil.h"

ConfigGameBase::ConfigGameBase(std::string name_in)
  : ConfigBase(name_in),
    impartial(check_game_config("impartial") && get_game_config("impartial").get<bool>())
{
  if(impartial)
    {
      // Reversal is the identity only if both sides have the same
      // moves and the same lost positions.

      if(read_config("move_graph_0.json") != read_config("move_graph_1.json"))
        {
          throw std::runtime_error("game config for " + name_in + " is impartial, but its two move graphs differ");
        }

      const nlohmann::json& components = get_component_config("components");
      bool has_lost_0 = components.contains("lost,side_to_move=0");
      bool has_lost_1 = components.contains("lost,side_to_move=1");
      if((has_lost_0 != has_lost_1) ||
         (has_lost_0 && (components.at("lost,side_to_move=0") != components.at("lost,side_to_move=1"))))
        {
          throw std::runtime_error("game config for " + name_in + " is impartial, but its two lost components differ");
        }
    }
}

DFAString ConfigGameBase::get_position_initial() const
{
  std::vector<int> characters = get_game_config("initial_position").get<std::vector<int>>();

  return DFAString(get_shape(), characters);
}

// An impartial game has the same moves and outcomes for both sides, so
// a set of positions is its own reverse.
bool ConfigGameBase::is_impartial() const
{
  return impartial;
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

shared_dfa_ptr ConfigExplicitOutcomeGame::build_positions_reversed(shared_dfa_ptr positions_in) const
{
  if(is_impartial())
    {
      return positions_in;
    }

  return Game::build_positions_reversed(positions_in);
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

shared_dfa_ptr ConfigNormalPlayGame::build_positions_reversed(shared_dfa_ptr positions_in) const
{
  if(is_impartial())
    {
      return positions_in;
    }

  return NormalPlayGame::build_positions_reversed(positions_in);
}

DFAString ConfigNormalPlayGame::get_position_initial() const
{
  return ConfigGameBase::get_position_initial();
}
