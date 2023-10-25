// UnorderedBitSet.cpp

#include "UnorderedBitSet.h"

#include <cassert>

UnorderedBitSet::UnorderedBitSet(size_t size_in)
  : _size(size_in)
{
}

UnorderedBitSet::UnorderedBitSet(UnorderedBitSet&& old)
  : _size(old._size)
{
  std::swap(_map, old._map);
  std::swap(_order, old._order);
}

void UnorderedBitSet::add(size_t index_in)
{
  if(!_map.contains(index_in))
    {
      _map.try_emplace(index_in, _map.size());
      _order.push_back(index_in);
    }
}

UnorderedBitSetIterator UnorderedBitSet::begin() const
{
  return _order.cbegin();
}

UnorderedBitSetIterator UnorderedBitSet::cbegin() const
{
  return _order.cbegin();
}

UnorderedBitSetIterator UnorderedBitSet::cend() const
{
  return _order.cend();
}

bool UnorderedBitSet::check(size_t element_in) const
{
  assert(element_in < _size);
  return _map.contains(element_in);
}

size_t UnorderedBitSet::count() const
{
  return _map.size();
}

UnorderedBitSetIterator UnorderedBitSet::end() const
{
  return _order.cend();
}

size_t UnorderedBitSet::rank(size_t element_in) const
{
  assert(element_in < _size);
  return _map.at(element_in);
}

UnorderedBitSetIndex::UnorderedBitSetIndex(const UnorderedBitSet& bitset_in)
  : _bitset(bitset_in)
{
}

size_t UnorderedBitSetIndex::rank(size_t element_in) const
{
  return _bitset.rank(element_in);
}
