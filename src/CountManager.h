// CountManager.h

#ifndef COUNT_MANAGER_H
#define COUNT_MANAGER_H

#include <map>
#include <string>

class CountManager
{
private:

  std::string manager_name;
  std::map<std::string, int> counts;

 public:

  CountManager(std::string);
  ~CountManager();

  void inc(std::string);
};

#endif
