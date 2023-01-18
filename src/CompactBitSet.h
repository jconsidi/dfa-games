// CompactBitSet.hpp

#ifndef COMPACT_BIT_SET_HPP
#define COMPACT_BIT_SET_HPP

#include <ctype.h>

#include <string>

#include "VectorBitSet.h"
#include "MemoryMap.h"

class CompactBitSetIndex;
class CompactBitSetIterator;

class CompactBitSet
{
private:

  size_t _size;
  std::string _filename;

  VectorBitSet *_high_bits;
  VectorBitSetIndex *_high_index;

  VectorBitSet *_compact_bits;

public:

  CompactBitSet(size_t);
  CompactBitSet(std::string, size_t);
  CompactBitSet(const CompactBitSet&) = delete;
  CompactBitSet(CompactBitSet&&);

  ~CompactBitSet();

  void add(size_t);
  void allocate();
  CompactBitSetIterator cbegin() const;
  CompactBitSetIterator cend() const;
  bool check(size_t) const;
  size_t count() const;
  void prepare(size_t);
  size_t size() const;

  friend CompactBitSetIndex;
  friend CompactBitSetIterator;
};

class CompactBitSetIndex
{
private:

  const CompactBitSet& _bitset;
  VectorBitSetIndex *_compact_index;

public:

  CompactBitSetIndex(const CompactBitSet&);

  size_t rank(size_t) const;
};

class CompactBitSetIterator
{
private:

  VectorBitSetIterator _high_iter;
  VectorBitSetIterator _compact_iter;

  CompactBitSetIterator(VectorBitSetIterator, VectorBitSetIterator);

public:

  size_t operator*() const;
  CompactBitSetIterator& operator++(); // prefix ++
  bool operator<(const CompactBitSetIterator&) const;

  friend CompactBitSet;
};

#endif
