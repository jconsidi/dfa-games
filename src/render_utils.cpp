// render_utils.cpp

#include "render_utils.h"

#include <fstream>
#include <stdexcept>

std::string script_safe(std::string json)
{
  for(size_t pos = json.find("</"); pos != std::string::npos; pos = json.find("</", pos + 3))
    {
      json.replace(pos, 2, "<\\/");
    }
  return json;
}

void replace_once(std::string& text, const std::string& placeholder, const std::string& value)
{
  size_t pos = text.find(placeholder);
  if((pos == std::string::npos) || (text.find(placeholder, pos + 1) != std::string::npos))
    {
      throw std::logic_error("page template must contain " + placeholder + " exactly once");
    }
  text.replace(pos, placeholder.size(), value);
}

void write_page(const std::string& output_path, const std::string& page)
{
  std::ofstream output(output_path, std::ios::binary);
  if(!output)
    {
      throw std::runtime_error("could not open " + output_path + " for writing");
    }
  output << page;
  output.close();
  if(!output)
    {
      throw std::runtime_error("error writing " + output_path);
    }
}
