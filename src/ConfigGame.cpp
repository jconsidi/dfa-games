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

dfa_shape_t ConfigBase::get_shape_config() const
{
  return game_config.at("shape").get<dfa_shape_t>();
}

ConfigGame::ConfigGame(std::string name_in)
  : ConfigBase(name_in),
    Game(name_in, ConfigBase::get_shape_config())
{
}
