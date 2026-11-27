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

  // the caller owns dfa_in and outlives the view
  ChangeDFA change_test(shared_dfa_ptr(&dfa_in, [](const DFA *) {}), changes_in);
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

  // last layer change, since that layer has the largest alphabet

  int last = ndim - 1;
  if(accept.get_layer_shape(last) < 2)
    {
      // too small for this test
      return;
    }

  change_vector change_last(ndim);
  change_last[last] = change_type(0, 1);

  FixedDFA last_input(shape_in, last, 0);
  FixedDFA last_expected(shape_in, last, 1);

  test_change("fixed last", last_input, change_last, last_expected);
  test_change("reject last", reject, change_last, reject);
}

// accept is not restricted to the before character at the last layer, so
// the change must throw. Run last, since a throw ends the program's
// building.
void test_unfiltered_throws(const dfa_shape_t& shape_in)
{
  int last = int(shape_in.size()) - 1;
  change_vector change_last(shape_in.size());
  change_last[last] = change_type(0, 1);

  AcceptDFA accept(shape_in);
  try
    {
      ChangeDFA change_test(shared_dfa_ptr(&accept, [](const DFA *) {}), change_last);
    }
  catch(const std::invalid_argument& e)
    {
      std::cout << "accept last: threw as expected: " << e.what() << std::endl;
      return;
    }
  throw std::logic_error("accept last: unfiltered input did not throw");
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
      test_unfiltered_throws(dfa_shape_t({TEST3_DFA_SHAPE}));
    }
  catch(const std::logic_error& e)
    {
      std::cerr << e.what() << std::endl;
      std::cerr.flush();
      return 1;
    }

  return 0;
}
