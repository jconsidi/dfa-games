// FlexBitSet.cpp

#include "FlexBitSet.h"

#include <cassert>
#include <iostream>

FlexBitSet::FlexBitSet(size_t size_in)
  : _size(size_in),
    _filename(""),
    _compact_bitset(0),
    _ordered_bitset(0)
{
}

FlexBitSet::FlexBitSet(std::string filename_in, size_t size_in)
  : _size(size_in),
    _filename(filename_in),
    _compact_bitset(0),
    _ordered_bitset(0)
{
}

FlexBitSet::FlexBitSet(FlexBitSet&& old)
  : _size(old._size),
    _filename(old._filename),
    _compact_bitset(old._compact_bitset),
    _ordered_bitset(old._ordered_bitset)
{
  old._compact_bitset = 0;
  old._ordered_bitset = 0;
}

FlexBitSet::~FlexBitSet()
{
  if(_compact_bitset)
    {
      delete _compact_bitset;
      _compact_bitset = 0;
    }

  if(_ordered_bitset)
    {
      delete _ordered_bitset;
      _ordered_bitset = 0;
    }
}

void FlexBitSet::add(size_t index_in)
{
  assert(index_in < _size);

  if(_compact_bitset)
    {
      _compact_bitset->add(index_in);
      return;
    }

  if(_ordered_bitset)
    {
      _ordered_bitset->add(index_in);
      return;
    }

  assert(0);
}

void FlexBitSet::allocate()
{
  if(_compact_bitset)
    {
      _compact_bitset->allocate();
      return;
    }

  if(_ordered_bitset)
    {
      // no allocation needed
      return;
    }

  // no elements prepared
}

FlexBitSetIterator FlexBitSet::cbegin() const
{
  if(_compact_bitset)
    {
      return FlexBitSetIterator(_compact_bitset->cbegin());
    }

  if(_ordered_bitset)
    {
      return FlexBitSetIterator(_ordered_bitset->cbegin());
    }

  assert(0);
}

FlexBitSetIterator FlexBitSet::cend() const
{
  if(_compact_bitset)
    {
      return FlexBitSetIterator(_compact_bitset->cend());
    }

  if(_ordered_bitset)
    {
      return FlexBitSetIterator(_ordered_bitset->cend());
    }

  assert(0);
}

bool FlexBitSet::check(size_t index_in) const
{
  if(_compact_bitset)
    {
      return _compact_bitset->check(index_in);
    }

  if(_ordered_bitset)
    {
      return _ordered_bitset->check(index_in);
    }

  assert(0);
}

size_t FlexBitSet::count() const
{
  if(_compact_bitset)
    {
      return _compact_bitset->count();
    }

  if(_ordered_bitset)
    {
      return _ordered_bitset->count();
    }

  // empty set
  return 0;
}

void FlexBitSet::prepare(size_t index_in)
{
  assert(index_in < _size);

  if(_compact_bitset)
    {
      _compact_bitset->prepare(index_in);
      return;
    }

  if(_ordered_bitset)
    {
      // actually add to the bitset, then check size and decide
      // whether to switch to compact bitset.
      _ordered_bitset->add(index_in);

      if(_ordered_bitset->count() > 1048576)
	{
	  // re-prepare with compact bit set

	  std::cout << "FlexBitSet : switching from OrderedBitSet to CompactBitSet" << std::endl;

	  _compact_bitset = (_filename != "") ? new CompactBitSet(_filename, _size) : new CompactBitSet(_size);
	  for(auto iter = _ordered_bitset->cbegin();
	      iter < _ordered_bitset->cend();
	      ++iter)
	    {
	      _compact_bitset->prepare(*iter);
	    }

	  // drop map bit set
	  delete _ordered_bitset;
	  _ordered_bitset = 0;
	}

      return;
    }

  // no previous preparation, so start with map bit set
  _ordered_bitset = new OrderedBitSet(_size);
  _ordered_bitset->add(index_in);
}

size_t FlexBitSet::size() const
{
  return _size;
}

FlexBitSetIndex::FlexBitSetIndex(const FlexBitSet& bitset_in)
  : _compact_index(0),
    _map_index(0)
{
  if(bitset_in._compact_bitset)
    {
      _compact_index = new CompactBitSetIndex(*(bitset_in._compact_bitset));
      return;
    }

  if(bitset_in._ordered_bitset)
    {
      _map_index = new OrderedBitSetIndex(*(bitset_in._ordered_bitset));
      return;
    }
}

size_t FlexBitSetIndex::rank(unsigned long index_in) const
{
  if(_compact_index)
    {
      return _compact_index->rank(index_in);
    }

  if(_map_index)
    {
      return _map_index->rank(index_in);
    }

  assert(0);
}

FlexBitSetIterator::FlexBitSetIterator(const CompactBitSetIterator& iter_in)
  : _compact_iter(new CompactBitSetIterator(iter_in)),
    _map_iter(0)
{
}

FlexBitSetIterator::FlexBitSetIterator(const OrderedBitSetIterator& iter_in)
  : _compact_iter(0),
    _map_iter(new OrderedBitSetIterator(iter_in))
{
}

FlexBitSetIterator::FlexBitSetIterator(const FlexBitSetIterator& old_in)
  : _compact_iter(old_in._compact_iter ? new CompactBitSetIterator(*(old_in._compact_iter)) : 0),
    _map_iter(old_in._map_iter ? new OrderedBitSetIterator(*(old_in._map_iter)) : 0)
{
}

size_t FlexBitSetIterator::operator*() const
{
  if(_compact_iter)
    {
      return **_compact_iter;
    }

  if(_map_iter)
    {
      return **_map_iter;
    }

  assert(0);
}

FlexBitSetIterator& FlexBitSetIterator::operator++()
{
  if(_compact_iter)
    {
      ++(*_compact_iter);
      return *this;
    }

  if(_map_iter)
    {
      ++(*_map_iter);
      return *this;
    }

  assert(0);
}

bool FlexBitSetIterator::operator<(FlexBitSetIterator const& right_in) const
{
  if(_compact_iter)
    {
      assert(right_in._compact_iter);
      return *_compact_iter < *(right_in._compact_iter);
    }

  if(_map_iter)
    {
      return *_map_iter < *(right_in._map_iter);
    }

  assert(0);
}
