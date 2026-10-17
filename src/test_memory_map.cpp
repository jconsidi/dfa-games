// test_memory_map.cpp

#include <atomic>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <fcntl.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

#include "MemoryMap.h"
#include "utils.h"

static std::string test_dir;

static std::string test_path(const std::string& name)
{
  return test_dir + "/" + name;
}

static bool path_exists(const std::string& path)
{
  struct stat stat_buffer;
  if(::stat(path.c_str(), &stat_buffer) == 0)
    {
      return true;
    }
  if(errno != ENOENT)
    {
      throw std::runtime_error("stat " + path + " failed");
    }
  return false;
}

static off_t file_size(const std::string& path)
{
  struct stat stat_buffer;
  if(::stat(path.c_str(), &stat_buffer) != 0)
    {
      throw std::runtime_error("stat " + path + " failed");
    }
  return stat_buffer.st_size;
}

// Writes value i at element i through a fresh writable mapping, so every
// page of the file starts dirty.
static MemoryMap<uint64_t> make_dirty(const std::string& path, size_t n)
{
  MemoryMap<uint64_t> map(path, n);
  for(size_t i = 0; i < n; ++i)
    {
      map[i] = i;
    }
  return map;
}

static void check_prefix(const std::string& name, const std::string& path, size_t n)
{
  if(file_size(path) != off_t(n * sizeof(uint64_t)))
    {
      throw std::logic_error(name + ": " + path + " is " + std::to_string(file_size(path)) + " bytes, expected " + std::to_string(n * sizeof(uint64_t)));
    }
  if(n == 0)
    {
      return;
    }
  MemoryMap<uint64_t> reread(path);
  if(reread.size() != n)
    {
      throw std::logic_error(name + ": reread " + std::to_string(reread.size()) + " elements, expected " + std::to_string(n));
    }
  for(size_t i = 0; i < n; ++i)
    {
      if(reread[i] != i)
	{
	  throw std::logic_error(name + ": element " + std::to_string(i) + " is " + std::to_string(reread[i]));
	}
    }
}

static size_t test_truncate()
{
  size_t cases = 0;
  const size_t n = 1000000;
  for(bool mapped : {true, false})
    {
      for(size_t kept : {n, n / 3, size_t(1), size_t(0)})
	{
	  std::string name = std::string("truncate ") + (mapped ? "mapped" : "unmapped") + " to " + std::to_string(kept) + " of " + std::to_string(n);
	  std::string path = test_path("truncate");
	  MemoryMap<uint64_t> map = make_dirty(path, n);
	  if(!mapped)
	    {
	      map.munmap();
	    }
	  map.truncate(kept);
	  if(map.size() != kept)
	    {
	      throw std::logic_error(name + ": size " + std::to_string(map.size()));
	    }
	  check_prefix(name, path, kept);

	  // still usable after the truncate, as BinaryDFA's rename after it is
	  map.rename(test_path("truncate-renamed"));
	  check_prefix(name + " renamed", test_path("truncate-renamed"), kept);
	  map.unlink();
	  if(path_exists(test_path("truncate-renamed")))
	    {
	      throw std::logic_error(name + ": unlink left the file");
	    }
	  ++cases;
	}
    }
  return cases;
}

