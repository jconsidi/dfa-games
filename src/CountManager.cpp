// CountManager.cpp

#include "CountManager.h"

#include <iostream>

CountManager::CountManager(std::string manager_name_in)
  : manager_name(manager_name_in)
{
}

CountManager::~CountManager()
{
  for(auto count_pair: counts)
    {
      std::cout << manager_name << " : " << count_pair.first << " count = " << count_pair.second << std::endl;
    }
}

void CountManager::inc(std::string count_name)
{
  auto search = counts.find(count_name);
  if(search != counts.end())
    {
      int old_count = search->second;
      counts[count_name] = old_count + 1;
    }
  else
    {
      counts[count_name] = 1;
    }
}
