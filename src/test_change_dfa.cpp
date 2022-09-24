// test_change_dfa.cpp

#include <iostream>
#include <sstream>
#include <stdexcept>

#include "AcceptDFA.h"
#include "ChangeDFA.h"
#include "DifferenceDFA.h"
#include "DFA.h"
#include "DFAParams.h"
#include "FixedDFA.h"
#include "RejectDFA.h"

template<int ndim, int... shape_pack>
void test_change(std::string test_name, const DFA<ndim, shape_pack...>& dfa_in, change_func change_rule, const DFA<ndim, shape_pack...>& dfa_expected)
{
  int shape[] = {shape_pack...};
  std::ostringstream builder;
  builder << "[" << shape[0];
  for(int i = 1; i < ndim; ++i)
    {
      builder << "," << shape[i];
    }
  builder << "] " << test_name;
  std::string test_name_full = builder.str();

  std::cout << test_name_full << std::endl;

  ChangeDFA<ndim, shape_pack...> change_test(dfa_in, change_rule);
  std::cout << test_name_full << ": expected " << dfa_expected.size() << " matches, actually " << change_test.size() << " matches" << std::endl;

  DifferenceDFA<ndim, shape_pack...> extra_dfa(change_test, dfa_expected);
  double extra_size = extra_dfa.size();
  if(extra_size > 0)
    {
      std::cerr << test_name_full << ": change dfa has " << extra_size << " extra matches." << std::endl;
      throw std::logic_error("change failed with extra matches");
    }

  DifferenceDFA<ndim, shape_pack...> missing_dfa(dfa_expected, change_test);
  double missing_size = missing_dfa.size();
  if(missing_size > 0)
    {
      std::cerr << test_name_full << ": change dfa has " << missing_size << " missing matches." << std::endl;
      throw std::logic_error("change failed with missing matches");
    }
}

template<int ndim, int... shape_pack>
void test_suite()
{
  AcceptDFA<ndim, shape_pack...> accept;
  RejectDFA<ndim, shape_pack...> reject;

  // identity rule
  change_func identity = [](int layer, int before, int after)
  {
    return before == after;
  };

  test_change("accept identity", accept, identity, accept);
  test_change("reject identity", reject, identity, reject);

  // layer 0 change

  change_func l0 = [](int layer, int before, int after)
  {
    if(layer == 0)
      {
	return (before == 0) && (after == 1);
      }
    else
      {
	return before == after;
      }
  };

  FixedDFA<ndim, shape_pack...> l0_expected(0, 1);

  test_change("accept l0", accept, l0, l0_expected);
  test_change("reject l0", reject, l0, reject);
}

int main()
{
  try
    {
      test_suite<TEST1_DFA_PARAMS>();
      test_suite<TEST2_DFA_PARAMS>();
      test_suite<TEST3_DFA_PARAMS>();
      test_suite<TEST4_DFA_PARAMS>();
      test_suite<TEST5_DFA_PARAMS>();
      test_suite<TICTACTOE2_DFA_PARAMS>();
      test_suite<TICTACTOE3_DFA_PARAMS>();
      test_suite<TICTACTOE4_DFA_PARAMS>();
    }
  catch(std::logic_error e)
    {
      std::cerr << e.what() << std::endl;
      std::cerr.flush();
      return 1;
    }

  return 0;
}
