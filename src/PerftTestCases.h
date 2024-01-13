// PerftTestCases.h

#ifndef PERFT_TEST_CASES
#define PERFT_TEST_CASES

#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

typedef std::tuple<std::string, std::string, std::vector<uint64_t>> PerftTestCase;

extern const std::vector<PerftTestCase> perft_test_cases;

#endif
