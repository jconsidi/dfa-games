// format_utils.h

#ifndef FORMAT_UTILS_H
#define FORMAT_UTILS_H

#include <cstdint>
#include <string>

// Decimal digits of n with a comma between each group of three, e.g.
// 3372170416 -> "3,372,170,416". Independent of the global locale.
inline std::string with_commas(uint64_t n)
{
  std::string digits = std::to_string(n);
  std::string output;
  output.reserve(digits.size() + digits.size() / 3);
  for(size_t i = 0; i < digits.size(); ++i)
    {
      if((i > 0) && ((digits.size() - i) % 3 == 0))
	{
	  output.push_back(',');
	}
      output.push_back(digits[i]);
    }
  return output;
}

#endif
