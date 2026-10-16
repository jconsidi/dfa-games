// test_memory_map.cpp

#include <cerrno>
#include <chrono>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

#include "MemoryMap.h"

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
