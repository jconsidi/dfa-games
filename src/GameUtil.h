// GameUtil.h

#ifndef GAME_UTIL_H
#define GAME_UTIL_H

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "DFAUtil.h"
#include "Game.h"

class GameUtil
{
public:

  static const std::vector<std::tuple<int, int, std::vector<int>>>& get_queen_moves(int, int, int);
};

#endif
