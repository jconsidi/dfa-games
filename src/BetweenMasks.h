// BetweenMasks.h

#ifndef BETWEEN_MASKS_H
#define BETWEEN_MASKS_H

#include "BoardMask.h"

struct BetweenMasks
{
  BoardMask masks[64][64] = {{0}};

  BetweenMasks();
};

extern BetweenMasks between_masks;

#endif
