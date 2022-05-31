// BitSet.hpp

#ifndef BIT_SET_HPP
#define BIT_SET_HPP

#include <ctype.h>

#include "MemoryMap.h"

class BitSetIndex;
class BitSetIterator;

class BitSet
{
private:

  size_t _size;
  MemoryMap<uint64_t> *_memory_map;

public:

  BitSet(size_t);
  BitSet(std::string, size_t);
  BitSet(const BitSet&) = delete;
  BitSet(BitSet&&);

  ~BitSet();

  void add(size_t);
  BitSetIterator cbegin() const;
  BitSetIterator cend() const;
  bool check(size_t) const;
  size_t count() const;
  size_t size() const;

  friend BitSetIndex;
  friend BitSetIterator;
};

class BitSetIndex
{
private:

  const BitSet& _vector;
  MemoryMap<size_t> _index;

public:

  BitSetIndex(const BitSet&);

  size_t operator[](size_t) const;
};

class BitSetIterator
{
private:

  const BitSet& _bit_vector;
  size_t _index_high;
  size_t _index_low;

  BitSetIterator(const BitSet& bit_vector_in, size_t index_high_in, size_t index_low_in);

public:

  size_t operator*() const;
  BitSetIterator& operator++(); // prefix ++
  bool operator<(const BitSetIterator&) const;

  friend BitSet;
};

#endif
