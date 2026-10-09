// test_sort_utils.cpp

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "BinaryDFA.h"
#include "sort_utils.h"

// Input patterns, generated from a size and a seed.
std::vector<uint64_t> make_input(const std::string& pattern, size_t n, uint64_t seed)
{
  std::mt19937_64 rng(seed);
  std::vector<uint64_t> output(n);
  for(size_t i = 0; i < n; ++i)
    {
      if(pattern == "random")
        {
          output[i] = rng();
        }
      else if(pattern == "sorted")
        {
          output[i] = i;
        }
      else if(pattern == "reverse")
        {
          output[i] = n - i;
        }
      else if(pattern == "equal")
        {
          output[i] = 42;
        }
      else if(pattern == "few")
        {
          output[i] = rng() % 3;
        }
      else if(pattern == "sawtooth")
        {
          output[i] = i % 17;
        }
      else
        {
          throw std::logic_error("unknown pattern " + pattern);
        }
    }
  return output;
}

const std::vector<std::string> patterns = {"random", "sorted", "reverse", "equal", "few", "sawtooth"};
const std::vector<size_t> sizes = {0, 1, 2, 3, 31, 32, 100, 1000, 100000};
const std::vector<size_t> thresholds = {0, 1, 2, 7, 64};

// Sorted output must match std::sort exactly for a type ordered on all of
// its bits.
size_t test_uint64()
{
  size_t cases = 0;
  for(const std::string& pattern : patterns)
    {
      for(size_t n : sizes)
        {
          for(size_t threshold : thresholds)
            {
              std::vector<uint64_t> expected = make_input(pattern, n, n + threshold);
              std::vector<uint64_t> actual = expected;

              std::sort(expected.begin(), expected.end());
              parallel_sort_in_place(actual.data(), actual.data() + actual.size(), threshold);

              if(actual != expected)
                {
                  throw std::logic_error("uint64 " + pattern + " n=" + std::to_string(n) + " threshold=" + std::to_string(threshold) + ": differs from std::sort");
                }
              ++cases;
            }
        }
    }
  return cases;
}

size_t test_pairs()
{
  size_t cases = 0;
  for(const std::string& pattern : patterns)
    {
      for(size_t n : sizes)
        {
          for(size_t threshold : thresholds)
            {
              std::vector<uint64_t> raw = make_input(pattern, n, 1000 + n + threshold);
              std::vector<dfa_state_pair_t> expected;
              expected.reserve(n);
              for(uint64_t v : raw)
                {
                  // high half and low half, so equal patterns give equal pairs
                  expected.emplace_back(dfa_state_t(v % 5), dfa_state_t(v >> 32));
                }
              std::vector<dfa_state_pair_t> actual = expected;

              std::sort(expected.begin(), expected.end());
              parallel_sort_in_place(actual.data(), actual.data() + actual.size(), threshold);

              if(actual != expected)
                {
                  throw std::logic_error("pair " + pattern + " n=" + std::to_string(n) + " threshold=" + std::to_string(threshold) + ": differs from std::sort");
                }
              ++cases;
            }
        }
    }
  return cases;
}

// The hash records compare on their key only, so the order among equal keys
// is unspecified. Check the keys come out sorted and the records are a
// permutation of the input.
template <bool wide>
size_t test_hash_records()
{
  typedef BinaryDFATransitionsHashPlusIndex<wide> record_t;

  auto by_all_words = [](const record_t& a, const record_t& b)
  {
    return std::lexicographical_compare(a.data, a.data + binary_dfa_hash_width, b.data, b.data + binary_dfa_hash_width);
  };

  size_t cases = 0;
  for(const std::string& pattern : patterns)
    {
      for(size_t n : sizes)
        {
          for(size_t threshold : thresholds)
            {
              std::vector<uint64_t> raw = make_input(pattern, n, 2000 + n + threshold);
              std::vector<record_t> input(n);
              for(size_t i = 0; i < n; ++i)
                {
                  dfa_state_t key[record_t::key_words] = {dfa_state_t(raw[i] % 7), dfa_state_t(raw[i] >> 40), dfa_state_t(raw[i] << 8)};
                  input[i].set(key, i);
                }
              std::vector<record_t> actual = input;

              parallel_sort_in_place(actual.data(), actual.data() + actual.size(), threshold);

              std::string name = std::string(wide ? "wide" : "narrow") + " hash records " + pattern + " n=" + std::to_string(n) + " threshold=" + std::to_string(threshold);
              if(!std::is_sorted(actual.begin(), actual.end()))
                {
                  throw std::logic_error(name + ": keys not sorted");
                }

              std::sort(input.begin(), input.end(), by_all_words);
              std::sort(actual.begin(), actual.end(), by_all_words);
              if(!std::equal(input.begin(), input.end(), actual.begin(), actual.end(), [](const record_t& a, const record_t& b)
              {
                return std::equal(a.data, a.data + binary_dfa_hash_width, b.data);
              }))
                {
                  throw std::logic_error(name + ": records are not a permutation of the input");
                }
              ++cases;
            }
        }
    }
  return cases;
}

int main()
{
  try
    {
      size_t cases = 0;
      cases += test_uint64();
      cases += test_pairs();
      cases += test_hash_records<false>();
      cases += test_hash_records<true>();
      std::cout << "parallel_sort_in_place: " << cases << " cases passed" << std::endl;
    }
  catch(const std::logic_error& e)
    {
      std::cerr << e.what() << std::endl;
      return 1;
    }

  return 0;
}
