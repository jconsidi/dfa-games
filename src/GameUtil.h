// GameUtil.h

#ifndef GAME_UTIL_H
#define GAME_UTIL_H

#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "DFAUtil.h"
#include "Game.h"

template<int offset, int width, int height, int... shape_pack>
class GameUtil
{
public:

  typedef Game<offset+width*height, shape_pack...> game_type;
  typedef DFAUtil<offset+width*height, shape_pack...> dfa_util;

  typedef typename game_type::dfa_type dfa_type;
  typedef typename game_type::shared_dfa_ptr shared_dfa_ptr;
  typedef typename game_type::choice_type choice_type;

  static choice_type get_choice(std::string);
  static const std::vector<std::tuple<int, int, std::vector<int>>>& get_queen_moves();

  static void update_choice(choice_type&, int, int, int, bool);
};

#endif
