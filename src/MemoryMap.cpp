// MemoryMap.cpp

#include "MemoryMap.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

template<class T>
MemoryMap<T>::MemoryMap(size_t size_in)
  : _size(size_in),
    _length(sizeof(T) * _size),
    _mapped(0)
{
  assert(_length / sizeof(T) == _size);

  this->mmap(MAP_ANONYMOUS, 0);
}

template<class T>
MemoryMap<T>::MemoryMap(std::string filename_in, size_t size_in)
  : _size(size_in),
    _length(sizeof(T) * _size),
    _mapped(0)
{
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
T& MemoryMap<T>::operator[](size_t i)
{
  assert(_mapped);
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

  _mapped = ::mmap(0, _length, PROT_READ | PROT_WRITE, MAP_SHARED | flags, fildes, 0);
  if(_mapped == MAP_FAILED)
    {
      _mapped = 0;
      throw std::logic_error("mmap failed");
    }
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

template class MemoryMap<BinaryDFAForwardChild>;
template class MemoryMap<int>;
template class MemoryMap<size_t>;