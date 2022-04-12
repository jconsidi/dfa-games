// Flashsort.cpp

#include "Flashsort.h"

#include "Profile.h"

// flashsort in-situ permutation
// https://en.m.wikipedia.org/wiki/Flashsort

template<class T, class K>
void flashsort_permutation(T *begin, T *end, std::function<K(const T&)> key_func)
{
  Profile profile("flashsort_permutation");

  if(begin + 1 >= end)
    {
      return;
    }

  // count occurrences of each key

  profile.tic("auxiliary count");

  std::vector<K> auxiliary;
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
  assert(auxiliary.size() >= 1);

  if(auxiliary.size() == 1)
    {
      // only key value was zero
      return;
    }

  // convert from counts to cumulative counts

  profile.tic("auxiliary cumulative");

  for(K k = 1; k < auxiliary.size(); ++k)
    {
      auxiliary[k] += auxiliary[k-1];
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

// template instantiations
