// Singleton.h

#ifndef SINGLETON_H
#define SINGLETON_H

#include "DFA.h"
#include "Side.h"

class Singleton
{
  Singleton() {}; // block instantiation

 public:

  static const DFA *get_check(Side);
  static const DFA *get_has_moves(Side);
};

#endif
