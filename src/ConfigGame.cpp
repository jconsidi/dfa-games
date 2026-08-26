// ConfigGame.cpp

#include "ConfigGame.h"

#include <fstream>
#include <stdexcept>

#include "DFA.h"

ConfigBase::ConfigBase(std::string name_in)
  : game_name(name_in),
    game_config(read_config(name_in, "game.json"))
{
  if(game_config.at("game") != game_name)
    {
      throw std::runtime_error("game config is for " + std::string(game_config.at("game")) + " instead of " + game_name);
    }
}

nlohmann::json ConfigBase::read_config(std::string game_name_in, std::string config_filename_in)
{
  std::string config_path = "config/" + game_name_in + "/" + config_filename_in;

  std::ifstream config_file(config_path);
  if(!config_file)
    {
      throw std::runtime_error(config_path + " could not be opened");
    }

  return nlohmann::json::parse(config_file);
}

const nlohmann::json& ConfigBase::get_config_value(std::string key_in) const
{
  if(!game_config.contains(key_in))
    {
      throw std::runtime_error("game config for " + game_name + " is missing key: " + key_in);
    }

  return game_config.at(key_in);
}

dfa_shape_t ConfigBase::get_shape_config() const
{
  return get_config_value("shape").get<dfa_shape_t>();
}

ConfigGame::ConfigGame(std::string name_in)
  : ConfigBase(name_in),
    Game(name_in, ConfigBase::get_shape_config())
{
}

DFAString ConfigGame::get_position_initial() const
{
  std::vector<int> characters = get_config_value("initial_position").get<std::vector<int>>();

  return DFAString(get_shape(), characters);
}
