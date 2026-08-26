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

 protected:

  ConfigBase(std::string);

  static nlohmann::json read_config(std::string, std::string);

  const nlohmann::json& get_config_value(std::string) const;
  dfa_shape_t get_shape_config() const;
};

class ConfigGame
: private ConfigBase, public Game
{
 protected:

  ConfigGame(std::string);

 public:

  virtual DFAString get_position_initial() const;
};

#endif
