// Profile.cpp

#include "Profile.h"

#include <chrono>
#include <iostream>

Profile::Profile(std::string name_in)
  : name(name_in),
    last_label(""),
    last_time(std::chrono::steady_clock::now())
{
}

Profile::Profile(const std::ostringstream& oss)
  : Profile(oss.str())
{
}

Profile::~Profile()
{
  tic("done");
}

void Profile::tic(std::string label_in)
{
  auto finish_time = std::chrono::steady_clock::now();
  auto diff = std::chrono::duration_cast<std::chrono::seconds>(finish_time - last_time);
  auto second = std::chrono::seconds(1);
  if(diff > second)
    {
      std::cout << name << " " << last_label << " took " << diff.count() << "s" << std::endl;
    }

#if 0
  std::cout << name << " " << label_in << " starting" << std::endl;
#endif
  last_label = label_in;
  last_time = std::chrono::steady_clock::now();
}
