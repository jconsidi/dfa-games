// test_scratch_config.cpp

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

#include "ScratchConfig.h"

static void unset_both()
{
  unsetenv("DFA_LOCAL_DIR");
  unsetenv("DFA_ARCHIVE_DIR");
}

static void test_defaults_to_scratch()
{
  unset_both();

  assert(ScratchConfig::get_local_dir() == "scratch");
  assert(ScratchConfig::get_archive_dir() == "scratch");
}

static void test_both_set_independently()
{
  unset_both();
  setenv("DFA_LOCAL_DIR", "/fast/local", 1);
  setenv("DFA_ARCHIVE_DIR", "/slow/archive", 1);

  assert(ScratchConfig::get_local_dir() == "/fast/local");
  assert(ScratchConfig::get_archive_dir() == "/slow/archive");

  unset_both();
}

static void test_both_set_to_the_same_path()
{
  unset_both();
  setenv("DFA_LOCAL_DIR", "/one/root", 1);
  setenv("DFA_ARCHIVE_DIR", "/one/root", 1);

  assert(ScratchConfig::get_local_dir() == "/one/root");
  assert(ScratchConfig::get_archive_dir() == "/one/root");

  unset_both();
}

static void test_only_local_set_is_an_error()
{
  unset_both();
  setenv("DFA_LOCAL_DIR", "/fast/local", 1);

  bool threw = false;
  try
    {
      ScratchConfig::get_archive_dir();
    }
  catch(const std::runtime_error&)
    {
      threw = true;
    }
  assert(threw);

  unset_both();
}

static void test_only_archive_set_is_an_error()
{
  unset_both();
  setenv("DFA_ARCHIVE_DIR", "/slow/archive", 1);

  bool threw = false;
  try
    {
      ScratchConfig::get_local_dir();
    }
  catch(const std::runtime_error&)
    {
      threw = true;
    }
  assert(threw);

  unset_both();
}

static void test_empty_value_is_an_error()
{
  unset_both();
  setenv("DFA_LOCAL_DIR", "", 1);

  bool threw = false;
  try
    {
      ScratchConfig::get_local_dir();
    }
  catch(const std::runtime_error&)
    {
      threw = true;
    }
  assert(threw);

  unset_both();
}

int main()
{
  try
    {
      test_defaults_to_scratch();
      test_both_set_independently();
      test_both_set_to_the_same_path();
      test_only_local_set_is_an_error();
      test_only_archive_set_is_an_error();
      test_empty_value_is_an_error();
    }
  catch(const std::exception& e)
    {
      std::cerr << e.what() << std::endl;
      std::cerr.flush();
      return 1;
    }

  return 0;
}
