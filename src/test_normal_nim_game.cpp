// test_normal_nim_game.cpp

#include <iostream>

#include "NormalNimGame.h"
#include "test_utils.h"

template<int n, int... shape_pack>
int test()
{
  std::cout << "TESTING " << n << std::endl;

  NormalNimGame<n, shape_pack...> nim;

  // run generic tests

  test_backward(nim, n * 16, n % 2);

  return 0;
}

int main()
{
  test<NORMALNIM1_DFA_PARAMS>();
  test<NORMALNIM2_DFA_PARAMS>();
  test<NORMALNIM3_DFA_PARAMS>();
  test<NORMALNIM4_DFA_PARAMS>();

  return 0;
}
