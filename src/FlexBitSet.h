// FlexBitSet.hpp

#ifndef FLEX_BIT_SET_HPP
#define FLEX_BIT_SET_HPP

#include <string>

#include "CompactBitSet.h"
#include "OrderedBitSet.h"

class FlexBitSetIndex;
class FlexBitSetIterator;

class FlexBitSet
{
private:

  size_t _size;
  std::string _filename;

  CompactBitSet *_compact_bitset;
  OrderedBitSet *_ordered_bitset;

public:

  FlexBitSet(size_t);
  FlexBitSet(std::string, size_t);
  FlexBitSet(const FlexBitSet&) = delete;
  FlexBitSet(FlexBitSet&&);

  ~FlexBitSet();

  void add(size_t);
  void allocate();
  FlexBitSetIterator cbegin() const;
  FlexBitSetIterator cend() const;
  bool check(size_t) const;
  size_t count() const;
  void prepare(size_t);
  size_t size() const;

  friend FlexBitSetIndex;
  friend FlexBitSetIterator;
};

class FlexBitSetIndex
{
private:

  CompactBitSetIndex *_compact_index;
  OrderedBitSetIndex *_map_index;

public:

  FlexBitSetIndex(const FlexBitSet&);

  size_t rank(size_t) const;
};

class FlexBitSetIterator
{
private:

  CompactBitSetIterator *_compact_iter;
  OrderedBitSetIterator *_map_iter;

  FlexBitSetIterator(const CompactBitSetIterator&);
  FlexBitSetIterator(const OrderedBitSetIterator&);

public:

  FlexBitSetIterator(const FlexBitSetIterator&);

  size_t operator*() const;
  FlexBitSetIterator& operator++(); // prefix ++
  bool operator<(const FlexBitSetIterator&) const;

  friend FlexBitSet;
};

#endif
