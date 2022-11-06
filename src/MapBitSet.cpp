// MapBitSet.cpp

#include "MapBitSet.h"

MapBitSet::MapBitSet(size_t size_in)
  : _size(size_in)
{
}

void MapBitSet::add(size_t index_in)
{
  _set.insert(index_in);
}

MapBitSetIterator MapBitSet::begin() const
{
  return MapBitSetIterator(_set.begin(), _set.end());
}

MapBitSetIterator MapBitSet::cbegin() const
{
  return MapBitSetIterator(_set.cbegin(), _set.cend());
}

MapBitSetIterator MapBitSet::cend() const
{
  return MapBitSetIterator(_set.cend(), _set.cend());
}

bool MapBitSet::check(size_t element_in) const
{
  return _set.contains(element_in);
}

size_t MapBitSet::count() const
{
  return _set.size();
}

MapBitSetIterator MapBitSet::end() const
{
  return MapBitSetIterator(_set.end(), _set.end());
}

MapBitSetIndex::MapBitSetIndex(const MapBitSet& bitset_in)
  : _index()
{
  _index.reserve(bitset_in.count());

  for(auto iter = bitset_in.cbegin();
      iter < bitset_in.cend();
      ++iter)
    {
      _index.push_back(*iter);
    }
}

size_t MapBitSetIndex::rank(size_t element_in) const
{
  size_t index_min = 0;
  size_t index_max = _index.size();

  while(index_min < index_max)
    {
      size_t index_mid = (index_min + index_max) / 2;
      if(_index[index_mid] < element_in)
	{
	  index_min = index_mid + 1;
	}
      else
	{
	  index_max = index_mid;
	}
    }

  return index_min;
}

MapBitSetIterator::MapBitSetIterator(const std::set<size_t>::iterator& iter_in, const std::set<size_t>::iterator& end_in)
  : _iter(iter_in),
    _end(end_in)
{
}

size_t MapBitSetIterator::operator*() const
{
  return *_iter;
}

MapBitSetIterator& MapBitSetIterator::operator++()
{
  ++_iter;
  return *this;
}

bool MapBitSetIterator::operator<(const MapBitSetIterator& right_in) const
{
  // HACK : the set iterator does not have operator<

  if(_iter == _end)
    {
      return false;
    }
  if(right_in._iter == right_in._end)
    {
      return true;
    }

  return *_iter < *(right_in._iter);
}
