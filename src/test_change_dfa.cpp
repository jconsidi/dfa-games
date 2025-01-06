// test_change_dfa.cpp

#include <iostream>
#include <sstream>
#include <stdexcept>

#include "AcceptDFA.h"
#include "ChangeDFA.h"
#include "DifferenceDFA.h"
#include "DFA.h"
#include "FixedDFA.h"
#include "RejectDFA.h"
#include "TestDFAParams.h"

void test_change(std::string test_name, const DFA& dfa_in, change_vector changes_in, const DFA& dfa_expected)
{
  const dfa_shape_t& shape = dfa_in.get_shape();
  int ndim = int(shape.size());

  std::ostringstream builder;
  builder << "[" << shape[0];
  for(int i = 1; i < ndim; ++i)
    {
      builder << "," << shape[i];
    }
  builder << "] " << test_name;
  std::string test_name_full = builder.str();

  std::cout << test_name_full << std::endl;

  ChangeDFA change_test(dfa_in, changes_in);
  std::cout << test_name_full << ": expected " << dfa_expected.size() << " matches, actually " << change_test.size() << " matches" << std::endl;

  DifferenceDFA extra_dfa(change_test, dfa_expected);
  double extra_size = extra_dfa.size();
  if(extra_size > 0)
    {
      std::cerr << test_name_full << ": change dfa has " << extra_size << " extra matches." << std::endl;
      throw std::logic_error("change failed with extra matches");
    }

  DifferenceDFA missing_dfa(dfa_expected, change_test);
  double missing_size = missing_dfa.size();
  if(missing_size > 0)
    {
      std::cerr << test_name_full << ": change dfa has " << missing_size << " missing matches." << std::endl;
      throw std::logic_error("change failed with missing matches");
    }
}

void test_suite(const dfa_shape_t& shape_in)
{
  int ndim = int(shape_in.size());

  AcceptDFA accept(shape_in);
  RejectDFA reject(shape_in);

  // identity rule
  change_vector identity(ndim);

  test_change("accept identity", accept, identity, accept);
  test_change("reject identity", reject, identity, reject);

  // layer 0 change

  if(accept.get_layer_shape(0) < 2)
    {
      // too small for this test
      return;
    }

  change_vector l0(ndim);
  l0[0] = change_type(0, 1);

  FixedDFA l0_expected(shape_in, 0, 1);

  test_change("accept l0", accept, l0, l0_expected);
  test_change("reject l0", reject, l0, reject);
}

int main()
{
  try
    {
      test_suite(dfa_shape_t({TEST1_DFA_SHAPE}));
      test_suite(dfa_shape_t({TEST2_DFA_SHAPE}));
      test_suite(dfa_shape_t({TEST3_DFA_SHAPE}));
      test_suite(dfa_shape_t({TEST4_DFA_SHAPE}));
      test_suite(dfa_shape_t({TEST5_DFA_SHAPE}));
    }
  catch(const std::logic_error& e)
    {
      std::cerr << e.what() << std::endl;
      std::cerr.flush();
      return 1;
    }

  return 0;
}
