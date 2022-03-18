// Flashsort.cpp

#include "Flashsort.h"

// flashsort in-situ permutation
// https://en.m.wikipedia.org/wiki/Flashsort

template<class T, class K>
void flashsort_permutation(T *begin, T *end, std::function<K(const T&)> key_func)
{
  // LATER: tighten this up to reduce space usage. the original
  // Flashsort publication only uses one auxiliary array.

  // count occurrences of each key

  std::vector<K> key_counts;
  for(T *p = begin; p < end; ++p)
    {
      K key = key_func(*p);
      // extend counts as needed
      if(key_counts.size() <= key)
	{
	  key_counts.insert(key_counts.end(), key + 1 - key_counts.size(), 0);
	}
      assert(key < key_counts.size());

      key_counts[key] += 1;
    }

  // calculate boundaries between key ranges

  std::vector<T *> key_edges(key_counts.size() + 1);
  key_edges[0] = begin;
  for(K k = 0; k < key_counts.size(); ++k)
    {
      key_edges[k + 1] = key_edges[k] + key_counts[k];
    }
  assert(key_edges[key_counts.size()] == end);

  // in-situ permutation

  std::vector<T *> todo(key_counts.size());
  for(K k = 0; k < key_counts.size(); ++k)
    {
      todo[k] = key_edges[k];
    }

  for(K k = 0; k < key_counts.size(); ++k)
    {
      for(T *p = todo[k]; p < key_edges[k+1]; ++p)
	{
	  K temp_key = key_func(*p);
	  if(temp_key == k)
	    {
	      // already in right range
	      continue;
	    }

	  T temp = *p;
	  do
	    {
	      T *next = todo[temp_key]++;
	      std::swap(temp, *next);
	      temp_key = key_func(temp);
	    }
	  while(temp_key != k);

	  *p = temp;
       }
    }
}

// template instantiations

#include "BinaryDFA.h"
#include "DFA.h"

template void flashsort_permutation<BinaryDFAForwardChild, dfa_state_t>(BinaryDFAForwardChild *begin, BinaryDFAForwardChild *end, std::function<dfa_state_t(const BinaryDFAForwardChild&)> key_func);
