// game_utils.h

#ifndef GAME_UTILS_H
#define GAME_UTILS_H

#include <string>

#include "Game.h"
#include "GameBase.h"
#include "Puzzle.h"

shared_dfa_ptr get_dfa(std::string game_name, std::string hash_or_name);
Game *get_game(std::string game_name);
GameBase *get_gamebase(std::string game_name);
Puzzle *get_puzzle(std::string game_name);

#endif
