// MemoryMap.cpp

#include "MemoryMap.h"

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cassert>
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
  : _filename(filename_in),
    _flags(0),
    _readonly(true),
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

  const size_t chunk_bytes_max = size_t(4) << 30; // 4GB
  static_assert(chunk_bytes_max % sizeof(T) == 0);
  const size_t chunk_elements_max = chunk_bytes_max / sizeof(T);
  const size_t chunk_elements = std::min(size_in, chunk_elements_max);

  int fildes = open(O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);

  std::vector<T> chunk_buffer(chunk_elements);

  profile.tic("chunk iota");
  const std::vector<size_t>& chunk_iota = get_iota(chunk_elements);

  for(size_t chunk_start = 0; chunk_start < _size; chunk_start += chunk_elements)
    {
      size_t chunk_end = std::min(chunk_start + chunk_elements, _size);
      size_t chunk_size = chunk_end - chunk_start;

      profile.tic("chunk populate");
      TRY_PARALLEL_4(std::transform,
                     chunk_iota.begin(),
                     chunk_iota.begin() + chunk_size,
                     chunk_buffer.begin(),
                     [&](size_t i)
                     {
                       return populate_func(chunk_start + i);
                     });

      profile.tic("chunk write");
      write_buffer<T>(fildes, chunk_buffer.data(), chunk_end - chunk_start);
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

  if(_length >> 30)
    {
      std::cerr << "mmap length = " << (_length >> 30) << "GB" << std::endl;
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
void MemoryMap<T>::msync()
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
  munmap();

  _size = size_in;
  _length = _size * sizeof(T);

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
  munmap();

  if(::unlink(_filename.c_str()))
    {
      perror("unlink");
      throw std::runtime_error("unlink() failed");
    }
}

// template instantiations

#include "BinaryDFA.h"

template class MemoryMap<dfa_state_pair>;
template class MemoryMap<double>;
template class MemoryMap<int>;
template class MemoryMap<long long unsigned int>;
template class MemoryMap<long unsigned int>;
template class MemoryMap<unsigned int>;

template class MemoryMap<BinaryDFATransitionsHashPlusIndex>;
