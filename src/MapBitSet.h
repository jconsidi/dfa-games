// MapBitSet.h

#ifndef MAP_BIT_SET_H
#define MAP_BIT_SET_H

#include <set>
#include <vector>

class MapBitSetIterator;

class MapBitSet
{
 private:

  size_t _size;
  std::set<size_t> _set;

 public:

  MapBitSet(size_t);
  MapBitSet(const MapBitSet&) = delete;
  MapBitSet(MapBitSet&&);

  void add(size_t);
  MapBitSetIterator begin() const;
  MapBitSetIterator cbegin() const;
  MapBitSetIterator cend() const;
  bool check(size_t) const;
  size_t count() const;
  MapBitSetIterator end() const;
  size_t size() const;
};

class MapBitSetIndex
{
  std::vector<size_t> _index;

 public:

  MapBitSetIndex(const MapBitSet& bitset_in);

  size_t rank(size_t) const;
};

class MapBitSetIterator
{
 private:

  std::set<size_t>::iterator _iter;
  std::set<size_t>::iterator _end;

  MapBitSetIterator(const std::set<size_t>::iterator&, const std::set<size_t>::iterator&);

 public:

  size_t operator*() const;
  MapBitSetIterator& operator++(); // prefix ++
  bool operator<(const MapBitSetIterator&) const;

  friend MapBitSet;
};

#endif
