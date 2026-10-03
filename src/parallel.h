// parallel.h

#ifndef PARALLEL_H
#define PARALLEL_H

// TRY_PARALLEL_* use par_unseq, which allows interleaving within a thread and
// so requires callables that neither allocate nor lock. TRY_PARALLEL_PAR_*
// use plain par for callables that do.

#ifdef __cpp_lib_parallel_algorithm
#include <charconv>
#include <cstdlib>
#include <execution>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>

#include <tbb/global_control.h>

// GCC's parallel STL algorithms (enabled by __cpp_lib_parallel_algorithm)
// dispatch through TBB's global thread pool, which by default sizes itself
// to every core on the machine. On a shared cluster (e.g. BU's SCC under
// SGE/UGE) that ignores the job's actual core grant unless something caps
// it, so every TRY_PARALLEL_* call below runs this once, before its first
// parallel dispatch, to cap the pool at NSLOTS.

inline unsigned int parallel_max_threads_from_environment()
{
  const char *nslots = std::getenv("NSLOTS");
  if(nslots == nullptr)
    {
      unsigned int hardware_threads = std::thread::hardware_concurrency();
      return (hardware_threads > 0) ? hardware_threads : 1;
    }

  std::string_view nslots_view(nslots);
  unsigned int parsed = 0;
  auto [ptr, ec] = std::from_chars(nslots_view.data(), nslots_view.data() + nslots_view.size(), parsed);
  if((ec != std::errc()) || (ptr != nslots_view.data() + nslots_view.size()) || (parsed == 0))
    {
      throw std::runtime_error("NSLOTS environment variable must be a positive integer, got \"" + std::string(nslots_view) + "\"");
    }

  return parsed;
}

inline void ensure_parallel_capped()
{
  static const tbb::global_control parallel_control(tbb::global_control::max_allowed_parallelism, parallel_max_threads_from_environment());
  (void) parallel_control;
}

#define TRY_PARALLEL_PAR_3(f, a, b, c) (ensure_parallel_capped(), f(std::execution::par, a, b, c))
#define TRY_PARALLEL_2(f, a, b) (ensure_parallel_capped(), f(std::execution::par_unseq, a, b))
#define TRY_PARALLEL_3(f, a, b, c) (ensure_parallel_capped(), f(std::execution::par_unseq, a, b, c))
#define TRY_PARALLEL_4(f, a, b, c, d) (ensure_parallel_capped(), f(std::execution::par_unseq, a, b, c, d))
#define TRY_PARALLEL_5(f, a, b, c, d, e) (ensure_parallel_capped(), f(std::execution::par_unseq, a, b, c, d, e))
#define TRY_PARALLEL_6(f, a, b, c, d, e, g) (ensure_parallel_capped(), f(std::execution::par_unseq, a, b, c, d, e, g))
#else
#define TRY_PARALLEL_PAR_3(f, a, b, c) f(a, b, c)
#define TRY_PARALLEL_2(f, a, b) f(a, b)
#define TRY_PARALLEL_3(f, a, b, c) f(a, b, c)
#define TRY_PARALLEL_4(f, a, b, c, d) f(a, b, c, d)
#define TRY_PARALLEL_5(f, a, b, c, d, e) f(a, b, c, d, e)
#define TRY_PARALLEL_6(f, a, b, c, d, e, g) f(a, b, c, d, e, g)
#endif

#endif
