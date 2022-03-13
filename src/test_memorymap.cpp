// test_memorymap.cpp

#include <iostream>
#include <string>

#include "BinaryDFA.h"
#include "MemoryMap.h"

int main()
{
  std::string filename = "/tmp/test-mm.bin";

  MemoryMap<int> a(filename, 10);
  a.mmap();
  for(int i = 0; i < 10; ++i)
    {
      a[i] = i;
    }

  for(int i = 0; i < 10; ++i)
    {
      assert(a[i] == i);
    }

  MemoryMap<int> b(filename, 10);
  b.mmap();
  for(int i = 0; i < 10; ++i)
    {
      b[i] = 10 - i;
    }

  for(int i = 0; i < 10; ++i)
    {
      assert(a[i] == 10 - i);
    }

  std::string filename2 = "/tmp/test-mm2.bin";
  MemoryMap<BinaryDFAForwardChild> c(filename2, 2);
  c.mmap();

  BinaryDFAForwardChild& first = c[0];
  BinaryDFAForwardChild& last = c[1];

  first.left = 0;
  first.right = 1;
  first.i = 2;
  first.j = 3;

  last.left = 4;
  last.right = 5;
  last.i = 6;
  last.j = 7;

  std::string filename3 = "/tmp/test-mm3.bin";
  MemoryMap<BinaryDFAForwardChild> big(filename3, 130114094ULL * 17ULL);

  return 0;
}
