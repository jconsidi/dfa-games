// MemoryMap.cpp

#include "MemoryMap.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <iostream>

#include "Profile.h"
#include "parallel.h"
#include "utils.h"

template<class T>
MemoryMap<T>::MemoryMap(size_t size_in)
  : _filename(),
    _flags(MAP_ANONYMOUS),
    _readonly(false),
    _size(size_in),
    _length(sizeof(T) * _size),
    _mapped(0)
{
  assert(_length / sizeof(T) == _size);

  if(size_in > 0)
    {
      this->mmap(0);
    }
}

template<class T>
MemoryMap<T>::MemoryMap(std::string filename_in)
  : MemoryMap(filename_in, true)
{
}

template<class T>
MemoryMap<T>::MemoryMap(std::string filename_in, bool readonly_in)
  : _filename(filename_in),
    _flags(0),
    _readonly(readonly_in),
    _size(0),
    _length(0),
    _mapped(0)
{
  int fildes = open(O_RDONLY, 0);

  struct stat stat_buffer;
  int stat_ret = fstat(fildes, &stat_buffer);
  if(stat_ret != 0)
    {
      perror("constructor stat");
      throw std::runtime_error("constructor stat failed");
    }
  if(stat_buffer.st_size % sizeof(T) != 0)
    {
      throw std::runtime_error("invalid length of file to mmap");
    }

  _length = stat_buffer.st_size;
  _size = _length / sizeof(T);

  this->mmap(fildes);

  int close_ret = close(fildes);
  if(close_ret != 0)
    {
      perror("constructor close");
      throw std::runtime_error("constructor close failed");
    }
}

