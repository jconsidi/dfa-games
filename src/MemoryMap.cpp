// MemoryMap.cpp

#include "MemoryMap.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>

template<class T>
MemoryMap<T>::MemoryMap(std::string filename_in, size_t size_in)
  : _filename(filename_in),
    _size(size_in),
    _length(sizeof(T) * _size),
    _mapped(0)
{
  assert(_length / sizeof(T) == _size);
}

template<class T>
MemoryMap<T>::MemoryMap(MemoryMap&& old)
  : _filename(old._filename),
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
T& MemoryMap<T>::operator[](int i)
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
void MemoryMap<T>::mmap()
{
  assert(!_mapped);

  int fildes = open(_filename.c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
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

  _mapped = ::mmap(0, _length, PROT_READ | PROT_WRITE, MAP_SHARED, fildes, 0);
  if(_mapped == MAP_FAILED)
    {
      _mapped = 0;
      throw std::logic_error("mmap failed");
    }

  close(fildes);
}

template<class T>
void MemoryMap<T>::munmap()
{
  assert(_mapped);

  if(::munmap(_mapped, _length))
    {
      throw std::logic_error("munmap failed");
    }
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
