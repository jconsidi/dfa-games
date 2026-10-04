// Profile.h

#ifndef PROFILE_H
#define PROFILE_H

#include <chrono>
#include <cstdint>
#include <sstream>
#include <string>

// Process-wide resource counters at one instant, for the trace file's
// per-phase deltas. CPU time covers every thread, so user_us / wall_us is
// the average number of cores a phase kept busy.
struct ProfileUsage
{
  int64_t user_us = 0;
  int64_t sys_us = 0;
  int64_t majflt = 0;
  int64_t inblock = 0;
  int64_t oublock = 0;
};

class Profile
{
  std::string name;
  std::string last_label;
  std::chrono::time_point<std::chrono::steady_clock> last_time;
  std::string prefix;

  // Trace file state for the phase in progress; unused when
  // DFA_PROFILE_TRACE is not set. The self_* fields accumulate only time
  // during which this phase was on top of the Profile stack.
  uint64_t phase_id = 0;
  ProfileUsage phase_usage;
  int64_t self_us = 0;
  int64_t self_user_us = 0;
  int64_t self_sys_us = 0;
  bool phase_has_recorded_child = false;

  void push_label_suffix(std::string);
  void tic_impl(std::string, bool);
  void trace_begin();
  void trace_end(std::chrono::time_point<std::chrono::steady_clock>, const ProfileUsage&);
  Profile *trace_enclosing() const;
  static void trace_switch(std::chrono::time_point<std::chrono::steady_clock>, const ProfileUsage&);

 public:

  Profile(std::string);
  Profile(const std::ostringstream&);
  ~Profile();

  void set_prefix(std::string);
  void tic(std::string);
};

#endif
