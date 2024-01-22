// parallel.h

#ifndef PARALLEL_H
#define PARALLEL_H

#ifdef __cpp_lib_parallel_algorithm
#define TRY_PARALLEL_2(f, a, b) f(std::execution::par_unseq, a, b)
#define TRY_PARALLEL_3(f, a, b, c) f(std::execution::par_unseq, a, b, c)
#define TRY_PARALLEL_4(f, a, b, c, d) f(std::execution::par_unseq, a, b, c, d)
#define TRY_PARALLEL_5(f, a, b, c, d, e) f(std::execution::par_unseq, a, b, c, d, e)
#define TRY_PARALLEL_6(f, a, b, c, d, e, g) f(std::execution::par_unseq, a, b, c, d, e, g)
#else
#define TRY_PARALLEL_2(f, a, b) f(a, b)
#define TRY_PARALLEL_3(f, a, b, c) f(a, b, c)
#define TRY_PARALLEL_4(f, a, b, c, d) f(a, b, c, d)
#define TRY_PARALLEL_5(f, a, b, c, d, e) f(a, b, c, d, e)
#define TRY_PARALLEL_6(f, a, b, c, d, e, g) f(a, b, c, d, e, g)
#endif

#endif
