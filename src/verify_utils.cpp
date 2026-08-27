// verify_utils.cpp

#include "verify_utils.h"

#include <stdexcept>

int verify_parse_side_to_move(std::string dfa_name)
{
  if(dfa_name.find(",side_to_move=0") != std::string::npos)
    {
      return 0;
    }

  if(dfa_name.find(",side_to_move=1") != std::string::npos)
    {
      return 1;
    }

  throw std::runtime_error("parsing side_to_move failed");
}
