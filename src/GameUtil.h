// GameUtil.h

#ifndef GAME_UTIL_H
#define GAME_UTIL_H

#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "DFAUtil.h"
#include "Game.h"

class GameUtil
{
public:

  static std::vector<std::pair<int, int>> get_between(int, int, int, int);
  static const std::vector<std::tuple<int, int, std::vector<int>>>& get_queen_moves(int, int, int);
};

#endif
