// BitSet.hpp

#ifndef BIT_SET_HPP
#define BIT_SET_HPP

#include "CompactBitSet.h"

typedef CompactBitSet BitSet;
typedef CompactBitSetIndex BitSetIndex;
typedef CompactBitSetIterator BitSetIterator;

template <class T>
void populate_bitset(BitSet& bitset, T cbegin, T cend)
{
  for(T iter = cbegin; iter < cend; ++iter)
    {
      bitset.prepare(*iter);
    }

  bitset.allocate();

  for(T iter = cbegin; iter < cend; ++iter)
    {
      bitset.add(*iter);
    }
}

#endif
