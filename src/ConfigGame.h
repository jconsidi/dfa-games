// ConfigGame.h

#ifndef CONFIG_GAME_H
#define CONFIG_GAME_H

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "Game.h"

class ConfigBase
{
 private:

  std::string game_name;
  nlohmann::json game_config;
  nlohmann::json components_config;

 protected:

  ConfigBase(std::string);

  static nlohmann::json read_config(std::string, std::string);

  bool check_component_config(std::string) const;
  bool check_game_config(std::string) const;
  const nlohmann::json& get_component_config(std::string) const;
  const nlohmann::json& get_game_config(std::string) const;
  dfa_shape_t get_shape_config() const;
};

class ConfigGame
: private ConfigBase, public Game
{
 protected:

  ConfigGame(std::string);

  virtual MoveGraph build_move_graph(int) const;
  virtual shared_dfa_ptr build_positions_lost(int) const;

  shared_dfa_ptr get_component(std::string) const;

 public:

  virtual DFAString get_position_initial() const;
};

#endif
