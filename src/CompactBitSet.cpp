// CompactBitSet.cpp

#include "CompactBitSet.h"

#include <bit>

#define COMPACT_FACTOR 64

CompactBitSet::CompactBitSet(size_t size_in)
  : _size(size_in),
    _filename(""),
    _high_bits(new VectorBitSet((size_in + (COMPACT_FACTOR - 1)) / COMPACT_FACTOR)),
    _high_index(0),
    _compact_bits(0)
{
}

CompactBitSet::CompactBitSet(std::string filename_in, size_t size_in)
  : _size(size_in),
    _filename(filename_in),
    _high_bits(new VectorBitSet(_filename + "-high", (size_in + (COMPACT_FACTOR - 1)) / COMPACT_FACTOR)),
    _high_index(0),
    _compact_bits(0)
{
}

CompactBitSet::CompactBitSet(CompactBitSet&& old)
  : _size(old._size),
    _filename(old._filename),
    _high_bits(old._high_bits),
    _high_index(old._high_index),
    _compact_bits(old._compact_bits)
{
  old._size = 0;
  old._high_bits = 0;
  old._high_index = 0;
  old._compact_bits = 0;
}

CompactBitSet::~CompactBitSet()
{
  if(_high_bits)
    {
      delete _high_bits;
      _high_bits = 0;
    }

  if(_high_index)
    {
      delete _high_index;
      _high_index = 0;
    }

  if(_compact_bits)
    {
      delete _compact_bits;
      _compact_bits = 0;
    }
}

void CompactBitSet::add(size_t index_in)
{
  assert(index_in < _size);
  assert(_high_bits);
  assert(_high_index);
  assert(_compact_bits);

  size_t high_in = index_in / COMPACT_FACTOR;
  assert(_high_bits->check(high_in));
  size_t low_in = index_in % COMPACT_FACTOR;

  size_t high_compact = _high_index->rank(high_in);
  size_t index_compact = high_compact * COMPACT_FACTOR + low_in;

  _compact_bits->add(index_compact);
}

void CompactBitSet::allocate()
{
  assert(_high_bits);
  assert(!_high_index);
  assert(!_compact_bits);

  _high_index = new VectorBitSetIndex(*_high_bits);

  size_t compact_size = _high_bits->count() * COMPACT_FACTOR;
  if(compact_size > 0)
    {
      _compact_bits = (_filename != "") ? new VectorBitSet(_filename + "-compact", compact_size) : new VectorBitSet(compact_size);
    }
}

CompactBitSetIterator CompactBitSet::cbegin() const
{
  assert(_high_bits);

  auto high_iter = _high_bits->cbegin();
  if(!(high_iter < _high_bits->cend()))
    {
      return this->cend();
    }

  assert(_compact_bits);
  auto compact_iter = _compact_bits->cbegin();

  return CompactBitSetIterator(high_iter, compact_iter);
}

bool CompactBitSet::check(size_t index_in) const
{
  assert(index_in < _size);
  assert(_high_bits);
  assert(_high_index);

  size_t high_in = index_in / COMPACT_FACTOR;
  if(!_high_bits->check(high_in))
    {
      return false;
    }
  size_t low_in = index_in % COMPACT_FACTOR;

  size_t high_compact = _high_index->rank(high_in);
  size_t index_compact = high_compact * COMPACT_FACTOR + low_in;

  assert(_compact_bits);
  return _compact_bits->check(index_compact);
}

CompactBitSetIterator CompactBitSet::cend() const
{
  assert(_high_bits);
  assert(_compact_bits);

  return CompactBitSetIterator(_high_bits->cend(), _compact_bits->cend());
}

size_t CompactBitSet::count() const
{
  if(_compact_bits)
    {
      return _compact_bits->count();
    }
  else
    {
      assert(_high_bits->count() == 0);
      return 0;
    }
}

void CompactBitSet::prepare(size_t index_in)
{
  assert(_high_bits);
  assert(!_high_index);
  assert(!_compact_bits);

  size_t high_in = index_in / COMPACT_FACTOR;
  _high_bits->add(high_in);
}

size_t CompactBitSet::size() const
{
  return _size;
}

CompactBitSetIndex::CompactBitSetIndex(const CompactBitSet& bitset_in)
  : _bitset(bitset_in),
    _compact_index(*(_bitset._compact_bits))
{
}

size_t CompactBitSetIndex::rank(size_t index_in) const
{
  assert(index_in < _bitset.size());
  assert(_bitset._high_bits);
  assert(_bitset._high_index);

  size_t high_in = index_in / COMPACT_FACTOR;
  if(!_bitset._high_bits->check(high_in))
    {
      assert(0);
    }
  size_t low_in = index_in % COMPACT_FACTOR;

  size_t high_compact = _bitset._high_index->rank(high_in);
  size_t index_compact = high_compact * COMPACT_FACTOR + low_in;

  return _compact_index.rank(index_compact);
}

CompactBitSetIterator::CompactBitSetIterator(VectorBitSetIterator high_iter_in, VectorBitSetIterator compact_iter_in)
  : _high_iter(high_iter_in),
    _compact_iter(compact_iter_in)
{
}

size_t CompactBitSetIterator::operator*() const
{
  size_t high_index = *_high_iter;

  size_t compact_index = *_compact_iter;
  size_t low_index = compact_index % COMPACT_FACTOR;

  return high_index * COMPACT_FACTOR + low_index;
}

CompactBitSetIterator& CompactBitSetIterator::operator++()
{
  // prefix ++

  // snapshot high bits before advancing
  size_t index_old = *_compact_iter;
  size_t high_old = index_old / COMPACT_FACTOR;

  // advance
  ++_compact_iter;

  // check if high bits changed and advance other iter if needed.
  size_t index_new = *_compact_iter;
  size_t high_new = index_new / COMPACT_FACTOR;

  if(high_new != high_old)
    {
      ++_high_iter;
    }

  // done

  return *this;
}

bool CompactBitSetIterator::operator<(const CompactBitSetIterator& right_in) const
{
  return this->_compact_iter < right_in._compact_iter;
}

