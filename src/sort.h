// sort.h

#ifndef SORT_H
#define SORT_H

#include <functional>

template <class T> void sort(T *, T *);
template <class T> void sort(T *, T *, std::function<int(T, T)>);

template <class T> T *sort_unique(T *, T *);

#endif
