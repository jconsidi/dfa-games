// Flashsort.cpp

#include "Flashsort.h"

// flashsort in-situ permutation
// https://en.m.wikipedia.org/wiki/Flashsort

template<class T, class K>
void flashsort_permutation(T *begin, T *end, std::function<K(const T&)> key_func)
{
  if(begin + 1 >= end)
    {
      return;
    }

  // count occurrences of each key with extra space left at beginning

  std::vector<K> auxiliary;
  for(T *p = begin; p < end; ++p)
    {
      K key = key_func(*p);
      // extend counts as needed
      if(auxiliary.size() <= key + 1)
	{
	  auxiliary.insert(auxiliary.end(), key + 2 - auxiliary.size(), 0);
	}
      assert(key + 1 < auxiliary.size());

      auxiliary[key + 1] += 1;
    }

  // convert from counts to cumulative counts

  for(K k = 1; k < auxiliary.size(); ++k)
    {
      auxiliary[k] += auxiliary[k-1];
    }

  assert(auxiliary[auxiliary.size() - 1] == end - begin);

  // convert from cumulative counts to L vector pointing at last entry
  // that needs to be moved.

  for(K k = 1; k < auxiliary.size(); ++k)
    {
      if(auxiliary[k] > 0)
	{
	  --auxiliary[k];
	}
    }
  assert(auxiliary[auxiliary.size() - 1] + 1 == end - begin);

  // coarse sort to improve locality of bigger instances

  if(auxiliary.size() >= 1000000)
    {
      flashsort_permutation<T,K>(begin, end, [&](const T& v)
      {
	return key_func(v) / 1000;
      });
    }

  // in-situ permutation

  K c = 0;
  while(c + 1 < auxiliary.size())
    {
      size_t i = auxiliary[c + 1];
      assert(begin + i < end);
      if(i == auxiliary[c])
	{
	  ++c;
	  continue;
	}

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
	  --auxiliary[c + 1];
	  continue;
	}

      // need to shuffle keys

      T temp = begin[i];
      do
	{
	  T *next = begin + auxiliary[temp_key + 1]--;
	  std::swap(temp, *next);
	  temp_key = key_func(temp);
	}
      while(temp_key != c);

      begin[i] = temp;
    }
}

// template instantiations

#include "BinaryDFA.h"
#include "DFA.h"

template void flashsort_permutation<BinaryDFAForwardChild, dfa_state_t>(BinaryDFAForwardChild *begin, BinaryDFAForwardChild *end, std::function<dfa_state_t(const BinaryDFAForwardChild&)> key_func);
