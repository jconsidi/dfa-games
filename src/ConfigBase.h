// ConfigBase.h

#ifndef CONFIG_BASE_H
#define CONFIG_BASE_H

#include <nlohmann/json.hpp>

#include "DFA.h"
#include "GameBase.h"
#include "MoveGraph.h"

class ConfigBase
{
 private:

  std::string game_name;
  dfa_shape_t shape;

  nlohmann::json game_config;
  nlohmann::json components_config;

  void check_game(const GameBase& game) const;

 protected:

  ConfigBase(std::string);

  MoveGraph build_move_graph(const GameBase&, int) const;

  bool check_component_config(std::string) const;
  bool check_game_config(std::string) const;

  const nlohmann::json& get_component_config(std::string) const;
  const nlohmann::json& get_game_config(std::string) const;
  dfa_shape_t get_shape() const {return shape;}
  static dfa_shape_t get_shape_config(std::string);

  static nlohmann::json read_config(std::string, std::string);
  nlohmann::json read_config(std::string) const;

 public:

  shared_dfa_ptr get_component(const GameBase&, std::string) const;
};

#endif
