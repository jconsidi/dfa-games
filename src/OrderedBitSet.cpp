// OrderedBitSet.cpp

#include "OrderedBitSet.h"

OrderedBitSet::OrderedBitSet(size_t size_in)
  : _size(size_in)
{
}

void OrderedBitSet::add(size_t index_in)
{
  _set.insert(index_in);
}

OrderedBitSetIterator OrderedBitSet::begin() const
{
  return OrderedBitSetIterator(_set.begin(), _set.end());
}

OrderedBitSetIterator OrderedBitSet::cbegin() const
{
  return OrderedBitSetIterator(_set.cbegin(), _set.cend());
}

OrderedBitSetIterator OrderedBitSet::cend() const
{
  return OrderedBitSetIterator(_set.cend(), _set.cend());
}

bool OrderedBitSet::check(size_t element_in) const
{
  assert(element_in < _size);
  return _set.contains(element_in);
}

size_t OrderedBitSet::count() const
{
  return _set.size();
}

OrderedBitSetIterator OrderedBitSet::end() const
{
  return OrderedBitSetIterator(_set.end(), _set.end());
}

OrderedBitSetIndex::OrderedBitSetIndex(const OrderedBitSet& bitset_in)
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

size_t OrderedBitSetIndex::rank(size_t element_in) const
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

  assert(_index[index_min] == element_in);
  return index_min;
}

OrderedBitSetIterator::OrderedBitSetIterator(const std::set<size_t>::iterator& iter_in, const std::set<size_t>::iterator& end_in)
  : _iter(iter_in),
    _end(end_in)
{
}

size_t OrderedBitSetIterator::operator*() const
{
  return *_iter;
}

OrderedBitSetIterator& OrderedBitSetIterator::operator++()
{
  ++_iter;
  return *this;
}

bool OrderedBitSetIterator::operator<(const OrderedBitSetIterator& right_in) const
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
