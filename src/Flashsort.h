// Flashsort.h

#ifndef FLASHSORT_H
#define FLASHSORT_H

#include <functional>

template<class T, class K>
  void flashsort_permutation(T *begin, T *end, std::function<K(const T&)> key_func);

#endif
