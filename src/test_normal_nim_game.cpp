// test_normal_nim_game.cpp

#include <iostream>

#include "NormalNimGame.h"
#include "test_utils.h"

template<int n, class T>
int test()
{
  std::cout << "TESTING " << n << std::endl;

  T nim;

  // run generic tests

  test_backward(nim, n * 16, n % 2);

  return 0;
}

int main()
{
  test<1, NormalNim1Game>();
  test<2, NormalNim2Game>();
  test<3, NormalNim3Game>();
  test<4, NormalNim4Game>();

  return 0;
}
