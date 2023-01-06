// test_amazons_backward.cpp

#include "AmazonsGame.h"
#include "test_utils.h"

void test(int width, int height, bool initial_win_expected)
{
  AmazonsGame game(width, height);
  test_backward(game, width * height - 8, initial_win_expected);
}

int main()
{
  test(4, 4, false); /* easy */
  test(4, 5, true); /* Song 2014 */
  test(4, 6, true); /* Song 2014 */
  test(4, 7, true); /* Song 2014 */
  test(5, 4, true); /* Song 2014 */
  test(5, 5, true); /* Muller 2001 */
  test(5, 6, true); /* Song 2014 */
  test(5, 7, true); /* unknown */
  test(6, 4, false); /* Song 2014 */
  test(6, 6, true); /* unknown */
  test(8, 8, true); /* unknown */
  test(10, 10, true); /* unknown */

  return 0;
}
