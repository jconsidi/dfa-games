// Side.h

#ifndef SIDE_H
#define SIDE_H

enum Side {SIDE_WHITE, SIDE_BLACK};

inline Side side_flip(Side side_in)
{
  return side_in == SIDE_WHITE ? SIDE_BLACK : SIDE_WHITE;
}

#endif
