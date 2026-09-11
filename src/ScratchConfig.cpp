// ScratchConfig.cpp

#include "ScratchConfig.h"

#include <cstdlib>
#include <stdexcept>

static const char *env_local = "DFA_LOCAL_DIR";
static const char *env_archive = "DFA_ARCHIVE_DIR";

// Returns the named variable's value, or throws if it is set to the empty
// string rather than silently treating that the same as unset.
static const char *getenv_nonempty(const char *name)
{
  const char *value = std::getenv(name);
  if(value && (value[0] == '\0'))
    {
      throw std::runtime_error(std::string(name) + " is set but empty");
    }

  return value;
}

std::string ScratchConfig::get_local_dir()
{
  const char *local = getenv_nonempty(env_local);
  if(local)
    {
      return std::string(local);
    }

  // Setting only one of the two is ambiguous rather than a default worth
  // guessing at: it could just as easily be a typo that was meant to keep
  // the two directories separate.
  if(getenv_nonempty(env_archive))
    {
      throw std::runtime_error(std::string(env_archive) + " is set but " +
				env_local + " is not; set both or neither");
    }

  return "scratch";
}

std::string ScratchConfig::get_archive_dir()
{
  const char *archive = getenv_nonempty(env_archive);
  if(archive)
    {
      return std::string(archive);
    }

  if(getenv_nonempty(env_local))
    {
      throw std::runtime_error(std::string(env_local) + " is set but " +
				env_archive + " is not; set both or neither");
    }

  return "scratch";
}
