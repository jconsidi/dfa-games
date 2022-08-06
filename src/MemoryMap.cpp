// MemoryMap.cpp

#include "MemoryMap.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

#include <iostream>

template<class T>
MemoryMap<T>::MemoryMap(size_t size_in)
  : _size(size_in),
    _length(sizeof(T) * _size),
    _mapped(0)
{
  assert(size_in > 0);
  assert(_length / sizeof(T) == _size);

  this->mmap(MAP_ANONYMOUS, 0);
}

template<class T>
MemoryMap<T>::MemoryMap(std::string filename_in, size_t size_in)
  : _size(size_in),
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

  this->mmap(0, fildes);

  close(fildes);
}

template<class T>
MemoryMap<T>::MemoryMap(MemoryMap&& old)
  : _size(old._size),
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
  assert(_mapped);
  assert(i < _size);
  return ((T *) _mapped)[i];
}

template<class T>
T MemoryMap<T>::operator[](size_t i) const
{
  assert(_mapped);
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
void MemoryMap<T>::mmap(int flags, int fildes)
{
  assert(!_mapped);

  if(_length >> 30)
    {
      std::cout << "mmap length = " << (_length >> 30) << "GB" << std::endl;
    }

  _mapped = ::mmap(0, _length, PROT_READ | PROT_WRITE, MAP_SHARED | flags, fildes, 0);
  if(_mapped == MAP_FAILED)
    {
      _mapped = 0;
      throw std::logic_error("mmap failed");
    }
  assert(_mapped);
}

template<class T>
void MemoryMap<T>::munmap()
{
  assert(_mapped);

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
