// UnorderedBitSet.h

#ifndef UNORDERED_BIT_SET_H
#define UNORDERED_BIT_SET_H

#include <unordered_map>
#include <vector>

typedef std::vector<size_t>::const_iterator UnorderedBitSetIterator;

class UnorderedBitSet
{
 private:

  size_t _size;
  std::unordered_map<size_t, size_t> _map;
  std::vector<size_t> _order;

 public:

  UnorderedBitSet(size_t);
  UnorderedBitSet(const UnorderedBitSet&) = delete;
  UnorderedBitSet(UnorderedBitSet&&);

  void add(size_t);
  void allocate() {}
  UnorderedBitSetIterator begin() const;
  UnorderedBitSetIterator cbegin() const;
  UnorderedBitSetIterator cend() const;
  bool check(size_t) const;
  size_t count() const;
  UnorderedBitSetIterator end() const;
  void prepare(size_t) {}
  size_t rank(size_t) const;
  size_t size() const {return _size;}
};

class UnorderedBitSetIndex
{
  const UnorderedBitSet& _bitset;

 public:

  UnorderedBitSetIndex(const UnorderedBitSet& bitset_in);

  size_t rank(size_t) const;
};

#endif
