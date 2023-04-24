// MemoryMap.cpp

#include "MemoryMap.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <iostream>

template<class T>
MemoryMap<T>::MemoryMap(size_t size_in)
  : _filename(),
    _flags(MAP_ANONYMOUS),
    _size(size_in),
    _length(sizeof(T) * _size),
    _mapped(0)
{
  assert(size_in > 0);
  assert(_length / sizeof(T) == _size);

  this->mmap(0);
}

template<class T>
MemoryMap<T>::MemoryMap(std::string filename_in)
  : _filename(filename_in),
    _flags(0),
    _size(0),
    _length(0),
    _mapped(0)
{
  this->mmap();
}

template<class T>
MemoryMap<T>::MemoryMap(std::string filename_in, size_t size_in)
  : _filename(filename_in),
    _flags(0),
    _size(size_in),
    _length(sizeof(T) * _size),
    _mapped(0)
{
  assert(size_in > 0);
  assert(_length / sizeof(T) == _size);

  int fildes = open(filename_in.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if(fildes == -1)
    {
      perror("open");
      throw std::logic_error("open() failed");
    }

  if(ftruncate(fildes, _length))
    {
      perror("ftruncate");
      throw std::logic_error("ftruncate() failed");
    }

  this->mmap(fildes);

  close(fildes);
}

template<class T>
MemoryMap<T>::MemoryMap(MemoryMap&& old)
  : _filename(old._filename),
    _flags(old._flags),
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

  _size = other._size;
  _length = other._length;
  _mapped = other._mapped;

  other._mapped = 0;

  return *this;
}

template<class T>
T& MemoryMap<T>::operator[](size_t i)
{
  this->mmap();
  assert(i < _size);
  return ((T *) _mapped)[i];
}

template<class T>
T MemoryMap<T>::operator[](size_t i) const
{
  this->mmap();
  assert(i < _size);
  return ((T *) _mapped)[i];
}

template<class T>
T *MemoryMap<T>::begin()
{
  return ((T *) _mapped);
}

template<class T>
T *MemoryMap<T>::end()
{
  return ((T *) _mapped) + _size;
}

template<class T>
void MemoryMap<T>::mmap() const
{
  if(_mapped)
    {
      return;
    }

  assert(_flags != MAP_ANONYMOUS);

  int fildes = open(_filename.c_str(), O_RDWR);
  if(fildes == -1)
    {
      std::cerr << "open " << _filename << " failed" << std::endl;
      perror("open");
      throw std::runtime_error("open() failed");
    }

  this->mmap(fildes);
  close(fildes);
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

      if(_length > 0)
	{
	  assert(stat_buffer.st_size == _length);
	  assert(_size == _length / sizeof(T));
	}
      else
	{
	  _length = stat_buffer.st_size;
	  _size = _length / sizeof(T);
	}
    }

  if(_length >> 30)
    {
      std::cout << "mmap length = " << (_length >> 30) << "GB" << std::endl;
    }

  _mapped = ::mmap(0, _length, PROT_READ | PROT_WRITE, MAP_SHARED | _flags, fildes, 0);
  if(_mapped == MAP_FAILED)
    {
      _mapped = 0;
      perror("mmap");
      throw std::logic_error("mmap failed");
    }
  assert(_mapped);
}

template<class T>
void MemoryMap<T>::munmap()
{
  if(!_mapped)
    {
      return;
    }

  if(::munmap(_mapped, _length))
    {
      throw std::logic_error("munmap failed");
    }

  _mapped = 0;
}

template<class T>
size_t MemoryMap<T>::length() const
{
  return _length;
}

template<class T>
size_t MemoryMap<T>::size() const
{
  return _size;
}

// template instantiations

#include "BinaryDFA.h"

template class MemoryMap<int>;
template class MemoryMap<size_t>;
template class MemoryMap<uint32_t>;
template class MemoryMap<uint64_t>;