static size_t test_unlink()
{
  size_t cases = 0;
  const size_t n = 1000000;
  for(bool mapped : {true, false})
    {
      std::string name = std::string("unlink ") + (mapped ? "mapped" : "unmapped");

      // the only link
      {
	std::string path = test_path("unlink");
	MemoryMap<uint64_t> map = make_dirty(path, n);
	if(!mapped)
	  {
	    map.munmap();
	  }
	map.unlink();
	if(path_exists(path))
	  {
	    throw std::logic_error(name + ": unlink left the file");
	  }
	++cases;
      }

      // another link keeps every element, including the ones dirty at the
      // unlink
      {
	std::string path = test_path("unlink-linked");
	std::string other = test_path("unlink-other");
	MemoryMap<uint64_t> map = make_dirty(path, n);
	if(::link(path.c_str(), other.c_str()) != 0)
	  {
	    throw std::runtime_error(name + ": link failed");
	  }
	if(!mapped)
	  {
	    map.munmap();
	  }
	map.unlink();
	if(path_exists(path))
	  {
	    throw std::logic_error(name + " with another link: unlink left the file");
	  }
	check_prefix(name + " with another link", other, n);
	if(::unlink(other.c_str()) != 0)
	  {
	    throw std::runtime_error(name + ": unlink of the other link failed");
	  }
	++cases;
      }

      // read only
      {
	std::string path = test_path("unlink-readonly");
	{
	  MemoryMap<uint64_t> writer = make_dirty(path, n);
	}
	MemoryMap<uint64_t> reader(path);
	if(!mapped)
	  {
	    reader.munmap();
	  }
	reader.unlink();
	if(path_exists(path))
	  {
	    throw std::logic_error(name + " read only: unlink left the file");
	  }
	++cases;
      }
    }
  return cases;
}

// Throws unless f throws one of the exceptions what names.
template <class F>
static void require_throws(const std::string& what, F f)
{
  try
    {
      f();
    }
  catch(const std::exception& e)
    {
      std::cout << what << ": threw as expected: " << e.what() << std::endl;
      return;
    }
  throw std::logic_error(what + ": did not throw");
}

static int open_test_file(const std::string& path)
{
  int fd = ::open(path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0600);
  if(fd < 0)
    {
      throw std::runtime_error("open " + path + " failed");
    }
  return fd;
}

// pwrite_bytes and pread_bytes at offsets, with per call caps small enough
// that every transfer takes many calls, from threads writing disjoint
// ranges of one file, and their failures.
static size_t test_pread_pwrite()
{
  size_t cases = 0;
  std::string path = test_path("pread-pwrite");

  const size_t n = 100003;
  std::vector<unsigned char> pattern(n);
  for(size_t i = 0; i < n; ++i)
    {
      pattern[i] = (unsigned char)((i * 131 + 7) % 251);
    }

  for(size_t cap : {size_t(1), size_t(7), size_t(4096), size_t(INT_MAX)})
    {
      for(size_t offset : {size_t(0), size_t(3), size_t(65536)})
        {
          std::string name = "pread_pwrite cap " + std::to_string(cap) + " offset " + std::to_string(offset);
          int fd = open_test_file(path);
          size_t bytes = (cap == 1) ? 997 : n;
          pwrite_bytes(fd, pattern.data(), bytes, offset, cap);
          if(file_size(path) != off_t(offset + bytes))
            {
              throw std::logic_error(name + ": file is " + std::to_string(file_size(path)) + " bytes, expected " + std::to_string(offset + bytes));
            }
          std::vector<unsigned char> back(bytes);
          pread_bytes(fd, back.data(), bytes, offset, cap);
          for(size_t i = 0; i < bytes; ++i)
            {
              if(back[i] != pattern[i])
                {
                  throw std::logic_error(name + ": byte " + std::to_string(i) + " read back differs");
                }
            }
          // the hole before offset reads as zeros
          if(offset > 0)
            {
              std::vector<unsigned char> hole(offset, 1);
              pread_bytes(fd, hole.data(), offset, 0, cap);
              for(size_t i = 0; i < offset; ++i)
                {
                  if(hole[i] != 0)
                    {
                      throw std::logic_error(name + ": hole byte " + std::to_string(i) + " is not zero");
                    }
                }
            }
          ::close(fd);
          ++cases;
        }
    }

  // threads writing disjoint ranges of one descriptor, in interleaved order
  {
    int fd = open_test_file(path);
    const size_t threads = 8;
    const size_t piece = 1000;
    const size_t pieces = n / piece;
    std::atomic<size_t> next{0};
    std::vector<std::thread> workers;
    for(size_t t = 0; t < threads; ++t)
      {
        workers.emplace_back([&]()
        {
          for(size_t p = next++; p < pieces; p = next++)
            {
              size_t q = pieces - 1 - p;
              pwrite_bytes(fd, pattern.data() + q * piece, piece, q * piece, 333);
            }
        });
      }
    for(std::thread& worker : workers)
      {
        worker.join();
      }
    std::vector<unsigned char> back(pieces * piece);
    pread_bytes(fd, back.data(), back.size(), 0);
    for(size_t i = 0; i < back.size(); ++i)
      {
        if(back[i] != pattern[i])
          {
            throw std::logic_error("pread_pwrite threads: byte " + std::to_string(i) + " differs");
          }
      }
    ::close(fd);
    ++cases;
  }

  // failures
  {
    int fd = open_test_file(path);
    pwrite_bytes(fd, pattern.data(), 100, 0);
    std::vector<unsigned char> back(101);
    require_throws("pread_bytes past end of file", [&]()
    {
      pread_bytes(fd, back.data(), 101, 0, 7);
    });
    require_throws("pread_bytes at end of file", [&]()
    {
      pread_bytes(fd, back.data(), 1, 100);
    });
    require_throws("pwrite_bytes cap 0", [&]()
    {
      pwrite_bytes(fd, pattern.data(), 1, 0, 0);
    });
    require_throws("pread_bytes cap 0", [&]()
    {
      pread_bytes(fd, back.data(), 1, 0, 0);
    });
    ::close(fd);
    require_throws("pwrite_bytes closed descriptor", [&]()
    {
      pwrite_bytes(fd, pattern.data(), 1, 0);
    });
    require_throws("pread_bytes closed descriptor", [&]()
    {
      pread_bytes(fd, back.data(), 1, 0);
    });
    cases += 6;
  }

  if(::unlink(path.c_str()) != 0)
    {
      throw std::runtime_error("unlink " + path + " failed");
    }
  return cases;
}

