// Profile.cpp

#include "Profile.h"

#include <algorithm>
#include <cassert>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

#include <sys/resource.h>
#include <unistd.h>

#include <nlohmann/json.hpp>

// Optional trace file: set DFA_PROFILE_TRACE=<path> to append one JSON
// object per line for each phase that took at least trace_min_us, written
// when the phase ends, plus a "run" header per process. Unlike the "took"
// lines on stdout it has microsecond times, explicit parent ids and CPU
// time, and unlike the totals printed at exit it survives the process
// being killed: each line is flushed as it is written, so the file is valid
// up to its last line. Only phases still open at that point are missing.
//
// Each record carries inclusive wall and CPU time, and self time: the part
// spent while this phase was on top of the Profile stack. Self time is
// tracked here rather than left to subtraction by a reader because phases
// do not always nest in time -- load_or_build's build callback can tic an
// enclosing Profile (MoveGraph's get_moves does), so a child phase can
// outlive the parent phase it started in.
//
// The threshold drops ~99% of records (mostly tiny MemoryMaps). Self time
// of an unrecorded phase is added to the enclosing phase, and any phase
// with a recorded child is recorded regardless of length, so the tree
// stays connected and self times still sum to the whole. Any failure to
// write the file throws rather than dropping records, since a trace with
// silent gaps would mislead whoever reads it.

namespace
{
  const int64_t trace_min_us = 10000;

  // Taken during this file's static initialization. Nothing in this
  // codebase creates a Profile that early, so start_us is non-negative.
  const auto trace_origin = std::chrono::steady_clock::now();
  const auto trace_origin_epoch = std::chrono::system_clock::now();

  struct ProfileTrace
  {
    std::string path;
    std::ofstream out;
    uint64_t next_id = 0;

    // when the top of the Profile stack last changed or ticked
    std::chrono::time_point<std::chrono::steady_clock> switch_time = trace_origin;
    ProfileUsage switch_usage;
  };

  void trace_write(ProfileTrace& trace, const nlohmann::json& record)
  {
    trace.out << record.dump() << '\n';
    trace.out.flush();
    if(!trace.out)
      {
        throw std::runtime_error("failed writing profile trace " + trace.path);
      }
  }

  ProfileTrace *trace_open()
  {
    const char *path = std::getenv("DFA_PROFILE_TRACE");
    if(!path)
      {
        return nullptr;
      }
    if(!*path)
      {
        throw std::runtime_error("DFA_PROFILE_TRACE is set but empty");
      }

    // Deliberately leaked so it outlives every other static, including
    // any Profile still open during static destruction.
    ProfileTrace *trace = new ProfileTrace;
    trace->path = path;
    trace->out.open(trace->path, std::ios::app);
    if(!trace->out)
      {
        throw std::runtime_error("failed opening profile trace " + trace->path);
      }

    char host[256] = {0};
    if(gethostname(host, sizeof(host) - 1) != 0)
      {
        throw std::runtime_error("gethostname failed while opening profile trace");
      }

    auto epoch = trace_origin_epoch.time_since_epoch();
    trace_write(*trace, {{"t", "run"},
                         {"pid", int64_t(getpid())},
                         {"host", host},
                         {"start_epoch_ms", int64_t(std::chrono::duration_cast<std::chrono::milliseconds>(epoch).count())},
                         {"hardware_concurrency", std::thread::hardware_concurrency()},
                         {"min_us", trace_min_us}});
    return trace;
  }

  ProfileTrace *trace_get()
  {
    static ProfileTrace *trace = trace_open();
    return trace;
  }

  ProfileUsage trace_usage()
  {
    struct rusage usage;
    if(getrusage(RUSAGE_SELF, &usage) != 0)
      {
        throw std::runtime_error("getrusage failed for profile trace");
      }

    ProfileUsage output;
    output.user_us = int64_t(usage.ru_utime.tv_sec) * 1000000 + int64_t(usage.ru_utime.tv_usec);
    output.sys_us = int64_t(usage.ru_stime.tv_sec) * 1000000 + int64_t(usage.ru_stime.tv_usec);
    output.majflt = int64_t(usage.ru_majflt);
    output.inblock = int64_t(usage.ru_inblock);
    output.oublock = int64_t(usage.ru_oublock);
    return output;
  }

