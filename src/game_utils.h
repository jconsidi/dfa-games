// game_utils.h

#ifndef GAME_UTILS_H
#define GAME_UTILS_H

#include <string>

#include "Game.h"
#include "GameBase.h"

Game *get_game(std::string game_name);
GameBase *get_gamebase(std::string game_name);

#endif