static long blocks_written()
{
  struct rusage usage;
  if(getrusage(RUSAGE_SELF, &usage) != 0)
    {
      throw std::runtime_error("getrusage failed");
    }
  return usage.ru_oublock;
}

// Dirties a file of the given size through its mapping, then unlinks it,
// once flushing first as munmap used to and once with unlink alone.
static void run_bench_unlink(size_t megabytes)
{
  size_t n = megabytes * (size_t(1) << 20) / sizeof(uint64_t);
  for(bool flush_first : {true, false, true, false})
    {
      std::string path = test_path("bench");
      MemoryMap<uint64_t> map = make_dirty(path, n);

      long blocks_before = blocks_written();
      auto start = std::chrono::steady_clock::now();
      if(flush_first)
	{
	  map.msync();
	}
      map.unlink();
      double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
      long blocks = blocks_written() - blocks_before;

      std::cout << "bench_unlink " << megabytes << " MB " << (flush_first ? "flush then unlink" : "unlink") << ": " << seconds << "s, " << blocks << " blocks written" << std::endl;
    }
}

int main(int argc, char **argv)
{
  test_dir = (std::filesystem::temp_directory_path() / ("test_memory_map." + std::to_string(getpid()))).string();
  std::filesystem::create_directories(test_dir);

  int ret = 0;
  try
    {
      if((argc == 3) && (std::string(argv[1]) == "bench_unlink"))
	{
	  run_bench_unlink(std::stoull(argv[2]));
	}
      else if(argc == 1)
	{
	  size_t truncate_cases = test_truncate();
	  std::cout << "test_memory_map: " << truncate_cases << " truncate cases kept their prefix, reread and renamed" << std::endl;
	  size_t unlink_cases = test_unlink();
	  std::cout << "test_memory_map: " << unlink_cases << " unlink cases removed the file, and other links kept every element" << std::endl;
	  size_t io_cases = test_pread_pwrite();
	  std::cout << "test_memory_map: " << io_cases << " pread_bytes and pwrite_bytes cases read back what was written or threw" << std::endl;
	}
      else
	{
	  std::cerr << "usage: " << argv[0] << " [bench_unlink MEGABYTES]" << std::endl;
	  ret = 1;
	}
    }
  catch(const std::exception& e)
    {
      std::cerr << "test_memory_map: " << e.what() << std::endl;
      ret = 1;
    }

  std::filesystem::remove_all(test_dir);
  return ret;
}