  int64_t trace_us(std::chrono::steady_clock::duration duration)
  {
    return int64_t(std::chrono::duration_cast<std::chrono::microseconds>(duration).count());
  }

  // Profile's stacks are unsynchronized, so every Profile must live on one
  // thread. A Profile created inside a parallel region would interleave
  // with the main thread's and corrupt nesting for everything after it.
  void check_thread()
  {
    static const std::thread::id owner = std::this_thread::get_id();
    assert(std::this_thread::get_id() == owner);
  }
}

static std::vector<Profile *> profile_stack;
static std::vector<std::string> profile_label_stack;

static std::vector<std::string> profile_total_order;
static std::map<std::string, std::chrono::milliseconds> profile_totals;

Profile::Profile(std::string name_in)
  : name(name_in),
    last_label(""),
    last_time(std::chrono::steady_clock::now()),
    prefix("")
{
  check_thread();

  if(trace_get())
    {
      trace_switch(last_time, trace_usage());
    }

  profile_stack.push_back(this);

  assert(profile_label_stack.size() % 2 == 0);

  push_label_suffix(name);
  push_label_suffix(last_label);

  trace_begin();
}

Profile::Profile(const std::ostringstream& oss)
  : Profile(oss.str())
{
  // Delegating to Profile(std::string) above already pushed this once;
  // pushing again here left a second, dangling entry in profile_stack
  // once this object was destroyed (the destructor only pops once),
  // corrupting bookkeeping for every later Profile on the stack. Currently
  // unused by any caller in this codebase, so nothing exercises it, but
  // fixing a latent double-push in a currently-dead overload beats leaving
  // it for whoever reaches for it next.
}

Profile::~Profile()
{
  // ends the last phase without opening a "done" phase in the trace
  tic_impl("done", false);

  assert(profile_stack.back() == this);
  profile_stack.pop_back();

  profile_label_stack.pop_back();
  profile_label_stack.pop_back();
}

void Profile::push_label_suffix(std::string suffix_in)
{
  if(suffix_in == "")
    {
      // reuse previous label when suffix is empty
      assert(profile_label_stack.size() > 0);
      profile_label_stack.push_back(profile_label_stack.back());
      return;
    }

  std::string total_label = ((profile_label_stack.size() == 0) ?
			     suffix_in :
			     profile_label_stack.back() + " / " + suffix_in);

  auto search = profile_totals.find(total_label);
  if(search == profile_totals.end())
    {
      profile_total_order.push_back(total_label);
      profile_totals[total_label] = std::chrono::milliseconds(0);
    }

  profile_label_stack.push_back(total_label);
}

void Profile::set_prefix(std::string prefix_in)
{
  tic("set_prefix");
  prefix = prefix_in;
}

void Profile::tic(std::string label_in)
{
  tic_impl(label_in, true);
}

void Profile::tic_impl(std::string label_in, bool begin_next)
{
  check_thread();

  auto finish_time = std::chrono::steady_clock::now();
  if(trace_get())
    {
      ProfileUsage finish_usage = trace_usage();
      trace_switch(finish_time, finish_usage);
      trace_end(finish_time, finish_usage);
    }

  auto diff = finish_time - last_time;
  auto second = std::chrono::seconds(1);
  if(diff > second)
    {
      auto diff_seconds = std::chrono::duration_cast<std::chrono::seconds>(diff);
      std::cout << name << " ";
      if(prefix != "")
	{
	  std::cout << prefix << " ";
	}
      std::cout << last_label << " took " << diff_seconds.count() << "s" << std::endl;
    }

  assert(profile_label_stack.size() > 0);
  std::string total_label = profile_label_stack.back();
  auto diff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(diff);

  profile_totals[total_label] = profile_totals[total_label] + diff_ms;

  // update label

  last_label = label_in;
  last_time = std::chrono::steady_clock::now();

  profile_label_stack.pop_back();
  push_label_suffix(last_label);

  if(begin_next)
    {
      trace_begin();
    }
}

