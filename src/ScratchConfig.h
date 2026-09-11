// ScratchConfig.h

#ifndef SCRATCH_CONFIG_H
#define SCRATCH_CONFIG_H

#include <string>

// Resolves the two storage roots the rest of the codebase currently
// addresses as a single hardcoded "scratch/" tree:
//
// - the local directory, for ephemeral construction state and disposable
//   caches (per-DFA layer staging, BinaryDFA's working files, the op
//   caches, and eventually a local mirror of dfas_by_hash);
// - the archive directory, for durable, content-addressed output
//   (dfas_by_hash and the named per-game results), which may be a slower
//   or networked filesystem.
//
// Both default to "scratch", reproducing today's single-tree behavior
// exactly when neither environment variable is set. Nothing in the
// codebase reads these yet; this is the config surface a later change
// routes the existing "scratch/..." literals through.
class ScratchConfig
{
public:

  static std::string get_local_dir();
  static std::string get_archive_dir();
};

#endif
