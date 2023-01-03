// IntersectionManager.cpp

#include "IntersectionManager.h"

#include "CountManager.h"
#include "DFAUtil.h"

// invariant: output_trace[i] = intersection(input_trace[0] .. input_trace[i])

static CountManager count_manager("IntersectionManager");

IntersectionManager::IntersectionManager(const dfa_shape_t& shape_in)
  : shape(shape_in)
{
}

shared_dfa_ptr IntersectionManager::intersect(shared_dfa_ptr left_in, shared_dfa_ptr right_in)
{
  for(int i = 0; i < input_trace.size(); ++i)
    {
      if(output_trace[i] != left_in)
	{
	  continue;
	}

      // found left DFA. check if right DFA is next...

      if((i + 1) >= input_trace.size())
	{
	  // end of trace, so must have be working on a new one.
	  break;
	}
      else if(input_trace[i + 1] == right_in)
	{
	  // trace match continues, so we already calculated this
	  // intersection.
	  return output_trace[i + 1];
	}
      else
	{
	  // right DFA is not next, so clear rest of trace.
	  input_trace.resize(i + 1);
	  output_trace.resize(i + 1);
	  break;
	}
    }

  if((output_trace.size() > 0) && output_trace.back() != left_in)
    {
      // left DFA was not found, so we will start fresh traces.
      input_trace.resize(0);
      output_trace.resize(0);
    }

  if(input_trace.size() == 0)
    {
      // start new trace
      input_trace.push_back(left_in);
      output_trace.push_back(left_in);
    }

  // compute new intersection and add to end of trace
  input_trace.push_back(right_in);
  output_trace.push_back(DFAUtil::get_intersection(left_in, right_in));

  count_manager.inc("get_intersection");

  return output_trace.back();
}
