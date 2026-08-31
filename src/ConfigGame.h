// ConfigGame.h

#ifndef CONFIG_GAME_H
#define CONFIG_GAME_H

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "Game.h"
#include "NormalPlayGame.h"

class ConfigBase
{
 private:

  std::string game_name;
  nlohmann::json game_config;
  nlohmann::json components_config;

 protected:

  ConfigBase(std::string);

  static dfa_shape_t get_shape_config(std::string);
  static nlohmann::json read_config(std::string, std::string);

  bool check_component_config(std::string) const;
  bool check_game_config(std::string) const;
  const nlohmann::json& get_component_config(std::string) const;
  const nlohmann::json& get_game_config(std::string) const;
  nlohmann::json read_config(std::string) const;
};

class ConfigGameBase
: protected ConfigBase
{
private:

  dfa_shape_t shape;

  void check_game(const Game& game) const;

 protected:

  ConfigGameBase(std::string);

  MoveGraph build_move_graph(const Game&, int) const;

  shared_dfa_ptr get_component(const Game&, std::string) const;

 public:

  DFAString get_position_initial() const;
};

class ConfigExplicitOutcomeGame
  : public ConfigGameBase,
    public Game
{
protected:

  virtual MoveGraph build_move_graph(int) const;
  virtual shared_dfa_ptr build_positions_lost(int) const;

  virtual DFAString get_position_initial() const;

public:

  ConfigExplicitOutcomeGame(std::string);
};

class ConfigNormalPlayGame
  : public ConfigGameBase,
    public NormalPlayGame
{
protected:

  virtual MoveGraph build_move_graph(int) const;

  virtual DFAString get_position_initial() const;

public:

  ConfigNormalPlayGame(std::string);
};

#endif
