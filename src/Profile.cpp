// Profile.cpp

#include "Profile.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <utility>
#include <vector>

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
  profile_stack.push_back(this);

  assert(profile_label_stack.size() % 2 == 0);

  push_label_suffix(name);
  push_label_suffix(last_label);
}

Profile::Profile(const std::ostringstream& oss)
  : Profile(oss.str())
{
  profile_stack.push_back(this);
}

Profile::~Profile()
{
  tic("done");

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
  prefix = prefix_in;
}

void Profile::tic(std::string label_in)
{
  auto finish_time = std::chrono::steady_clock::now();
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