template<class T>
MemoryMap<T>::MemoryMap(std::string filename_in, size_t size_in)
  : _filename(filename_in),
    _flags(0),
    _readonly(false),
    _size(size_in),
    _length(sizeof(T) * _size),
    _mapped(0)
{
  assert(_length / sizeof(T) == _size);

  int fildes = open(O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  ftruncate(fildes);
  this->mmap(fildes);

  int close_ret = close(fildes);
  if(close_ret != 0)
    {
      perror("constructor close");
      throw std::runtime_error("constructor close failed");
    }
}

template<class T>
MemoryMap<T>::MemoryMap(std::string filename_in, size_t size_in, std::function<T(size_t)> populate_func)
  : _filename(filename_in),
    _flags(0),
    _readonly(false),
    _size(size_in),
    _length(sizeof(T) * _size),
    _mapped(0)
{
  Profile profile("MemoryMap via populate_func");
  profile.tic("init");

  assert(_length / sizeof(T) == _size);

  int fildes = open(O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

  // The buffer holds whole elements from an allocation aligned for any
  // fundamental type, so out holds elements of T, as a mapping of the file
  // does.
  auto fill = [&](size_t begin, size_t n, uint8_t *out)
  {
    assert(reinterpret_cast<uintptr_t>(out) % alignof(T) == 0);
    T *elements = reinterpret_cast<T *>(out);
    for(size_t i = 0; i < n; ++i)
      {
        elements[i] = populate_func(begin + i);
      }
  };
  try
    {
      pwrite_filled(fildes, 0, _size, sizeof(T), fill, &profile);
    }
  catch(...)
    {
      ::close(fildes);
      throw;
    }

  profile.tic("truncate");
  ftruncate(fildes);

  profile.tic("mmap");
  this->mmap(fildes);

  profile.tic("close");
  int close_ret = close(fildes);
  if(close_ret != 0)
    {
      perror("constructor close");
      throw std::runtime_error("constructor close failed");
    }
}

template<class T>
MemoryMap<T>::MemoryMap(std::string filename_in, const std::vector<T>& buffer_in)
  : _filename(filename_in),
    _flags(0),
    _readonly(false),
    _size(buffer_in.size()),
    _length(sizeof(T) * _size),
    _mapped(0)
{
  int fildes = open(O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  write_buffer(fildes, buffer_in.data(), buffer_in.size());
  ftruncate(fildes);
  this->mmap(fildes);

  int close_ret = close(fildes);
  if(close_ret != 0)
    {
      perror("constructor close");
      throw std::runtime_error("constructor close failed");
    }
}

template<class T>
MemoryMap<T>::MemoryMap(MemoryMap&& old)
  : _filename(old._filename),
    _flags(old._flags),
    _readonly(old._readonly),
    _size(old._size),
    _length(old._length),
    _mapped(old._mapped)
{
  old._mapped = 0;
}

template<class T>
MemoryMap<T>::~MemoryMap()
{
  if(_mapped)
    {
      this->munmap();
    }
}

template<class T>
MemoryMap<T>& MemoryMap<T>::operator=(MemoryMap<T>&& other) noexcept
{
  if(this == &other)
    {
      return *this;
    }

  if(_mapped)
    {
      this->munmap();
    }

  _filename = other._filename;
  _flags = other._flags;
  _readonly = other._readonly;
  _size = other._size;
  _length = other._length;
  _mapped = other._mapped;

  other._mapped = 0;

  return *this;
}

template<class T>
T& MemoryMap<T>::operator[](size_t i)
{
  assert(i < _size);
  assert(_mapped);
  return ((T *) _mapped)[i];
}

template<class T>
const T& MemoryMap<T>::operator[](size_t i) const
{
  assert(i < _size);
  assert(_mapped);
  return ((T *) _mapped)[i];
}

template<class T>
T *MemoryMap<T>::begin()
{
  assert(_mapped);
  return ((T *) _mapped);
}

template<class T>
const T *MemoryMap<T>::begin() const
{
  assert(_mapped);
  return ((const T *) _mapped);
}

template<class T>
T *MemoryMap<T>::end()
{
  assert(_mapped);
  return ((T *) _mapped) + _size;
}

template<class T>
const T *MemoryMap<T>::end() const
{
  assert(_mapped);
  return ((const T *) _mapped) + _size;
}

template <class T>
void MemoryMap<T>::ftruncate(int fildes)
{
  if(::ftruncate(fildes, _length))
    {
      perror("ftruncate");
      throw std::runtime_error("ftruncate() failed");
    }
}

template<class T>
void MemoryMap<T>::mmap() const
{
  if(_mapped)
    {
      return;
    }

  if(_length == 0)
    {
      return;
    }

  assert(_filename != "");
  assert(_flags != MAP_ANONYMOUS);

  int fildes = open(_readonly ? O_RDONLY : O_RDWR, 0);
  this->mmap(fildes);

  int close_ret = close(fildes);
  if(close_ret != 0)
    {
      perror("mmap close");
      throw std::runtime_error("mmap close failed");
    }
}

template<class T>
void MemoryMap<T>::mmap(int fildes) const
{
  assert(!_mapped);

  if(fildes > 0)
    {
      struct stat stat_buffer;
      int stat_ret = fstat(fildes, &stat_buffer);
      if(stat_ret != 0)
	{
	  perror("MemoryMap stat");
	  throw std::runtime_error("MemoryMap stat failed");
	}
      if(stat_buffer.st_size % sizeof(T) != 0)
	{
	  throw std::runtime_error("invalid length of file to mmap");
	}

      assert(stat_buffer.st_size == _length);
      assert(_size == _length / sizeof(T));
    }

  if(_length == 0)
    {
      return;
    }

  int prot = PROT_READ;
  if(!_readonly)
    {
      prot |= PROT_WRITE;
    }

  _mapped = ::mmap(0, _length, prot, MAP_SHARED | _flags, fildes, 0);
  if(_mapped == MAP_FAILED)
    {
      _mapped = 0;
      perror("mmap");
      throw std::runtime_error("mmap failed");
    }
  assert(_mapped);
}

template<class T>
void MemoryMap<T>::msync() const
{
  if(!_mapped)
    {
      return;
    }

  int ret = ::msync(_mapped, _length, MS_SYNC);
  if(ret)
    {
      perror("msync");
      throw std::runtime_error("msync failed");
    }
}

template<class T>
void MemoryMap<T>::munmap() const
{
  if(!_mapped)
    {
      return;
    }

  if((_flags != MAP_ANONYMOUS) && !_readonly)
    {
      // munmap() does not imply a flush -- dirty pages of a MAP_SHARED
      // mapping stay in the page cache for the kernel's own writeback
      // schedule, same as an un-fsynced write(). A caller that removes the
      // underlying file soon after (staging cleanup, in particular) can
      // otherwise race that writeback on a networked filesystem. Read-only
      // and anonymous mappings have nothing dirty to flush, so this only
      // costs anything where flushing can actually matter.
      this->msync();
    }

  if(::munmap(_mapped, _length))
    {
      throw std::runtime_error("munmap failed");
    }

  _mapped = 0;
}

template<class T>
size_t MemoryMap<T>::length() const
{
  return _length;
}

template <class T>
int MemoryMap<T>::open(int flags, int mode) const
{
  assert(!_mapped);
  assert(_filename != "");

  int fildes = ::open(_filename.c_str(), flags, mode);
  if(fildes == -1)
    {
      if(errno != ENOENT)
	{
	  std::cerr << "open " << _filename << " failed" << std::endl;
	  perror("open");
	}
      throw std::runtime_error("open() failed");
    }

  return fildes;
}

template <class T>
void MemoryMap<T>::rename(std::string filename_in)
{
  munmap();

  int ret = ::rename(_filename.c_str(), filename_in.c_str());
  if(ret != 0)
    {
      std::cerr << "rename " << _filename << " to " << filename_in << " failed" << std::endl;
      perror("rename");
      throw std::runtime_error("rename() failed");
    }

  _filename = filename_in;
}

template<class T>
size_t MemoryMap<T>::size() const
{
  return _size;
}

template<class T>
void MemoryMap<T>::truncate(size_t size_in)
{
  size_t length_in = size_in * sizeof(T);

  if(_mapped && !_readonly && (length_in < _length))
    {
      // Shrink the file while it is still mapped, so the dirty pages past
      // the new end are dropped rather than flushed by munmap. Nothing
      // touches the mapping between the truncate and the munmap, so those
      // pages are never read past the end of the file. Only the kept
      // prefix is flushed.
      if(::truncate(_filename.c_str(), off_t(length_in)))
	{
	  perror("truncate");
	  throw std::runtime_error("truncate() failed");
	}

      if(length_in > 0)
	{
	  if(::msync(_mapped, length_in, MS_SYNC))
	    {
	      perror("msync");
	      throw std::runtime_error("msync failed");
	    }
	}

      if(::munmap(_mapped, _length))
	{
	  throw std::runtime_error("munmap failed");
	}
      _mapped = 0;

      _size = size_in;
      _length = length_in;
      return;
    }

  munmap();

  _size = size_in;
  _length = length_in;

  int fildes = open(O_RDWR, 0);
  ftruncate(fildes);

  int close_ret = close(fildes);
  if(close_ret != 0)
    {
      perror("constructor close");
      throw std::runtime_error("constructor close failed");
    }
}

template <class T>
void MemoryMap<T>::unlink()
{
  if(_flags & MAP_ANONYMOUS)
    {
      munmap();
      return;
    }

  // A file with no other link is truncated to nothing first, mapped or
  // not, so its dirty pages are dropped rather than flushed by munmap or
  // written back later. A file with other links keeps its contents for
  // them, so it is flushed and unmapped as usual.
  bool discard = false;
  if(!_readonly)
    {
      struct stat stat_buffer;
      if(::stat(_filename.c_str(), &stat_buffer))
	{
	  perror("unlink stat");
	  throw std::runtime_error("unlink stat failed");
	}
      discard = (stat_buffer.st_nlink == 1);
    }

  if(discard)
    {
      if(::truncate(_filename.c_str(), 0))
	{
	  perror("truncate");
	  throw std::runtime_error("truncate() failed");
	}

      if(_mapped)
	{
	  if(::munmap(_mapped, _length))
	    {
	      throw std::runtime_error("munmap failed");
	    }
	  _mapped = 0;
	}
    }
  else
    {
      munmap();
    }

  if(::unlink(_filename.c_str()))
    {
      perror("unlink");
      throw std::runtime_error("unlink() failed");
    }
}

// template instantiations

#include "BinaryDFA.h"
#include "flash_sort.h"

template class MemoryMap<double>;
template class MemoryMap<unsigned char>;
template class MemoryMap<int>;
template class MemoryMap<long long unsigned int>;
template class MemoryMap<long unsigned int>;
template class MemoryMap<unsigned int>;

template class MemoryMap<dfa_state_pair_t>;
template class MemoryMap<dfa_state_pair_wide_t>;
template class MemoryMap<BinaryDFATransitionsHashPlusIndex<false>>;
template class MemoryMap<BinaryDFATransitionsHashPlusIndex<true>>;
template class MemoryMap<dfa_pair_output<dfa_state_pair_t, uint32_t>>;
template class MemoryMap<dfa_pair_output<dfa_state_pair_t, uint64_t>>;
template class MemoryMap<dfa_pair_output<dfa_state_pair_wide_t, uint32_t>>;
template class MemoryMap<dfa_pair_output<dfa_state_pair_wide_t, uint64_t>>;

// BinaryDFA's row sort records: next ids at 4 or 8 bytes, shapes 1 to 4,
// pair ranks narrow or wide. Rank output records are the shape 1 ones.
#define MEMORY_MAP_ROW_RANK_RECORDS(id_t, rank_t)			\
  template class MemoryMap<RowRankRecord<id_t, 1, rank_t>>;		\
  template class MemoryMap<RowRankRecord<id_t, 2, rank_t>>;		\
  template class MemoryMap<RowRankRecord<id_t, 3, rank_t>>;		\
  template class MemoryMap<RowRankRecord<id_t, 4, rank_t>>;

MEMORY_MAP_ROW_RANK_RECORDS(uint32_t, uint32_t)
MEMORY_MAP_ROW_RANK_RECORDS(uint32_t, uint64_t)
MEMORY_MAP_ROW_RANK_RECORDS(uint64_t, uint32_t)
MEMORY_MAP_ROW_RANK_RECORDS(uint64_t, uint64_t)
