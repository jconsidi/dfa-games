// verify_utils.cpp

#include "verify_utils.h"

#include <format>
#include <stdexcept>
#include <vector>

int verify_parse_side_to_move(std::string dfa_name)
{
  std::vector<std::string> templates = {
    ",side_to_move={:d}",
    ",side={:d}"
  };

  for(const std::string& t : templates)
    {
      for(int side_to_move = 0; side_to_move < 2; ++side_to_move)
        {
          if(dfa_name.find(std::vformat(t, std::make_format_args(side_to_move))) != std::string::npos)
            {
              return side_to_move;
            }
        }
    }

  throw std::runtime_error("parsing side_to_move failed");
}
