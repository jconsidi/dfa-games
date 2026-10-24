// bucket_sort.cpp

#include <charconv>
#include <cstdlib>
#include <optional>

#include "bucket_sort.h"

#include "sort_utils.h"

size_t bucket_sort_memory_max = 0;
size_t bucket_sort_min_bytes = 0;

// A positive byte count from the environment variable name, or none if it
// is unset. Anything else throws.
static std::optional<size_t> env_bytes(const char *name)
{
  const char *value = std::getenv(name);
  if(!value)
    {
      return std::nullopt;
    }
  std::string value_string(value);
  size_t parsed = 0;
  auto [end, error] = std::from_chars(value_string.data(), value_string.data() + value_string.size(), parsed);
  if(value_string.empty() || (error != std::errc()) || (end != value_string.data() + value_string.size()) || (parsed == 0))
    {
      throw std::runtime_error(std::format("{:s}=\"{:s}\" must be a positive integer number of bytes", name, value_string));
    }
  return parsed;
}

size_t bucket_sort_memory_bytes()
{
  if(bucket_sort_memory_max > 0)
    {
      return bucket_sort_memory_max;
    }
  std::optional<size_t> env = env_bytes("DFA_BUCKET_SORT_MEMORY_MAX");
  return env ? *env : physical_memory_bytes() / 4;
}

size_t bucket_sort_cutoff_bytes()
{
  if(bucket_sort_min_bytes > 0)
    {
      return bucket_sort_min_bytes;
    }
  std::optional<size_t> env = env_bytes("DFA_BUCKET_SORT_MIN_BYTES");
  return env ? *env : physical_memory_bytes() / 10;
}

bool bucket_sort_out_of_core(size_t bytes)
{
  return bytes > bucket_sort_cutoff_bytes();
}

int bucket_digit_bit(int field_bits, uint64_t bound)
{
  if((field_bits <= 0) || (field_bits > 64))
    {
      throw std::invalid_argument(std::format("bucket_digit_bit: field_bits {:d} outside 1 to 64", field_bits));
    }
  if((field_bits < 64) && (bound > (uint64_t(1) << field_bits)))
    {
      throw std::invalid_argument(std::format("bucket_digit_bit: bound {:d} does not fit {:d} bits", bound, field_bits));
    }
  // values below bound use the low bit_width(bound - 1) bits of the field
  int used = (bound <= 1) ? 0 : int(std::bit_width(bound - 1));
  return field_bits - used;
}

void bucket_append_bytes(const std::string& path, const void *buffer, size_t bytes, size_t offset)
{
  int fd = ::open(path.c_str(), O_WRONLY | O_CREAT, 0600);
  if(fd < 0)
    {
      throw std::system_error(errno, std::generic_category(), "bucket_append_bytes: open " + path);
    }
  try
    {
      pwrite_bytes(fd, buffer, bytes, offset);
    }
  catch(...)
    {
      ::close(fd);
      throw;
    }
  if(::close(fd) != 0)
    {
      throw std::system_error(errno, std::generic_category(), "bucket_append_bytes: close " + path);
    }
}
