// parallel.h

#ifndef PARALLEL_H
#define PARALLEL_H

// TRY_PARALLEL_* use par, not par_unseq. par_unseq would forbid callables
// that allocate or lock, which is hard to audit across every caller, and in
// this build it bought nothing: libstdc++ expresses unseq as #pragma omp simd,
// which GCC ignores without -fopenmp or -fopenmp-simd.

#ifdef __cpp_lib_parallel_algorithm
#include <execution>
#define TRY_PARALLEL_2(f, a, b) f(std::execution::par, a, b)
#define TRY_PARALLEL_3(f, a, b, c) f(std::execution::par, a, b, c)
#define TRY_PARALLEL_4(f, a, b, c, d) f(std::execution::par, a, b, c, d)
#define TRY_PARALLEL_5(f, a, b, c, d, e) f(std::execution::par, a, b, c, d, e)
#define TRY_PARALLEL_6(f, a, b, c, d, e, g) f(std::execution::par, a, b, c, d, e, g)
#else
#define TRY_PARALLEL_2(f, a, b) f(a, b)
#define TRY_PARALLEL_3(f, a, b, c) f(a, b, c)
#define TRY_PARALLEL_4(f, a, b, c, d) f(a, b, c, d)
#define TRY_PARALLEL_5(f, a, b, c, d, e) f(a, b, c, d, e)
#define TRY_PARALLEL_6(f, a, b, c, d, e, g) f(a, b, c, d, e, g)
#endif

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <exception>
#include <mutex>
#include <numeric>
#include <vector>

// Calls f(i) for i in [0, n) in parallel. An exception escaping a parallel
// algorithm terminates the process, so the first one is kept and thrown
// once every call has returned.
template <class F>
void parallel_for(size_t n, F f)
{
  std::vector<size_t> indexes(n);
  std::iota(indexes.begin(), indexes.end(), size_t(0));
  std::atomic<bool> failed(false);
  std::exception_ptr failure;
  std::mutex failure_mutex;
  auto body = [&](size_t i)
  {
    if(failed)
      {
        return;
      }
    try
      {
        f(i);
      }
    catch(...)
      {
        std::lock_guard<std::mutex> lock(failure_mutex);
        if(!failure)
          {
            failure = std::current_exception();
          }
        failed = true;
      }
  };
  TRY_PARALLEL_3(std::for_each, indexes.begin(), indexes.end(), body);
  if(failure)
    {
      std::rethrow_exception(failure);
    }
}

#endif
