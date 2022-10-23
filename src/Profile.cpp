// Profile.cpp

#include "Profile.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <utility>
#include <vector>

static std::map<std::string, std::chrono::milliseconds> profile_totals;

Profile::Profile(std::string name_in)
  : name(name_in),
    last_label(""),
    last_time(std::chrono::steady_clock::now()),
    prefix("")
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

  auto diff_ms = std::chrono::duration_cast<std::chrono::milliseconds>(diff);
  auto search = profile_totals.find(last_label);
  if(search != profile_totals.end())
    {
      profile_totals[last_label] = search->second + diff_ms;
    }
  else
    {
      profile_totals[last_label] = diff_ms;
    }

#if 0
  std::cout << name << " " << label_in << " starting" << std::endl;
#endif
  last_label = label_in;
  last_time = std::chrono::steady_clock::now();
}

// static object to output profile totals on shutdown

class ProfileReport
{
public:
  ~ProfileReport();
};

ProfileReport::~ProfileReport()
{
  std::vector<std::string> all_labels;
  for(auto all_pair : profile_totals)
    {
      all_labels.push_back(std::get<0>(all_pair));
    }

  std::sort(all_labels.begin(), all_labels.end());

  for(std::string label : all_labels)
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
