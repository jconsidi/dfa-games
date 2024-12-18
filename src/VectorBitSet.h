// VectorBitSet.hpp

#ifndef VECTOR_BIT_SET_HPP
#define VECTOR_BIT_SET_HPP

#include <cstdint>
#include <string>

#include "MemoryMap.h"

class VectorBitSetIndex;
class VectorBitSetIterator;

class VectorBitSet
{
private:

  size_t _size;
  std::string _filename;
  MemoryMap<uint64_t> *_memory_map;

public:

  VectorBitSet(size_t);
  VectorBitSet(std::string, size_t);
  VectorBitSet(const VectorBitSet&) = delete;
  VectorBitSet(VectorBitSet&&);

  ~VectorBitSet();

  void add(size_t);
  VectorBitSetIterator begin() const;
  VectorBitSetIterator cbegin() const;
  VectorBitSetIterator cend() const;
  bool check(size_t) const;
  size_t count() const;
  VectorBitSetIterator end() const;
  size_t size() const;

  friend VectorBitSetIndex;
  friend VectorBitSetIterator;
};

class VectorBitSetIndex
{
private:

  const VectorBitSet& _vector;
  MemoryMap<size_t> *_index;

public:

  VectorBitSetIndex(const VectorBitSet&);

  size_t rank(size_t) const;
};

class VectorBitSetIterator
{
private:

  const VectorBitSet& _bit_vector;
  size_t _index_high;
  size_t _index_low;

  VectorBitSetIterator(const VectorBitSet& bit_vector_in, size_t index_high_in, size_t index_low_in);

public:

  bool operator!=(const VectorBitSetIterator&) const;
  size_t operator*() const;
  VectorBitSetIterator& operator++(); // prefix ++
  bool operator<(const VectorBitSetIterator&) const;

  friend VectorBitSet;
};

#endif
