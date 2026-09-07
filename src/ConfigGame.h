// ConfigGame.h

#ifndef CONFIG_GAME_H
#define CONFIG_GAME_H

#include <string>
#include <vector>

#include "ConfigBase.h"
#include "DFA.h"
#include "Game.h"
#include "NormalPlayGame.h"

class ConfigGameBase
: protected ConfigBase
{
 protected:

  ConfigGameBase(std::string);

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
  dfa_shape_t get_shape() const {return Game::get_shape();}

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
  dfa_shape_t get_shape() const {return Game::get_shape();}

public:

  ConfigNormalPlayGame(std::string);
};

#endif
