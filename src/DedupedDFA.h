// DedupedDFA.h

#ifndef DEDUPED_DFA_H
#define DEDUPED_DFA_H

#include <map>

#include "DFA.h"

struct DFATransitionsCompare
{
  bool operator() (const DFATransitionsStaging& left, const DFATransitionsStaging& right) const
  {
    size_t left_num_transitions = left.size();
    size_t right_num_transitions = right.size();
    assert(left_num_transitions == right_num_transitions);

    if(left_num_transitions < right_num_transitions)
      {
	return true;
      }
    else if(left_num_transitions > right_num_transitions)
      {
	return false;
      }

    for(size_t i = 0; i < left_num_transitions; ++i)
      {
	if(left[i] < right[i])
	  {
	    return true;
	  }
	else if(left[i] > right[i])
	  {
	    return false;
	  }
      }

    return false;
  }
};

typedef std::map<DFATransitionsStaging, dfa_state_t, DFATransitionsCompare> DFATransitionsMap;

class DedupedDFA
  : public DFA
{
 private:

  DFATransitionsMap *state_lookup = 0;

 protected:

  DedupedDFA(const dfa_shape_t&);

  virtual dfa_state_t add_state(int, const DFATransitionsStaging&);
  virtual void set_initial_state(dfa_state_t);

 public:

    virtual ~DedupedDFA();
};

#endif
