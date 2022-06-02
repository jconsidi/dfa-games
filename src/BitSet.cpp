// BitSet.cpp

#include "BitSet.h"

#include <bit>

BitSet::BitSet(size_t size_in)
  : _size(size_in),
    _memory_map(new MemoryMap<uint64_t>((size_in + 63) / 64))
{
}

BitSet::BitSet(std::string filename_in, size_t size_in)
  : _size(size_in),
    _memory_map(new MemoryMap<uint64_t>(filename_in, (size_in + 63) / 64))
{
  size_t s = _memory_map->size();
  for(size_t i = 0; i < s; ++i)
    {
      (*_memory_map)[i] = 0;
    }
}

BitSet::BitSet(BitSet&& old)
  : _size(old._size),
    _memory_map(old._memory_map)
{
  old._memory_map = 0;
}

BitSet::~BitSet()
{
  if(_memory_map)
    {
      delete _memory_map;
      _memory_map = 0;
    }
}

void BitSet::add(size_t index_in)
{
  assert(index_in < _size);
  (*_memory_map)[index_in / 64] |= 1ULL << (index_in & 0x3fL);
}

BitSetIterator BitSet::cbegin() const
{
  for(size_t index_high = 0; index_high < _memory_map->size(); ++index_high)
    {
      uint64_t mask = (*_memory_map)[index_high];
      if(mask)
	{
	  return BitSetIterator(*this, index_high, std::countr_zero(mask));
	}
    }

  return cend();
}

bool BitSet::check(size_t index) const
{
  assert(index < _size);

  size_t index_high = index >> 6;
  size_t index_low = index & 0x3f;

  return ((*_memory_map)[index_high] & (1ULL << index_low)) != 0;
}

BitSetIterator BitSet::cend() const
{
  return BitSetIterator(*this, _memory_map->size(), 0ULL);
}

size_t BitSet::count() const
{
  size_t output = 0;
  for(size_t index_high = 0; index_high < _memory_map->size(); ++index_high)
    {
      output += std::popcount((*_memory_map)[index_high]);
    }

  return output;
}

size_t BitSet::size() const
{
  return _size;
}

BitSetIndex::BitSetIndex(const BitSet& vector_in)
  : _vector(vector_in),
    _index(vector_in._memory_map->size() + 1)
{
  auto mm = _vector._memory_map;

  _index[0] = 0;
  for(int i = 1; i <= mm->size(); ++i)
    {
      _index[i] = _index[i - 1] + std::popcount((*mm)[i - 1]);
    }
}

size_t BitSetIndex::operator[](size_t index) const
{
  auto mm = _vector._memory_map;

  size_t index_high = index >> 6;
  size_t index_low = index & 0x3f;

  auto output = _index[index_high] + std::popcount((*mm)[index_high] & ((1ULL << index_low) - 1));
  assert(output <= index);
  return output;
}

BitSetIterator::BitSetIterator(const BitSet& bit_vector_in, size_t index_high_in, size_t index_low_in)
  : _bit_vector(bit_vector_in),
    _index_high(index_high_in),
    _index_low(index_low_in)
{
}

size_t BitSetIterator::operator*() const
{
  return _index_high * 64 + _index_low;
}

BitSetIterator& BitSetIterator::operator++()
{
  // prefix ++

  auto mm = _bit_vector._memory_map;

  // check for another bit set in the same word
  auto rest_of_word = (*mm)[_index_high] >> _index_low >> 1; // shifting by 64 is a nop???
  if(rest_of_word)
    {
      _index_low += 1 + std::countr_zero(rest_of_word);
      assert(_index_low < 64);
      return *this;
    }

  // look for a later word with any bits set
  while(++_index_high < mm->size())
    {
      auto new_word = (*mm)[_index_high];
      if(new_word)
	{
	  _index_low = std::countr_zero(new_word);
	  assert(_index_low < 64);
	  return *this;
	}
    }

  // got to end without finding any more bits set
  _index_low = 0;
  return *this;
}

bool BitSetIterator::operator<(const BitSetIterator& right_in) const
{
  if(this->_index_high < right_in._index_high)
    {
      return true;
    }
  else if(this->_index_high == right_in._index_high)
    {
      return this->_index_low < right_in._index_low;
    }
  else
    {
      return false;
    }
}

