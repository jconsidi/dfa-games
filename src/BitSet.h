// BitSet.hpp

#ifndef BIT_SET_HPP
#define BIT_SET_HPP

#include "UnorderedBitSet.h"

#define BIT_SET_PREPARE_AND_ALLOCATE 0
typedef UnorderedBitSet BitSet;
typedef UnorderedBitSetIndex BitSetIndex;
typedef UnorderedBitSetIterator BitSetIterator;

template <class T>
void populate_bitset(BitSet& bitset, T cbegin, T cend)
{
#if BIT_SET_PREPARE_AND_ALLOCATE
  for(T iter = cbegin; iter < cend; ++iter)
    {
      bitset.prepare(*iter);
    }

  bitset.allocate();
#endif

  for(T iter = cbegin; iter < cend; ++iter)
    {
      bitset.add(*iter);
    }
}

#endif
