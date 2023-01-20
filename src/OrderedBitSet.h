// OrderedBitSet.h

#ifndef ORDERED_BIT_SET_H
#define ORDERED_BIT_SET_H

#include <set>
#include <vector>

class OrderedBitSetIterator;

class OrderedBitSet
{
 private:

  size_t _size;
  std::set<size_t> _set;

 public:

  OrderedBitSet(size_t);
  OrderedBitSet(const OrderedBitSet&) = delete;
  OrderedBitSet(OrderedBitSet&&);

  void add(size_t);
  OrderedBitSetIterator begin() const;
  OrderedBitSetIterator cbegin() const;
  OrderedBitSetIterator cend() const;
  bool check(size_t) const;
  size_t count() const;
  OrderedBitSetIterator end() const;
  size_t size() const;
};

class OrderedBitSetIndex
{
  std::vector<size_t> _index;

 public:

  OrderedBitSetIndex(const OrderedBitSet& bitset_in);

  size_t rank(size_t) const;
};

class OrderedBitSetIterator
{
 private:

  std::set<size_t>::iterator _iter;
  std::set<size_t>::iterator _end;

  OrderedBitSetIterator(const std::set<size_t>::iterator&, const std::set<size_t>::iterator&);

 public:

  size_t operator*() const;
  OrderedBitSetIterator& operator++(); // prefix ++
  bool operator<(const OrderedBitSetIterator&) const;

  friend OrderedBitSet;
};

#endif
