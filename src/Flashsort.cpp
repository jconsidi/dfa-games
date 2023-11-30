// Flashsort.cpp

#include "Flashsort.h"

#include <cassert>

#include "Profile.h"

// flashsort in-situ permutation
// https://en.m.wikipedia.org/wiki/Flashsort

template<class T, class K>
std::vector<size_t> _flashsort_count_cumulative(T *begin, T *end, std::function<K(const T&)> key_func)
{
  std::vector<K> auxiliary = {0};

  for(T *p = begin; p < end; ++p)
    {
      K key = key_func(*p);
      // extend counts as needed
      if(auxiliary.size() <= key)
	{
	  auxiliary.insert(auxiliary.end(), key + 1 - auxiliary.size(), 0);
	}
      assert(key < auxiliary.size());

      auxiliary[key] += 1;
    }

  for(size_t k = 1; k < auxiliary.size(); ++k)
    {
      auxiliary[k] += auxiliary[k - 1];
    }

  return auxiliary;
}

template<class T, class K>
void _flashsort_permute(T *begin, T *end, std::function<K(const T&)> key_func, std::vector<size_t>& auxiliary)
{
  Profile profile("_flashsort_permute");

  if(auxiliary.size() == 1)
    {
      // only key value was zero
      return;
    }

  assert(auxiliary[auxiliary.size() - 1] == end - begin);

  if(auxiliary[auxiliary.size() - 2] == 0)
    {
      // only one key value
      return;
    }

  // coarse sort to improve locality of bigger instances

  if(auxiliary.size() >= 1000000)
    {
      profile.tic("recursion for locality");
      flashsort_permutation<T,K>(begin, end, [&](const T& v)
      {
	return key_func(v) / 1000;
      });
    }

  // in-situ permutation

  profile.tic("permutation");

  K c = 0;
  while(c < auxiliary.size())
    {
      if(auxiliary[c] == 0)
	{
	  // c is done and previous c values had no entries
	  ++c;
	  continue;
	}

      size_t i = auxiliary[c] - 1;
      assert(begin + i < end);

      K temp_key = key_func(begin[i]);
      if(temp_key < c)
	{
	  // previous buckets were already finished
	  ++c;
	  continue;
	}

      if(temp_key == c)
	{
	  // already in the right bucket
	  --auxiliary[c];
	  continue;
	}

      // need to shuffle keys

      T temp = begin[i];
      do
	{
	  size_t j = --auxiliary[temp_key];
	  assert(begin + j < end);

	  T *next = begin + j;
	  std::swap(temp, *next);
	  temp_key = key_func(temp);
	}
      while(temp_key != c);

      begin[i] = temp;
    }

#ifdef PARANOIA
  // sanity check
  for(T *p = begin; p < end; ++p)
    {
      if(p > begin)
	{
	  assert(key_func(*(p-1)) <= key_func(*p));
	}
    }
#endif
}

template<class T, class K>
std::vector<T *> flashsort_partition(T *begin, T *end, std::function<K(const T&)> key_func)
{
  // returns vector partitioning input range into different key
  // values. uses more working space than flashsort_permutation.

  Profile profile("flashsort_partition");

  assert(begin <= end);

  // count occurrences of each key

  std::vector<size_t> auxiliary = _flashsort_count_cumulative<T, K>(begin, end, key_func);

  // output prep

  std::vector<T *> output;
  output.reserve(auxiliary.size() + 1);
  output.push_back(begin);
  for(size_t i = 0; i < auxiliary.size(); ++i)
    {
      output.push_back(begin + auxiliary[i]);
    }
  assert(output.size() == auxiliary.size() + 1);
  assert(output.back() == end);

  // permute elements into correct partitions

  _flashsort_permute<T, K>(begin, end, key_func, auxiliary);

  return output;
}

template<class T, class K>
void flashsort_permutation(T *begin, T *end, std::function<K(const T&)> key_func)
{
  Profile profile("flashsort_permutation");

  if(begin + 1 >= end)
    {
      return;
    }

  // cumulative key counts

  profile.tic("auxiliary count");

  std::vector<size_t> auxiliary = _flashsort_count_cumulative<T, K>(begin, end, key_func);

  // permute elements into correct partitions

  _flashsort_permute(begin, end, key_func, auxiliary);
}

// template instantiations

template std::vector<size_t *> flashsort_partition(size_t *, size_t *, std::function<size_t(const size_t&)>);
