// BitSet.hpp

#ifndef BIT_SET_HPP
#define BIT_SET_HPP

#include "VectorBitSet.h"

typedef VectorBitSet BitSet;
typedef VectorBitSetIndex BitSetIndex;
typedef VectorBitSetIterator BitSetIterator;

template <class T>
void populate_bitset(BitSet& bitset, T cbegin, T cend)
{
  for(T iter = cbegin; iter < cend; ++iter)
    {
      bitset.add(*iter);
    }
}

#endif
