// MemoryMap.h

#ifndef MEMORY_MAP_H
#define MEMORY_MAP_H

#include <cstdint>
#include <functional>
#include <string>

template<class T>
class MemoryMap
{
 private:
  std::string _filename;
  int _flags;
  bool _readonly;
  mutable size_t _size;
  mutable size_t _length;

  mutable void *_mapped;

  void ftruncate(int);
  void mmap(int) const;
  int open(int, int) const;

public:

  MemoryMap(size_t);
  MemoryMap(std::string);
  MemoryMap(std::string, size_t);
  MemoryMap(std::string, size_t, std::function<T(size_t)>);
  MemoryMap(const MemoryMap&) = delete;
  MemoryMap(MemoryMap&&);
  ~MemoryMap();

  MemoryMap& operator=(MemoryMap&&) noexcept;
  T& operator[](size_t);
  const T& operator[](size_t) const;

  T *begin();
  const T *begin() const;
  T *end();
  const T *end() const;
  size_t length() const;
  void mmap() const;
  void msync();
  void munmap() const;
  void rename(std::string);
  size_t size() const;
};

#endif
