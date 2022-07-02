// DedupedDFA.h

#ifndef DEDUPED_DFA_H
#define DEDUPED_DFA_H

#include "DFA.h"

struct DFATransitionsCompare
{
  bool operator() (const DFATransitions& left, const DFATransitions& right) const
  {
    size_t left_num_transitions = left.size();
    size_t right_num_transitions = right.size();

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

typedef std::map<DFATransitions, dfa_state_t, DFATransitionsCompare> DFATransitionsMap;

template <int ndim, int... shape_pack>
class DedupedDFA
  : public DFA<ndim, shape_pack...>
{
 private:

  DFATransitionsMap *state_lookup = 0;

 protected:

  DedupedDFA();

  dfa_state_t add_state(int, const DFATransitions&);
  dfa_state_t add_state(int, std::function<dfa_state_t(int)>);
  virtual void set_initial_state(dfa_state_t);

 public:

    virtual ~DedupedDFA();
};

#endif