void Profile::trace_begin()
{
  ProfileTrace *trace = trace_get();
  if(!trace)
    {
      return;
    }

  // ids are assigned at the start so children can name their parent
  phase_id = ++(trace->next_id);
  phase_usage = trace_usage();
  self_us = 0;
  self_user_us = 0;
  self_sys_us = 0;
  phase_has_recorded_child = false;
}

// Credit the time since the last switch to whichever phase is on top of the
// stack, then start a new interval.
void Profile::trace_switch(std::chrono::time_point<std::chrono::steady_clock> now,
                           const ProfileUsage& usage)
{
  ProfileTrace *trace = trace_get();
  assert(trace);

  if(profile_stack.size() > 0)
    {
      Profile *top = profile_stack.back();
      top->self_us += trace_us(now - trace->switch_time);
      top->self_user_us += usage.user_us - trace->switch_usage.user_us;
      top->self_sys_us += usage.sys_us - trace->switch_usage.sys_us;
    }

  trace->switch_time = now;
  trace->switch_usage = usage;
}

// The Profile below this one on the stack, whose current phase encloses
// this one's, or null at the bottom.
Profile *Profile::trace_enclosing() const
{
  auto self = std::find(profile_stack.rbegin(), profile_stack.rend(), this);
  assert(self != profile_stack.rend());
  auto enclosing = std::next(self);
  return (enclosing == profile_stack.rend()) ? nullptr : *enclosing;
}

void Profile::trace_end(std::chrono::time_point<std::chrono::steady_clock> finish_time,
                        const ProfileUsage& usage)
{
  ProfileTrace *trace = trace_get();
  assert(trace);
  assert(phase_id > 0);

  Profile *enclosing = trace_enclosing();

  int64_t wall_us = trace_us(finish_time - last_time);
  if((wall_us < trace_min_us) && !phase_has_recorded_child)
    {
      // too short to record: its self time becomes the enclosing phase's
      if(enclosing)
        {
          enclosing->self_us += self_us;
          enclosing->self_user_us += self_user_us;
          enclosing->self_sys_us += self_sys_us;
        }
      return;
    }

  if(enclosing)
    {
      enclosing->phase_has_recorded_child = true;
    }
  uint64_t parent_id = enclosing ? enclosing->phase_id : 0;

  trace_write(*trace, {{"t", "phase"},
                       {"id", phase_id},
                       {"parent", parent_id},
                       {"name", name},
                       {"prefix", prefix},
                       {"label", last_label},
                       {"start_us", trace_us(last_time - trace_origin)},
                       {"wall_us", wall_us},
                       {"self_us", self_us},
                       {"self_user_us", self_user_us},
                       {"self_sys_us", self_sys_us},
                       {"user_us", usage.user_us - phase_usage.user_us},
                       {"sys_us", usage.sys_us - phase_usage.sys_us},
                       {"majflt", usage.majflt - phase_usage.majflt},
                       {"inblock", usage.inblock - phase_usage.inblock},
                       {"oublock", usage.oublock - phase_usage.oublock}});
}

// static object to output profile totals on shutdown

class ProfileReport
{
public:
  ~ProfileReport();
};

ProfileReport::~ProfileReport()
{
  for(std::string label : profile_total_order)
    {
      auto total_ms = profile_totals[label];
      auto total_s = std::chrono::duration_cast<std::chrono::seconds>(total_ms);

      if(total_s.count() > 0)
	{
	  std::cout << "Profile : " << label << " = " << total_s.count() << std::endl;
	}
    }
}

static ProfileReport profile_report;
