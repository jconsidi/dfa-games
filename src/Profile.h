// Profile.h

#ifndef PROFILE_H
#define PROFILE_H

#include <chrono>
#include <sstream>
#include <string>

class Profile
{
  std::string name;
  std::string last_label;
  std::chrono::time_point<std::chrono::steady_clock> last_time;
  std::string prefix;

  void push_label_suffix(std::string);

 public:

  Profile(std::string);
  Profile(const std::ostringstream&);
  ~Profile();

  void set_prefix(std::string);
  void tic(std::string);
};

#endif
