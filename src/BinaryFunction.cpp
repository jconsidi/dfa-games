// BinaryFunction.cpp

#include "BinaryFunction.h"

BinaryFunction::BinaryFunction(std::function<bool(bool, bool)> function_in)
  : function(function_in)
{
}

bool BinaryFunction::operator()(bool left_in, bool right_in) const
{
  return function(left_in, right_in);
}

dfa_state_t BinaryFunction::get_left_sink() const
{
  if(has_left_sink(0))
    {
      return 0;
    }

  if(has_left_sink(1))
    {
      return 1;
    }

  return ~dfa_state_t(0);
}

dfa_state_t BinaryFunction::get_right_sink() const
{
  if(has_right_sink(0))
    {
      return 0;
    }

  if(has_right_sink(1))
    {
      return 1;
    }

  return ~dfa_state_t(0);
}

bool BinaryFunction::has_left_sink(bool state) const
{
  return ((function(state, 0) == state) &&
	  (function(state, 1) == state));
}

bool BinaryFunction::has_right_sink(bool state) const
{
  return ((function(0, state) == state) &&
	  (function(1, state) == state));
}

bool BinaryFunction::is_commutative() const
{
  return function(0, 1) == function(1, 0);
}

