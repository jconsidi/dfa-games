// Profile.h

#ifndef PROFILE_H
#define PROFILE_H

#include <chrono>
#include <string>

class Profile
{
  std::string name;
  std::string last_label;
  std::chrono::time_point<std::chrono::steady_clock> last_time;

 public:

  Profile(std::string);
  ~Profile();

  void tic(std::string);
};

#endif
