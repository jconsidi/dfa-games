// DFA.cpp

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <filesystem>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <string>

#include "DFA.h"
#include "DFAFormat.h"
#include "Profile.h"
#include "ScratchConfig.h"
#include "VectorBitSet.h"
#include "parallel.h"
#include "utils.h"

// True from the moment a "building" DFA (the constructor below that creates
// a staging directory, never the one that loads a saved file) starts until
// set_initial_state finishes -- it now saves by hash immediately, see
// there. Nothing in this codebase constructs a second DFA while a first is
// still temporary (every operand a constructor reads is already ready, and
// ready now means already saved), so this should never be true when a new
// build starts; the assert below is what actually proves that instead of
// just asserting it in a comment.
//
// Deliberately never cleared on a failed/abandoned build (an exception
// during construction, before set_initial_state runs): nothing in this
// codebase catches such a failure and goes on to build more DFAs -- the one
// real case on record, an add_state mmap failure, is an uncaught exception
// straight to std::terminate(), which skips every destructor including
// this object's. If some future path ever did catch one and try to
// continue, leaving this stuck true is exactly the outcome wanted: fail
// loudly on the next build rather than silently pretend nothing happened.
static bool build_in_progress = false;

// Staging directory for a DFA under construction.
//
// Bare pid, no counter: build_in_progress (above) makes it an asserted
// invariant that this process never has two of these outstanding at once,
// and a pid cannot be reused while this process holds it, so the pid alone
// already names a directory nothing else on the machine can collide with.
static std::string get_temp_directory()
{
  return (ScratchConfig::get_local_dir() + "/build/" +
	  std::to_string(getpid()));
}

static std::vector<std::string> get_layer_file_names(int ndim, std::string directory)
{
  std::vector<std::string> output;

  for(int layer = 0; layer < ndim; ++layer)
    {
      output.push_back(directory + "/layer=" + std::to_string(layer));
    }

  return output;
}

// Ensure every directory component of path below root exists, tolerating
// EEXIST at each level, so a save (or the staging directory below) never has
// to trust that some other constructor already created the right directory
// ahead of time. path's own last "/"-separated component (the file,
// symlink, or directory about to be created) is left alone.
//
// root itself -- ScratchConfig::get_local_dir() or get_archive_dir() -- is
// never created here, only checked: it is configuration the operator is
// responsible for provisioning (a mount point, a path that may not exist
// because of a typo), not something this codebase should conjure into
// existence. Without that boundary, a mistyped or unmounted
// DFA_LOCAL_DIR/DFA_ARCHIVE_DIR would silently build a whole directory tree
// wherever it happened to point instead of failing loudly.
static void ensure_parent_directories(const std::string& root, const std::string& path)
{
  assert(path.starts_with(root + "/"));

  struct stat root_stat;
  if(stat(root.c_str(), &root_stat) || !S_ISDIR(root_stat.st_mode))
    {
      throw std::runtime_error(root + " does not exist or is not a directory");
    }

  size_t pos = root.length();
  while((pos = path.find('/', pos + 1)) != std::string::npos)
    {
      std::string prefix = path.substr(0, pos);
      if(mkdir(prefix.c_str(), 0700) && (errno != EEXIST))
	{
	  perror(("DFA save mkdir " + prefix).c_str());
	  throw std::runtime_error("DFA save mkdir failed");
	}
    }
}

// fsync() is a real barrier down to the device on Linux. On macOS -- per
// Apple's own fsync(2) documentation -- it only flushes the OS's own
// buffers and does not necessarily flush the drive's write cache;
// F_FULLFSYNC is Apple's stronger barrier for exactly this, and this
// project is developed almost entirely on macOS (see CLAUDE.md
// **Platforms**), so every fsync in the publish path goes through here
// rather than risking the weaker guarantee on the platform that matters
// most. Falls back to fsync() only when F_FULLFSYNC is unavailable at
// compile time (not Apple) or fails at run time (e.g. a filesystem that
// does not support it), since fsync() is still strictly better than
// nothing.
static void fsync_durable(int fildes, const std::string& what)
{
#ifdef F_FULLFSYNC
  if(fcntl(fildes, F_FULLFSYNC) == 0)
    {
      return;
    }
#endif

  if(fsync(fildes))
    {
      perror(what.c_str());
      throw std::runtime_error(what + " failed");
    }
}

std::string create_directory(std::string directory)
{
  // Placeholder trailing component so ensure_parent_directories treats
  // directory itself as a prefix to create, not just directory's parents.
  // Staging is always local -- never durable -- so the root is fixed here.
  ensure_parent_directories(ScratchConfig::get_local_dir(), directory + "/x");

  // ensure_parent_directories above tolerates directory itself already
  // existing (it is just the last prefix in its walk, EEXIST and all) --
  // deliberately, since two concurrent builds racing to create the exact
  // same path is not a real scenario here (bare-pid naming plus
  // build_in_progress / binary_build_in_progress already make "at most one
  // per live process" an asserted invariant). But a pid *can* be reused
  // across processes once the old one has exited, and if that old process
  // never got to clean up (a crash partway through, e.g. ENOSPC/EDQUOT
  // mid-build), its leftover directory is still sitting at this exact
  // path. Silently building on top of unknown leftover content is exactly
  // the kind of silence this project's own priorities rule out -- and it
  // is what eventually surfaces, confusingly, as a "directory not empty"
  // failure later, once something in the old leftovers or the new build's
  // own files can't be removed cleanly together. Reclaim it here instead,
  // before it can be mistaken for this build's own fresh staging area.
  remove_directory(directory);

  if(mkdir(directory.c_str(), 0700))
    {
      perror(("DFA staging mkdir " + directory).c_str());
      throw std::runtime_error("DFA staging mkdir failed");
    }

  return directory;
}

// Empty and remove a staging directory, including anything unexpected left
// in it (a subdirectory should never occur from this project's own code,
// but a leftover from a differently-shaped previous build reusing this pid
// is exactly the case create_directory above now guards against by calling
// this first). Reached from ~DFA for a DFA that was never saved, from
// save_by_hash once the .dfa file has taken over, and from create_directory
// itself before reusing a pid-named path. A missing directory is not an
// error -- the common case, since most construction paths never need this.
void remove_directory(std::string directory)
{
  std::error_code ec;
  std::filesystem::remove_all(directory, ec);
  if(ec)
    {
      throw std::runtime_error("DFA staging remove_all failed for " + directory + ": " + ec.message());
    }
}

DFA::DFA(const dfa_shape_t& shape_in)
  : shape(shape_in),
    ndim(int(shape.size())),
    layer_sizes(),
    directory(create_directory(get_temp_directory())),
    layer_file_names(get_layer_file_names(int(shape_in.size()), directory)),
    layer_transitions(),
    temporary(true)
{
  assert(!build_in_progress);
  build_in_progress = true;

  assert(ndim > 0);

  for(int layer = 0; layer < ndim; ++layer)
    {
      // initialize each layer with the two uniform states

      layer_sizes.push_back(2);
      // TODO: make sure initial layer size is big enough for giant
      // layer shapes
      layer_transitions.emplace_back(layer_file_names.at(layer), size_t(1024));

      int layer_shape = get_layer_shape(layer);
      for(dfa_state_t state = 0; state < 2; ++state)
	{
	  for(int c = 0; c < layer_shape; ++c)
	    {
	      layer_transitions[layer][state * layer_shape + c] = state;
	    }
	}
    }

  assert(layer_sizes.size() == ndim);
  assert(layer_file_names.size() == ndim);
  assert(layer_transitions.size() == ndim);
}

// Path of a saved DFA. A name under dfas_by_hash/ addresses the file
// directly; any other name is a symbolic link to one. durable resolves
// under the archive root, non-durable (cache) under the local root.
static std::string get_file_name(std::string name_in, bool durable)
{
  std::string root = durable ? ScratchConfig::get_archive_dir() : ScratchConfig::get_local_dir();

  if(name_in.starts_with("dfas_by_hash/"))
    {
      return root + "/" + name_in + ".dfa";
    }

  return root + "/" + name_in;
}

DFA::DFA(const dfa_shape_t& shape_in, std::string name_in, bool durable)
  : shape(),
    ndim(0),
    name(name_in),
    layer_sizes(),
    directory(),
    layer_file_names(),
    layer_transitions(),
    temporary(false)
{
  load_file(get_file_name(name_in, durable));

  // The file carries its own shape, so this is a cross check rather than an
  // input: a name that resolves to a DFA of the wrong shape is a mistake
  // worth catching here instead of much later.
  if(shape != shape_in)
    {
      throw std::runtime_error("DFA " + name_in + " has a different shape than expected");
    }

  assert(ready());
  assert(hash);
  assert(hash->length() == 64);
}

DFA::~DFA() noexcept(false)
{
  if(temporary)
    {
      // layer_transitions still holds this build's staging maps here --
      // member destructors only run after this body finishes. Release them
      // (same msync-then-clear as save_by_hash) before removing the
      // directory they live in, rather than unlinking files still mapped
      // underneath a live MemoryMap.
      for(MemoryMap<dfa_state_t>& layer_transition : layer_transitions)
	{
	  layer_transition.msync();
	}
      layer_transitions.clear();

      remove_directory(directory);
    }

  close_file();

  if(linear_bound)
    {
      delete linear_bound;
      linear_bound = 0;
    }
}

static uint16_t read_u16(const uint8_t *bytes)
{
  return uint16_t(uint16_t(bytes[0]) | uint16_t(uint16_t(bytes[1]) << 8));
}

static uint32_t read_u32(const uint8_t *bytes)
{
  return (uint32_t(bytes[0]) |
	  (uint32_t(bytes[1]) << 8) |
	  (uint32_t(bytes[2]) << 16) |
	  (uint32_t(bytes[3]) << 24));
}

static uint64_t read_u64(const uint8_t *bytes)
{
  return uint64_t(read_u32(bytes)) | (uint64_t(read_u32(bytes + 4)) << 32);
}

void DFA::close_file() const
{
  if(file_layout)
    {
      delete file_layout;
      file_layout = 0;
    }

  if(file_map)
    {
      delete file_map;
      file_map = 0;
    }
}

// Map a file whose contents are already known to match this DFA. Used after
// saving, to swap the in memory object over to what was just written.
void DFA::attach_file(std::string file_name_in) const
{
  close_file();

  file_name = file_name_in;
  file_map = new MemoryMap<uint8_t>(file_name_in, true);
  digest_verified = true;

  std::vector<uint64_t> file_layer_sizes;
  for(int layer = 0; layer < ndim; ++layer)
    {
      file_layer_sizes.push_back(uint64_t(layer_sizes.at(layer)));
    }
  file_layout = new dfa_format::Layout(shape, file_layer_sizes);
}

// Map a saved DFA and take the shape, layer sizes, initial state and flags
// from its header.
//
// Every check FORMAT-DFA.md section 7 requires ("must") of a reader is
// done here. Failures throw, which is what DFAUtil::_try_load turns into
// "not found". Section 7's one recommended ("may") check, the digest, is
// deliberately not done here -- see mmap() for why and where.
void DFA::load_file(std::string file_name_in)
{
  file_name = file_name_in;
  file_map = new MemoryMap<uint8_t>(file_name_in, true);
  digest_verified = false;

  size_t file_length = file_map->size();
  if(file_length < dfa_format::header_bytes)
    {
      throw std::runtime_error(file_name_in + " is shorter than a DFA header");
    }

  const uint8_t *bytes = file_map->begin();

  if(memcmp(bytes, dfa_format::magic, sizeof(dfa_format::magic)))
    {
      throw std::runtime_error(file_name_in + " is not a DFA file");
    }

  uint16_t file_version_major = read_u16(bytes + dfa_format::off_version_major);
  if(file_version_major != dfa_format::version_major)
    {
      throw std::runtime_error(file_name_in + " has an unsupported major version");
    }
  // A higher minor version is readable by ignoring what we do not understand.

  if(read_u32(bytes + dfa_format::off_header_bytes) != dfa_format::header_bytes)
    {
      throw std::runtime_error(file_name_in + " has an unsupported header size");
    }

  uint32_t file_ndim = read_u32(bytes + dfa_format::off_ndim);
  if(file_ndim < 1)
    {
      throw std::runtime_error(file_name_in + " has no layers");
    }

  uint32_t flags = read_u32(bytes + dfa_format::off_flags);
  if(flags & ~dfa_format::flag_canonical)
    {
      throw std::runtime_error(file_name_in + " sets reserved flag bits");
    }
  canonical = (flags & dfa_format::flag_canonical) != 0;

  if(file_length < dfa_format::off_tables + 20 * size_t(file_ndim))
    {
      throw std::runtime_error(file_name_in + " is too short for its layer tables");
    }

  ndim = int(file_ndim);
  const uint8_t *size_table = bytes + dfa_format::off_tables;
  const uint8_t *offset_table = size_table + 8 * size_t(file_ndim);
  const uint8_t *shape_table = offset_table + 8 * size_t(file_ndim);

  std::vector<uint64_t> file_layer_sizes;
  for(int layer = 0; layer < ndim; ++layer)
    {
      uint64_t layer_size = read_u64(size_table + 8 * size_t(layer));
      if(layer_size > DFA_STATE_MAX)
	{
	  throw std::runtime_error(file_name_in + " has a layer too large for dfa_state_t");
	}
      file_layer_sizes.push_back(layer_size);
      layer_sizes.push_back(size_t(layer_size));

      shape.push_back(int(read_u32(shape_table + 4 * size_t(layer))));
    }

  // Layout::Layout rejects ndim < 1, shape < 1 and layer sizes < 2.
  file_layout = new dfa_format::Layout(shape, file_layer_sizes);

  for(int layer = 0; layer < ndim; ++layer)
    {
      if(read_u64(offset_table + 8 * size_t(layer)) != file_layout->get_layer_offset(layer))
	{
	  throw std::runtime_error(file_name_in + " has a layer offset the layout does not imply");
	}
    }

  // An equality, not a lower bound: the file ends where the last block does.
  if(file_length != file_layout->file_len())
    {
      throw std::runtime_error(file_name_in + " is not the length its layout implies");
    }

  uint64_t file_initial_state = read_u64(bytes + dfa_format::off_initial_state);
  if(file_initial_state >= file_layer_sizes.at(0))
    {
      throw std::runtime_error(file_name_in + " has an out of range initial state");
    }
  initial_state = dfa_state_t(file_initial_state);

  char digest[65] = {0};
  for(size_t i = 0; i < dfa_format::digest_length; ++i)
    {
      snprintf(digest + 2 * i, 3, "%02x", bytes[dfa_format::off_digest + i]);
    }
  hash = std::string(digest);
}

dfa_state_t DFA::add_state(int layer, const DFATransitionsStaging& transitions)
{
  assert((0 <= layer) && (layer < ndim));

  int layer_shape = get_layer_shape(layer);
  assert(transitions.size() == layer_shape);

  // check for uniform states

  if(transitions[0] < 2)
    {
      bool is_uniform = true;
      for(int i = 1; i < layer_shape; ++i)
	{
	  if(transitions[i] != transitions[0])
	    {
	      is_uniform = false;
	      break;
	    }
	}

      if(is_uniform)
	{
	  return transitions[0];
	}
    }

  // add new state

  assert(layer_sizes[layer] < DFA_STATE_MAX);

  size_t current_offset = size_t(layer_sizes[layer]) * size_t(layer_shape);
  size_t next_offset = current_offset + size_t(layer_shape);

  MemoryMap<dfa_state_t>& current_transitions = layer_transitions[layer];
  size_t current_size = current_transitions.size();
  if(next_offset > current_size)
    {
      size_t next_size = current_size * 2;
      assert(next_size <= size_t(DFA_STATE_MAX));
      current_transitions = MemoryMap<dfa_state_t>(layer_file_names[layer], next_size);
    }

  size_t transition_bound = this->get_layer_size(layer + 1);
  for(int i = 0; i < layer_shape; ++i)
    {
      assert(transitions[i] < transition_bound);
      current_transitions[current_offset + i] = transitions[i];
    }

  return dfa_state_t(layer_sizes[layer]++);
}

dfa_state_t DFA::add_state_by_function(int layer, std::function<dfa_state_t(int)> transition_func)
{
  int layer_shape = this->get_layer_shape(layer);

  static DFATransitionsStaging transitions;
  transitions.resize(layer_shape);
  for(int i = 0; i < layer_shape; ++i)
    {
      transitions[i] = transition_func(i);
    }

  return add_state(layer, transitions);
}

dfa_state_t DFA::add_state_by_reference(int layer, const DFATransitionsReference& next_states)
{
  int layer_shape = this->get_layer_shape(layer);
  assert(next_states.get_layer_shape() == layer_shape);

  static DFATransitionsStaging temp_states;
  temp_states.resize(layer_shape);
  for(int i = 0; i < layer_shape; ++i)
    {
      temp_states[i] = next_states[i];
    }

  return this->add_state(layer, temp_states);
}

void DFA::build_layer(int layer, size_t layer_size_in, std::function<void(dfa_state_t, dfa_state_t *)> populate_func)
{
  assert(initial_state == ~dfa_state_t(0));
  assert(0 <= layer);
  assert(layer < ndim);

  assert(layer_sizes[layer] == 2);
  assert(2 <= layer_size_in);
  assert(layer_size_in < ~dfa_state_t(0));

  int layer_shape = get_layer_shape(layer);

  // Small enough to build entirely in memory: skip the file altogether
  // rather than write it out only to immediately mmap it back in. Bounded
  // (anon_bytes_max), not unconditional -- a dirty anonymous page can only
  // be reclaimed by the kernel via swap, unlike a file-backed one, which
  // it can just write back to the already-local, already-fast backing
  // file, so an unbounded version of this would trade a graceful page-out
  // for a harder OOM risk on a layer big enough to matter.
  size_t anon_bytes_max = size_t(1) << 20; // 1MB
  size_t total_bytes = size_t(layer_size_in) * size_t(layer_shape) * sizeof(dfa_state_t);
  if(total_bytes <= anon_bytes_max)
    {
      MemoryMap<dfa_state_t> anon_transitions(size_t(layer_size_in) * size_t(layer_shape));

      std::vector<dfa_state_t> state_iota(layer_size_in);
      std::iota(state_iota.begin(), state_iota.end(), 0);

      auto populate_row = [&](dfa_state_t state_id)
      {
        populate_func(state_id, anon_transitions.begin() + size_t(state_id) * size_t(layer_shape));
      };

      // constant state handling
      std::fill_n(anon_transitions.begin(), layer_shape, 0);
      std::fill_n(anon_transitions.begin() + layer_shape, layer_shape, 1);

      TRY_PARALLEL_3(std::for_each,
                     state_iota.begin() + 2,
                     state_iota.end(),
                     populate_row);

      layer_transitions[layer] = std::move(anon_transitions);
      layer_sizes[layer] = layer_size_in;

      assert(layer_transitions[layer].size() == size_t(layer_size_in) * size_t(get_layer_shape(layer)));
      return;
    }

  // close memory map and open file directly

  layer_transitions[layer].munmap();

  int fildes = open(layer_transitions[layer].filename().c_str(), O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
  if(fildes == -1)
    {
      throw std::runtime_error("open() failed");
    }

  // write file in chunks

  const size_t chunk_bytes_max = size_t(1) << 30; // 1GB
  const size_t chunk_transitions_max = chunk_bytes_max / sizeof(dfa_state_t);
  const size_t chunk_states_max = chunk_transitions_max / size_t(layer_shape);
  const size_t chunk_states = std::min(layer_size_in, chunk_states_max);
  assert(chunk_states >= 2);

  std::vector<dfa_state_t> chunk_buffer;
  chunk_buffer.reserve(chunk_states * layer_shape);

  std::vector<dfa_state_t> chunk_iota(chunk_states);
  std::iota(chunk_iota.begin(), chunk_iota.end(), 0);

  for(size_t chunk_start = 0; chunk_start < layer_size_in; chunk_start += chunk_states)
    {
      size_t chunk_end = std::min(chunk_start + chunk_states, layer_size_in);
      size_t chunk_size = chunk_end - chunk_start;
      chunk_buffer.resize(chunk_size * layer_shape);

      auto populate_buffer = [&](size_t i)
      {
        size_t state_id = chunk_start + i;
        assert(state_id <= DFA_STATE_MAX);
        populate_func(dfa_state_t(state_id), chunk_buffer.data() + i * layer_shape);
      };

      if(chunk_start == 0)
        {
          // constant state handling
          std::fill_n(chunk_buffer.begin(), layer_shape, 0);
          std::fill_n(chunk_buffer.begin() + layer_shape, layer_shape, 1);

          TRY_PARALLEL_3(std::for_each,
                         chunk_iota.begin() + 2,
                         chunk_iota.begin() + chunk_size,
                         populate_buffer);
        }
      else
        {
          TRY_PARALLEL_3(std::for_each,
                         chunk_iota.begin(),
                         chunk_iota.begin() + chunk_size,
                         populate_buffer);
        }

      write_buffer(fildes, chunk_buffer.data(), chunk_buffer.size());
    }

  if(ftruncate(fildes, size_t(layer_size_in) * size_t(get_layer_shape(layer)) * sizeof(dfa_state_t)))
    {
      perror("ftruncate");
      throw std::runtime_error("ftruncate() failed");
    }

    if(close(fildes))
    {
      perror("close");
      throw std::runtime_error("close() failed");
    }

  layer_sizes[layer] = layer_size_in;
  // open layer transitions for read only
  layer_transitions[layer] = MemoryMap<dfa_state_t>(layer_file_names[layer], true);
  assert(layer_transitions[layer].size() == size_t(layer_size_in) * size_t(get_layer_shape(layer)));
}

void DFA::copy_layer(int layer, const DFA& dfa_in)
{
  assert(dfa_in.ready());

  assert(initial_state == ~dfa_state_t(0));
  assert(0 <= layer);
  assert(layer < ndim);

  assert(layer_sizes[layer] == 2);
  assert(dfa_in.get_layer_size(layer) >= 2);

  int layer_shape = get_layer_shape(layer);
  assert(dfa_in.get_layer_shape(layer) == layer_shape);

  // The source may be staged as uint32 or saved at whatever width the format
  // derived for it, so copy through get_transitions rather than reading its
  // bytes directly.
  dfa_in.mmap();

  build_layer(layer, dfa_in.get_layer_size(layer), [&](dfa_state_t state, dfa_state_t *transitions_out)
  {
    DFATransitionsReference transitions_in = dfa_in.get_transitions(layer, state);
    for(int c = 0; c < layer_shape; ++c)
      {
	transitions_out[c] = transitions_in[c];
      }
  });
}

void DFA::set_initial_state(dfa_state_t initial_state_in)
{
  assert(initial_state == ~dfa_state_t(0));

  assert(initial_state_in < get_layer_size(0));
  initial_state = initial_state_in;

  // Trim staging down to the layer sizes actually reached, since add_state
  // grows the files by doubling.
  for(int layer = 0; layer < ndim; ++layer)
    {
      int layer_shape = get_layer_shape(layer);
      size_t expected_transitions_size = size_t(layer_sizes[layer]) * size_t(layer_shape);
      if(layer_transitions[layer].size() != expected_transitions_size)
	{
	  layer_transitions[layer] = MemoryMap<dfa_state_t>(layer_file_names[layer], expected_transitions_size);
	}
    }

  assert(ready());

  // Consolidate immediately rather than leaving this DFA temporary until
  // something else saves or discards it: add_state's growth-path files are
  // already real, already-written disk files (one or more per layer, from
  // doubling growth plus the trim just above), not a deferred cost this
  // introduces -- so folding them into the one canonical dfas_by_hash file
  // here is a write of roughly the same total data, not a new one. What it
  // buys back is real: those per-layer files and their mmaps stop
  // accumulating for as long as whatever holds this DFA does, which for a
  // process-lifetime cache (DFAUtil's singletons) or a leaked top-level
  // Game* (every build/solve/verify/validate binary today) is otherwise the
  // life of the process. set_canonical must run before this -- see its own
  // comment -- since canonical is part of what gets written below.
  save_by_hash(ScratchConfig::get_local_dir());
}

// Write this DFA to file_name_in in the format of FORMAT-DFA.md and return
// the hex digest of what was written.
//
// Mirrors rust/dfa-format/src/write.rs. The two write into the same content
// addressed store, so they have to agree byte for byte.
std::string DFA::serialize(std::string file_name_in) const
{
  Profile profile("serialize");

  assert(ready());
  assert(temporary);

  std::vector<uint64_t> sizes;
  for(int layer = 0; layer < ndim; ++layer)
    {
      sizes.push_back(uint64_t(layer_sizes.at(layer)));
    }
  dfa_format::Layout layout(shape, sizes);

  int fildes = open(file_name_in.c_str(), O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
  if(fildes == -1)
    {
      perror("DFA serialize open");
      throw std::runtime_error("DFA serialize open failed");
    }

  // Header. The digest is filled in at the end, and flags is already settled
  // because canonicity is declared by the subclass rather than discovered.
  std::vector<uint8_t> header(dfa_format::header_bytes, 0);
  memcpy(&header[dfa_format::off_magic], dfa_format::magic, sizeof(dfa_format::magic));
  dfa_format::encode_entry(dfa_format::version_major, 2, &header[dfa_format::off_version_major]);
  dfa_format::encode_entry(dfa_format::version_minor, 2, &header[dfa_format::off_version_minor]);
  dfa_format::encode_entry(dfa_format::header_bytes, 4, &header[dfa_format::off_header_bytes]);
  dfa_format::encode_entry(uint64_t(ndim), 4, &header[dfa_format::off_ndim]);
  dfa_format::encode_entry(canonical ? dfa_format::flag_canonical : 0, 4, &header[dfa_format::off_flags]);
  dfa_format::encode_entry(initial_state, 8, &header[dfa_format::off_initial_state]);
  write_buffer(fildes, header.data(), header.size());

  // Tables, then padding up to the first block.
  std::vector<uint8_t> tables;
  for(int layer = 0; layer < ndim; ++layer)
    {
      uint8_t entry[8];
      dfa_format::encode_entry(sizes.at(layer), 8, entry);
      tables.insert(tables.end(), entry, entry + 8);
    }
  for(int layer = 0; layer < ndim; ++layer)
    {
      uint8_t entry[8];
      dfa_format::encode_entry(layout.get_layer_offset(layer), 8, entry);
      tables.insert(tables.end(), entry, entry + 8);
    }
  for(int layer = 0; layer < ndim; ++layer)
    {
      uint8_t entry[4];
      dfa_format::encode_entry(uint64_t(shape.at(layer)), 4, entry);
      tables.insert(tables.end(), entry, entry + 4);
    }
  tables.resize(size_t(layout.get_layer_offset(0) - dfa_format::off_tables), 0);
  write_buffer(fildes, tables.data(), tables.size());

  // Blocks, narrowing each transition to the width the layout derives.
  for(int layer = 0; layer < ndim; ++layer)
    {
      profile.tic("serialize layer");

      int layer_shape = get_layer_shape(layer);
      int width = layout.get_width(layer);
      uint64_t layer_size = sizes.at(layer);
      uint64_t bound = layout.next_layer_size(layer);

      layer_transitions.at(layer).mmap();
      const MemoryMap<dfa_state_t>& source = layer_transitions.at(layer);

      size_t block_bytes = size_t(layout.get_block_bytes(layer));
      std::vector<uint8_t> block(block_bytes);

      std::vector<dfa_state_t> previous_row;
      for(uint64_t state = 0; state < layer_size; ++state)
	{
	  size_t source_offset = size_t(state) * size_t(layer_shape);
	  size_t block_offset = size_t(state) * size_t(layer_shape) * size_t(width);

	  for(int c = 0; c < layer_shape; ++c)
	    {
	      dfa_state_t next_state = source[source_offset + size_t(c)];
	      assert(next_state < bound);
	      dfa_format::encode_entry(next_state, width, &block[block_offset + size_t(c) * size_t(width)]);
	    }

	  // Rows 0 and 1 carry fixed values (section 4).
	  if(state < 2)
	    {
	      for(int c = 0; c < layer_shape; ++c)
		{
		  assert(source[source_offset + size_t(c)] == state);
		}
	      continue;
	    }

	  // Verify the cheap half of the canonical claim. The rows are
	  // streaming past in order anyway, so comparing each against the
	  // last costs nothing, and a subclass that declares canonical
	  // wrongly fails here rather than producing a file readers reject.
	  if(canonical)
	    {
	      std::vector<dfa_state_t> row(source.begin() + long(source_offset),
					   source.begin() + long(source_offset) + layer_shape);

	      bool uniform_reject = true;
	      bool uniform_accept = true;
	      for(int c = 0; c < layer_shape; ++c)
		{
		  uniform_reject = uniform_reject && (row[size_t(c)] == dfa_format::state_reject);
		  uniform_accept = uniform_accept && (row[size_t(c)] == dfa_format::state_accept);
		}
	      assert(!uniform_reject);
	      assert(!uniform_accept);

	      if(previous_row.size())
		{
		  assert(previous_row < row);
		}
	      previous_row = row;
	    }
	}

      write_buffer(fildes, block.data(), block.size());

      // Padding between blocks. The last block is followed by EOF.
      if(layer + 1 < ndim)
	{
	  uint64_t end = layout.get_layer_offset(layer) + layout.get_block_bytes(layer);
	  std::vector<uint8_t> padding(size_t(layout.get_layer_offset(layer + 1) - end), 0);
	  write_buffer(fildes, padding.data(), padding.size());
	}
    }

  // The digest covers [48, EOF), which includes flags, so it can only be
  // computed once everything else is on disk.
  profile.tic("serialize digest");

  if(lseek(fildes, off_t(dfa_format::digest_coverage_start), SEEK_SET) == -1)
    {
      perror("DFA serialize lseek");
      throw std::runtime_error("DFA serialize lseek failed");
    }

  unsigned char digest[SHA256_DIGEST_LENGTH];
  static const EVP_MD *hash_implementation = EVP_sha256();
  EVP_MD_CTX *hash_context = EVP_MD_CTX_create();
  EVP_DigestInit_ex(hash_context, hash_implementation, NULL);

  std::vector<uint8_t> chunk(size_t(1) << 23);
  while(1)
    {
      ssize_t chunk_read = read(fildes, chunk.data(), chunk.size());
      if(chunk_read < 0)
	{
	  perror("DFA serialize read");
	  throw std::runtime_error("DFA serialize read failed");
	}
      if(chunk_read == 0)
	{
	  break;
	}
      EVP_DigestUpdate(hash_context, chunk.data(), size_t(chunk_read));
    }
  EVP_DigestFinal_ex(hash_context, digest, 0);
  EVP_MD_CTX_destroy(hash_context);

  if(lseek(fildes, off_t(dfa_format::off_digest), SEEK_SET) == -1)
    {
      perror("DFA serialize lseek");
      throw std::runtime_error("DFA serialize lseek failed");
    }
  write_buffer(fildes, digest, sizeof(digest));

  fsync_durable(fildes, "DFA serialize fsync");
  if(close(fildes))
    {
      perror("DFA serialize close");
      throw std::runtime_error("DFA serialize close failed");
    }

  std::stringstream ss;
  for(size_t i = 0; i < SHA256_DIGEST_LENGTH; ++i)
    {
      ss << std::hex << std::setw(2) << std::setfill('0') << int(digest[i]);
    }
  return ss.str();
}

DFAIterator DFA::cbegin() const
{
  if(initial_state == 0)
    {
      return cend();
    }

  mmap();

  std::vector<int> characters;

  dfa_state_t current_state = initial_state;
  for(int layer = 0; layer < ndim; ++layer)
    {
      int layer_shape = get_layer_shape(layer);
      DFATransitionsReference transitions = get_transitions(layer, current_state);

      // scan for first accepted character
      for(characters.push_back(0);
	  (characters[layer] < layer_shape) && !transitions[characters[layer]];
	  ++characters[layer])
	{
	}
      assert(characters[layer] < layer_shape);
      assert(transitions[characters[layer]]);

      current_state = transitions[characters[layer]];
    }

  assert(current_state == 1);

  return DFAIterator(*this, characters);
}

DFAIterator DFA::cend() const
{
  std::vector<int> characters;
  characters.push_back(shape[0]);
  for(int layer = 1; layer < ndim; ++layer)
    {
      characters.push_back(0);
    }

  return DFAIterator(*this, characters);
}

bool DFA::contains(const DFAString& string_in) const
{
  int current_state = initial_state;
  for(int layer = 0; layer < ndim; ++layer)
    {
      current_state = this->get_transitions(layer, current_state)[string_in[layer]];
    }

  return current_state != 0;
}

std::string DFA::calculate_digest() const
{
  Profile profile("calculate_digest");

  if(!file_map)
    {
      throw std::runtime_error("DFA has not been saved, so it has no digest yet");
    }
  file_map->mmap();

  unsigned char digest[SHA256_DIGEST_LENGTH];
  static const EVP_MD *hash_implementation = EVP_sha256();
  EVP_MD_CTX *hash_context = EVP_MD_CTX_create();
  EVP_DigestInit_ex(hash_context, hash_implementation, NULL);
  EVP_DigestUpdate(hash_context,
		   file_map->begin() + dfa_format::digest_coverage_start,
		   file_map->size() - dfa_format::digest_coverage_start);
  EVP_DigestFinal_ex(hash_context, digest, 0);
  EVP_MD_CTX_destroy(hash_context);

  std::stringstream ss;
  for(size_t i = 0; i < SHA256_DIGEST_LENGTH; ++i)
    {
      ss << std::hex << std::setw(2) << std::setfill('0') << int(digest[i]);
    }
  return ss.str();
}

// Independent digest computation over an arbitrary file path, for
// reverifying a copy just written to a second root rather than trusting the
// write happened correctly. Deliberately separate from calculate_digest(),
// which hashes this->file_map and must not be disturbed by this.
static std::string digest_of_file(const std::string& file_name)
{
  MemoryMap<uint8_t> file_map(file_name, true);
  file_map.mmap();

  unsigned char digest[SHA256_DIGEST_LENGTH];
  static const EVP_MD *hash_implementation = EVP_sha256();
  EVP_MD_CTX *hash_context = EVP_MD_CTX_create();
  EVP_DigestInit_ex(hash_context, hash_implementation, NULL);
  EVP_DigestUpdate(hash_context,
		   file_map.begin() + dfa_format::digest_coverage_start,
		   file_map.size() - dfa_format::digest_coverage_start);
  EVP_DigestFinal_ex(hash_context, digest, 0);
  EVP_MD_CTX_destroy(hash_context);

  std::stringstream ss;
  for(size_t i = 0; i < SHA256_DIGEST_LENGTH; ++i)
    {
      ss << std::hex << std::setw(2) << std::setfill('0') << int(digest[i]);
    }
  return ss.str();
}

std::string DFA::get_hash() const
{
  assert(ready());

  // The hash is the file's own digest, so it does not exist until the DFA has
  // been written. Nothing here has a name yet to say whether the result is
  // meant to be durable, so this is a bare content-addressed publish, and it
  // defaults to the local root -- the common case is an operand being hashed
  // to build some other DFA's cache key, not a result anyone asked to keep.
  if(!hash)
    {
      save_by_hash(ScratchConfig::get_local_dir());
    }

  assert(hash);
  assert(hash->length() == 64);

  return *hash;
}

dfa_state_t DFA::get_initial_state() const
{
  assert(initial_state != ~dfa_state_t(0));
  return initial_state;
}

int DFA::get_layer_shape(int layer) const
{
  assert((0 <= layer) && (layer < ndim));

  return shape.at(layer);
}

size_t DFA::get_layer_size(int layer) const
{
  assert(layer <= ndim);

  if(layer == ndim)
    {
      return 2;
    }

  return layer_sizes[layer];
}

const DFALinearBound& DFA::get_linear_bound() const
{
  Profile profile("get_linear_bound");

  if(!linear_bound)
    {
      assert(ready());

      mmap();

      std::vector<std::vector<bool>> bounds;
      bool reached_accept_all = initial_state == 1;

      // States reachable from initial_state, tracked layer by layer, so the
      // aggregation below reflects only characters some accepted string can
      // actually use at that layer -- not every ordinary state this DFA's
      // construction happened to leave in the layer regardless of whether
      // anything reaches it. reachable[layer] holds the states entering
      // that layer; reachable.size() only ever grows as far as needed,
      // since layers past the point reached_accept_all goes true never
      // consult it (see the header comment on get_linear_bound).
      std::vector<VectorBitSet> reachable;
      reachable.emplace_back(get_layer_size(0));
      if(!reached_accept_all && (initial_state >= 2))
	{
	  reachable[0].add(initial_state);
	}

      for(int layer = 0; layer < ndim; ++layer)
	{
	  int layer_shape = get_layer_shape(layer);
	  if(reached_accept_all)
	    {
	      bounds.emplace_back(layer_shape, true);
	      continue;
	    }

	  bounds.emplace_back(layer_shape, false);
	  std::vector<bool>& curr_bounds = bounds[layer];

	  bool have_next_layer = (layer + 1 < ndim);
	  if(have_next_layer)
	    {
	      reachable.emplace_back(get_layer_size(layer + 1));
	    }
	  VectorBitSet *next_reachable = have_next_layer ? &reachable.back() : 0;

	  // narrow shape case: pack this layer's alphabet into one uint32 per
	  // state and reduce in parallel. VectorBitSetIterator does not
	  // satisfy std::random_access_iterator (see BinaryDFA::build_linear
	  // for the same restriction), so materialize the reachable state ids
	  // into a plain array first.

	  if(layer_shape <= 32)
	    {
	      MemoryMap<dfa_state_t> curr_reachable_ids(reachable[layer].count());
	      {
		dfa_state_t i = 0;
		for(auto iter = reachable[layer].begin(); iter < reachable[layer].end(); ++iter, ++i)
		  {
		    curr_reachable_ids[i] = dfa_state_t(*iter);
		  }
	      }

	      auto get_local = [&](dfa_state_t state_id)
		{
		  bool local_accept_all = false;
		  uint32_t local_bounds = 0;

		  DFATransitionsReference transitions = this->get_transitions(layer, state_id);
		  for(int i = 0; i < layer_shape; ++i)
		    {
		      if(transitions[i] == 1)
			{
			  local_accept_all = true;
			}
		      if(transitions[i])
			{
			  local_bounds |= uint32_t(1) << i;
			}
		    }

		  return std::pair<bool, uint32_t>(local_accept_all, local_bounds);
		};

	      auto reduce_local = [](std::pair<bool, uint32_t> a, std::pair<bool, uint32_t> b)
	      {
		return std::pair<bool, uint32_t>(std::get<0>(a) || std::get<0>(b),
						 std::get<1>(a) | std::get<1>(b));
	      };

	      // curr_reachable_ids.begin()/.end() assert the map is mapped,
	      // which a zero-length MemoryMap never is (no reachable states
	      // at this layer -- e.g. this DFA is constant reject) -- so
	      // this must stay conditional rather than always calling
	      // through to transform_reduce.
	      std::pair<bool, uint32_t> combined_bounds(false, 0);
	      if(curr_reachable_ids.size() > 0)
		{
		  combined_bounds =
		    TRY_PARALLEL_5(std::transform_reduce,
				   curr_reachable_ids.begin(),
				   curr_reachable_ids.end(),
				   (std::pair<bool, uint32_t>(false, 0)),
				   reduce_local,
				   get_local);
		}

	      if(std::get<0>(combined_bounds))
		{
		  reached_accept_all = true;
		}
	      for(int i = 0; i < 32; ++i)
		{
		  if(std::get<1>(combined_bounds) & (uint32_t(1) << i))
		    {
		      curr_bounds[i] = true;
		    }
		}

	      if(next_reachable)
		{
		  for(size_t idx = 0; idx < curr_reachable_ids.size(); ++idx)
		    {
		      DFATransitionsReference transitions = this->get_transitions(layer, curr_reachable_ids[idx]);
		      for(int i = 0; i < layer_shape; ++i)
			{
			  dfa_state_t t = transitions[i];
			  if(t >= 2)
			    {
			      next_reachable->add(t);
			    }
			}
		    }
		}

	      continue;
	    }

	  // general shape case: not parallelized, same as before this fix.

	  for(auto iter = reachable[layer].begin(); iter < reachable[layer].end(); ++iter)
	    {
	      dfa_state_t state_id = dfa_state_t(*iter);
	      DFATransitionsReference transitions = this->get_transitions(layer, state_id);
	      for(int i = 0; i < layer_shape; ++i)
		{
		  dfa_state_t t = transitions[i];
		  if(t == 1)
		    {
		      reached_accept_all = true;
		    }
		  if(t)
		    {
		      curr_bounds[i] = true;
		    }
		  if(next_reachable && (t >= 2))
		    {
		      next_reachable->add(t);
		    }
		}
	    }
	}

      linear_bound = new DFALinearBound(shape, bounds);
    }

  return *linear_bound;
}

std::string DFA::get_name() const
{
  if(name != "")
    {
      return name;
    }

  std::stringstream output;
  output << this;
  return output.str();
}

const dfa_shape_t& DFA::get_shape() const
{
  return shape;
}

int DFA::get_shape_size() const
{
  return int(shape.size());
}

DFATransitionsReference DFA::get_transitions(int layer, size_t state_index) const
{
  assert(layer < ndim);
  assert(state_index < layer_sizes[layer]);

  int layer_shape = get_layer_shape(layer);

  if(file_map)
    {
      // Saved: entries are stored at the width the format derives. Routed
      // through this->mmap(), not file_map->mmap() directly, so the first
      // real access to a loaded DFA's data -- which this is, for most
      // callers -- runs (and every access after the first skips) the
      // digest check mmap() does for a loaded file. See mmap() and
      // digest_verified.
      this->mmap();
      size_t offset = size_t(file_layout->row_offset(layer, uint64_t(state_index)));
      return DFATransitionsReference(file_map->begin() + offset,
				     layer_shape,
				     file_layout->get_width(layer));
    }

  // Being built: staging holds one uint32 per transition.
  const MemoryMap<dfa_state_t>& staging = layer_transitions[layer];
  staging.mmap();
  const uint8_t *row = reinterpret_cast<const uint8_t *>(staging.begin() +
							 state_index * size_t(layer_shape));
  return DFATransitionsReference(row, layer_shape, int(sizeof(dfa_state_t)));
}

bool DFA::is_canonical() const
{
  return canonical;
}

void DFA::set_canonical(bool canonical_in)
{
  // canonical is part of what serialize() writes (FORMAT-DFA.md section 8,
  // flag_canonical), and set_initial_state finalizes -- and may serialize --
  // this DFA the moment it runs. Deciding canonical after that would either
  // be silently discarded or (once set_initial_state saves eagerly) land in
  // a file already written with the wrong flag, so it must happen first.
  assert(!ready());
  canonical = canonical_in;
}

bool DFA::is_constant(bool constant_in) const
{
  assert(ready());
  return initial_state == int(constant_in);
}

bool DFA::is_linear() const
{
  assert(ready());

  if(initial_state < 2)
    {
      // degenerate, but treat this as linear
      return true;
    }

  mmap();

  dfa_state_t curr_accept_state = initial_state;
  for(int layer = 0; layer < ndim; ++layer)
    {
      int layer_shape = this->get_layer_shape(layer);
      DFATransitionsReference transitions = get_transitions(layer, curr_accept_state);

      int next_accept_state = 0;
      int i = 0;
      for(; i < layer_shape; ++i)
	{
	  if(transitions[i] != 0)
	    {
	      next_accept_state = transitions[i];
	      break;
	    }
	}
      // DFA construction should guarantee at least one non-rejecting
      // transition.
      assert(next_accept_state != 0);

      for(; i < layer_shape; ++i)
	{
	  if(transitions[i] == 0)
	    {
	      continue;
	    }
	  else if(transitions[i] != next_accept_state)
	    {
	      return false;
	    }
	}

      curr_accept_state = next_accept_state;
    }

  return true;
}

void DFA::mmap() const
{
  if(file_map)
    {
      file_map->mmap();

      // FORMAT-DFA.md section 7's recommended (not required) digest check,
      // done here rather than in load_file so a loaded-but-never-used DFA
      // never pays for it, and once per object (not on every mmap() --
      // get_transitions calls this on every transition lookup) so a DFA
      // that is actually used does not pay for it repeatedly.
      if(!digest_verified)
	{
	  assert(hash);
	  std::string digest = calculate_digest();
	  if(digest != *hash)
	    {
	      throw std::runtime_error(file_name + " failed digest verification: expected " +
					*hash + ", got " + digest);
	    }
	  digest_verified = true;
	}

      return;
    }

  for(int layer = 0; layer < ndim; ++layer)
    {
      layer_transitions[layer].mmap();
    }
}

void DFA::munmap() const
{
  if(file_map)
    {
      file_map->munmap();
      return;
    }

  for(int layer = 0; layer < ndim; ++layer)
    {
      layer_transitions[layer].munmap();
    }
}

std::optional<std::string> DFA::parse_hash(std::string name_in, bool durable)
{
  std::string hash_prefix = "dfas_by_hash/";
  std::string hash_suffix = ".dfa";

  if(name_in.starts_with(hash_prefix))
    {
      assert(name_in.length() == hash_prefix.length() + 64);
      std::string hash = name_in.substr(hash_prefix.length());
      assert(hash.length() == 64);
      return std::optional<std::string>(hash);
    }

  // Any other name is a symbolic link to a file in dfas_by_hash/.

  std::string symlink_path = (durable ? ScratchConfig::get_archive_dir() : ScratchConfig::get_local_dir()) + "/" + name_in;
  char link_target[1024] = {0};
  ssize_t ret = readlink(symlink_path.c_str(), link_target, sizeof(link_target) - 1);
  if(ret >= 0)
    {
      std::string link_target_string(link_target);
      if(!link_target_string.ends_with(hash_suffix))
	{
	  return std::optional<std::string>();
	}
      link_target_string.resize(link_target_string.length() - hash_suffix.length());

      if(link_target_string.length() < hash_prefix.length() + 64)
	{
	  return std::optional<std::string>();
	}

      size_t hash_offset = link_target_string.length() - hash_prefix.length() - 64;
      if(link_target_string.substr(hash_offset, hash_prefix.length()) == hash_prefix)
	{
	  std::string hash = link_target_string.substr(hash_offset + hash_prefix.length());
	  return std::optional<std::string>(hash);
	}
    }
  else if(errno != ENOENT)
    {
      perror("readlink");
    }

  return std::optional<std::string>();
}

bool DFA::ready() const
{
  return initial_state != ~dfa_state_t(0);
}

void DFA::save_impl(std::string name_in, const std::string& root) const
{
  assert(!name_in.starts_with("dfas_by_hash/"));

  save_by_hash(root);
  publish_symlink(name_in, root);
}

// Points root/name_in at this DFA's already-published dfas_by_hash file.
// Split out of save_impl so save_durable can publish by hash locally first
// and only then symlink under archive, without saving by hash under
// archive a second time.
void DFA::publish_symlink(std::string name_in, const std::string& root) const
{
  assert(!name_in.starts_with("dfas_by_hash/"));

  std::string symlink_path = root + "/" + name_in;

  // add symbolic link to the existing file in dfas_by_hash/
  std::string symlink_target = "dfas_by_hash/" + get_hash() + ".dfa";
  for(char c : name_in)
    {
      if(c == '/')
	{
	  symlink_target = "../" + symlink_target;
	}
    }

  // Publish the symlink atomically: symlink under a private temporary name
  // in the same directory, then rename it over the target. rename(2)
  // replaces the destination indivisibly, so there is never a window where
  // symlink_path names nothing, unlike unlink() then symlink(), which briefly
  // deletes the name before recreating it.
  static int next_symlink_id = 0;
  std::string symlink_temp_path = (symlink_path + ".tmp-" +
				    std::to_string(getpid()) + "-" +
				    std::to_string(next_symlink_id++));

  ensure_parent_directories(root, symlink_temp_path);

  int symlink_ret = symlink(symlink_target.c_str(), symlink_temp_path.c_str());
  if(symlink_ret)
    {
      perror(("DFA save symlink " + symlink_temp_path).c_str());
      throw std::runtime_error("DFA save symlink failed");
    }

  int rename_ret = rename(symlink_temp_path.c_str(), symlink_path.c_str());
  if(rename_ret)
    {
      perror(("DFA save rename " + symlink_temp_path + " to " + symlink_path).c_str());
      throw std::runtime_error("DFA save rename failed");
    }

  name = name_in;
}

void DFA::save_cache(std::string name_in) const
{
  save_impl(name_in, ScratchConfig::get_local_dir());
}

void DFA::save_durable(std::string name_in) const
{
  // Build and hash against local, fast storage first -- the write and the
  // read back to compute the digest, exactly what save_cache alone does --
  // so a slow or networked archive only ever sees a single publish of the
  // already-finished bytes (save_by_hash's own cross-root publish: a
  // hardlink when local and archive share a filesystem, a copy reverified
  // from the bytes on disk otherwise), never an in-progress build. This is
  // a no-op the second time when local and archive are the same root.
  save_by_hash(ScratchConfig::get_local_dir());

  std::string archive_dir = ScratchConfig::get_archive_dir();
  save_by_hash(archive_dir);
  publish_symlink(name_in, archive_dir);
}

void DFA::save_by_hash(const std::string& root) const
{
  assert(ready());
  if(!temporary)
    {
      // Already published somewhere. If that happens to be root, there is
      // nothing to do. Otherwise this DFA legitimately needs a second name
      // under root too -- a config-driven component reused as durable
      // solver output is the case that came up -- so copy it there rather
      // than refuse: refusing here was itself the bug.
      std::string expected_prefix = root + "/dfas_by_hash/";
      if(!file_name.starts_with(expected_prefix))
	{
	  publish_copy(root);
	}
      return;
    }

  std::string dfas_by_hash_dir = root + "/dfas_by_hash";

  // Write under a temporary name, since the final name is the digest of the
  // bytes and is not known until they have all been written.
  static int next_serialize_id = 0;
  std::string temporary_name = (dfas_by_hash_dir + "/.tmp-" +
				std::to_string(getpid()) + "-" +
				std::to_string(next_serialize_id++) + ".dfa");

  ensure_parent_directories(root, temporary_name);

  std::string digest = serialize(temporary_name);
  std::string file_name_new = dfas_by_hash_dir + "/" + digest + ".dfa";

  // link() fails with EEXIST rather than replacing, which is what section 10
  // asks for: a file of this name already holds these exact bytes, and a
  // reader may have it open. rename() would clobber it.
  int link_ret = link(temporary_name.c_str(), file_name_new.c_str());
  if(link_ret && (errno != EEXIST))
    {
      perror("DFA save link");
      throw std::runtime_error("DFA save link failed");
    }

  if(unlink(temporary_name.c_str()))
    {
      perror("DFA save unlink");
      throw std::runtime_error("DFA save unlink failed");
    }

  // Make the directory entry durable, not just the file's own bytes: without
  // this, a crash could leave file_name_new written and fsynced but the
  // link that names it not yet on disk as far as the directory is concerned.
  int dir_fildes = open(dfas_by_hash_dir.c_str(), O_RDONLY);
  if(dir_fildes == -1)
    {
      perror("DFA save directory open");
      throw std::runtime_error("DFA save directory open failed");
    }
  fsync_durable(dir_fildes, "DFA save directory fsync");
  if(close(dir_fildes))
    {
      perror("DFA save directory close");
      throw std::runtime_error("DFA save directory close failed");
    }

  hash = digest;

  // Switch this object over to the file, and drop the staging directory.
  attach_file(file_name_new);
  build_in_progress = false;

  // munmap() (run by ~MemoryMap, below) does not imply a flush -- dirty
  // pages of a MAP_SHARED mapping stay in the page cache and are written
  // back on the kernel's own schedule, same as an un-fsynced write(). On a
  // networked filesystem, unlinking (via remove_directory, below) a file
  // whose writeback is still pending can make the client treat it as still
  // busy, which is exactly what "directory not empty" during staging
  // cleanup looks like. msync(MS_SYNC) each mapping first so the data is
  // actually out before anything tries to remove the file it lives in.
  for(MemoryMap<dfa_state_t>& layer_transition : layer_transitions)
    {
      layer_transition.msync();
    }
  layer_transitions.clear();
  layer_file_names.clear();
  remove_directory(directory);
  directory = "";
  temporary = false;
}

// Publishes an already-published DFA's bytes under a second root, under
// the same content-addressed name.
//
// Tries a direct hardlink from the file already on disk first: local and
// archive are often configured as distinct directories that nonetheless
// share a filesystem, and link() there costs nothing and stores the bytes
// once, with nothing to reverify since it is the very inode rather than a
// new write of it. Only when that fails with EXDEV -- local and archive
// really are different filesystems, e.g. one of them is networked -- does
// this fall back to a byte copy: write to a temporary name under root's
// dfas_by_hash (its own staging counter, next_copy_id, kept separate from
// save_by_hash's own next_serialize_id since the two can be in flight on
// the same object at once), then reverify the digest from the bytes just
// written to root rather than trusting *hash, since a corrupted copy that
// reported success is exactly the failure mode this whole codebase exists
// to avoid, then link that into place and fsync the directory -- the same
// publish discipline save_by_hash itself uses.
void DFA::publish_copy(const std::string& root) const
{
  assert(!temporary);
  assert(hash);

  std::string dfas_by_hash_dir = root + "/dfas_by_hash";
  std::string file_name_new = dfas_by_hash_dir + "/" + *hash + ".dfa";

  ensure_parent_directories(root, file_name_new);

  int link_ret = link(file_name.c_str(), file_name_new.c_str());
  if(link_ret && (errno != EEXIST))
    {
      if(errno != EXDEV)
	{
	  perror("DFA publish_copy link");
	  throw std::runtime_error("DFA publish_copy link failed");
	}

      static int next_copy_id = 0;
      std::string temporary_name = (dfas_by_hash_dir + "/.tmp-copy-" +
				    std::to_string(getpid()) + "-" +
				    std::to_string(next_copy_id++) + ".dfa");

      ensure_parent_directories(root, temporary_name);

      // this->mmap(), not file_map->mmap() directly: a DFA reaching
      // publish_copy may have been loaded rather than built in this
      // process (e.g. a config component reused as durable output), so
      // its bytes may not be digest-verified yet. Copying unverified
      // bytes to a second root and reverifying only the copy would prove
      // the copy matches the source, not that the source itself is
      // sound.
      this->mmap();

      int fildes = open(temporary_name.c_str(), O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
      if(fildes == -1)
	{
	  perror("DFA publish_copy open");
	  throw std::runtime_error("DFA publish_copy open failed");
	}
      write_buffer(fildes, file_map->begin(), file_map->size());
      fsync_durable(fildes, "DFA publish_copy fsync");
      if(close(fildes))
	{
	  perror("DFA publish_copy close");
	  throw std::runtime_error("DFA publish_copy close failed");
	}

      std::string copy_digest = digest_of_file(temporary_name);
      if(copy_digest != *hash)
	{
	  throw std::runtime_error("DFA publish_copy digest mismatch copying to " + root);
	}

      // link() fails with EEXIST rather than replacing, same reasoning as
      // save_by_hash: a file of this name already holds these exact bytes,
      // and a reader may have it open.
      int copy_link_ret = link(temporary_name.c_str(), file_name_new.c_str());
      if(copy_link_ret && (errno != EEXIST))
	{
	  perror("DFA publish_copy link");
	  throw std::runtime_error("DFA publish_copy link failed");
	}

      if(unlink(temporary_name.c_str()))
	{
	  perror("DFA publish_copy unlink");
	  throw std::runtime_error("DFA publish_copy unlink failed");
	}
    }

  // Make the directory entry durable, same reasoning as save_by_hash: a
  // crash could otherwise leave the link (or the copy above) written and
  // fsynced but not yet durable as far as the directory is concerned.
  int dir_fildes = open(dfas_by_hash_dir.c_str(), O_RDONLY);
  if(dir_fildes == -1)
    {
      perror("DFA publish_copy directory open");
      throw std::runtime_error("DFA publish_copy directory open failed");
    }
  fsync_durable(dir_fildes, "DFA publish_copy directory fsync");
  if(close(dir_fildes))
    {
      perror("DFA publish_copy directory close");
      throw std::runtime_error("DFA publish_copy directory close failed");
    }
}

void DFA::set_name(std::string name_in) const
{
  name = name_in;
}

// Where the cached position count lives. Section 9 keeps derived data out of
// the file itself: the bytes must stay a function of the automaton alone, and
// readers must be able to treat the file as immutable.
static std::string get_size_file_name(std::string hash_in)
{
  return ScratchConfig::get_local_dir() + "/sizes/" + hash_in;
}

double DFA::size() const
{
  assert(ready());

  if(initial_state == 0)
    {
      return 0.0;
    }

  if((size_cache == 0.0) && !size_cache_loaded && hash)
    {
      size_cache_loaded = true;
      try
	{
	  MemoryMap<double> cached(get_size_file_name(*hash), true);
	  if(cached.size() == 1)
	    {
	      size_cache = cached[0];
	    }
	}
      catch(const std::runtime_error& e)
	{
	  // no cached size yet
	}
    }

  if(size_cache == 0.0)
    {
      mmap();

      std::vector<double> previous_counts({0, 1}); // reject, accept
      for(int layer = ndim - 1; layer >= 0; --layer)
	{
	  int layer_shape = this->get_layer_shape(layer);

	  size_t layer_size = get_layer_size(layer);
	  std::vector<double> current_counts(layer_size);
          const double *current_counts_first = &current_counts.at(0);
          TRY_PARALLEL_3(std::for_each, current_counts.begin(), current_counts.end(), [&](double& state_count_out)
          {
            size_t state_index = &state_count_out - current_counts_first;
            DFATransitionsReference transitions = this->get_transitions(layer, state_index);

            double state_count = 0;
            for(int i = 0; i < layer_shape; ++i)
              {
                state_count += previous_counts.at(transitions[i]);
              }

            state_count_out = state_count;
          });

          std::swap(current_counts, previous_counts);
	}

      size_cache = previous_counts.at(initial_state);

      if(hash)
	{
	  mkdir((ScratchConfig::get_local_dir() + "/sizes").c_str(), 0700);
	  MemoryMap<double> cached(get_size_file_name(*hash), size_t(1));
	  cached[0] = size_cache;
	  cached.msync();
	}
    }

  assert(size_cache >= 1.0);

  return size_cache;
}

size_t DFA::states() const
{
  size_t states_out = 0;

  for(int layer = 0; layer < ndim; ++layer)
    {
      size_t layer_size = get_layer_size(layer);
      assert(layer_size > 0);
      states_out += layer_size;
    }

  return states_out;
}

DFAIterator::DFAIterator(const DFA& dfa_in, const std::vector<int>& characters_in)
  : ndim(int(dfa_in.get_shape().size())),
    dfa(dfa_in),
    characters(characters_in),
    states()
{
  assert(characters.size() == ndim);

  if(characters[0] < dfa.get_shape()[0])
    {
      // not at end
      for(int i = 1; i < ndim; ++i)
	{
	  assert(characters[i] < dfa.get_shape()[i]);
	}

      states.push_back(dfa.get_initial_state());
      for(int layer = 0; layer < ndim; ++layer)
	{
	  states.push_back(dfa.get_transitions(layer, states[layer]).at(characters[layer]));
	  assert(states.at(layer + 1) < dfa.get_layer_size(layer + 1));
	}
      assert(states.size() == ndim + 1);
      assert(states[ndim] == 1);
    }
  else
    {
      // unique end
      assert(characters[0] == dfa.get_shape()[0]);
      for(int i = 1; i < ndim; ++i)
	{
	  assert(characters[i] == 0);
	}
    }
}

DFAString DFAIterator::operator*() const
{
  assert(characters[0] < dfa.get_shape()[0]);

  return DFAString(dfa.get_shape(), characters);
}

DFAIterator& DFAIterator::operator++()
{
  assert(characters[0] < dfa.get_shape()[0]);
  assert(states.size() == ndim + 1);
  assert(states[ndim] == 1);

  // advancing is like incrementing a number with carrying, except we
  // also have to skip over non-accepting states.

  states.pop_back();
  assert(states.size() == characters.size());
  while(states.size())
    {
      assert(states.size() == characters.size());

      int layer = int(states.size()) - 1;
      int layer_shape = dfa.get_layer_shape(layer);

      DFATransitionsReference transitions = dfa.get_transitions(layer, states[layer]);

      // scan for the next accepting character choice
      assert(characters[layer] < layer_shape);
      for(++characters[layer]; // initial advancement
	  ((characters[layer] < layer_shape) &&
	   !transitions[characters[layer]]);
	  ++characters[layer])
	{
	}
      if(characters[layer] < layer_shape)
	{
	  // found an accepting character/state
	  assert(states.size() == characters.size());
	  states.push_back(transitions[characters[layer]]);
	  assert(states.size() == characters.size() + 1);
	  break;
	}

      // no more character choices work at this layer
      characters.pop_back();
      states.pop_back();
    }

  if(states.size() == 0)
    {
      // no more accepting strings. states stays empty, matching cend().
      characters.push_back(dfa.get_shape()[0]);
      for(int layer = 1; layer < ndim; ++layer)
	{
	  characters.push_back(0);
	}

      return *this;
    }

  // fill forward from accepting state found

  assert(states.size() == characters.size() + 1);

  for(int layer = int(characters.size()); layer < ndim; ++layer)
    {
      // figure out first matching character for next layer
      int layer_shape = dfa.get_layer_shape(layer);
      DFATransitionsReference transitions = dfa.get_transitions(layer, states[layer]);

      for(characters.push_back(0);
	  ((characters[layer] < layer_shape) &&
	   (transitions[characters[layer]] == 0));
	  ++characters[layer])
	{
	}

      assert(characters.at(layer) < layer_shape);
      assert(transitions[characters[layer]]);

      states.push_back(transitions[characters[layer]]);
    }
  assert(states.size() == ndim + 1);
  assert(states[ndim] == 1);
  assert(characters.size() == ndim);

  // done

  return *this;
}

bool DFAIterator::operator<(const DFAIterator& right_in) const
{
  for(int i = 0; i < ndim; ++i)
    {
      int l = characters[i];
      int r = right_in.characters[i];
      if(l < r)
	{
	  return true;
	}
      else if(l > r)
	{
	  return false;
	}
    }

  return false;
}

DFALinearBound::DFALinearBound(const dfa_shape_t& shape_in, const std::vector<std::vector<bool>>& bounds_in)
  : shape(shape_in),
    bounds(bounds_in)
{
}

bool DFALinearBound::operator<=(const DFALinearBound& bounds_right) const
{
  assert(shape == bounds_right.shape);

  int ndim = int(shape.size());
  for(int layer = 0; layer < ndim; ++layer)
    {
      int layer_shape = shape[layer];
      for(int c = 0; c < layer_shape; ++c)
	{
	  if(bounds[layer][c] && !bounds_right.bounds[layer][c])
	    {
	      return false;
	    }
	}
    }

  return true;
}

bool DFALinearBound::check_bound(int layer_in, int character_in) const
{
  assert(0 <= layer_in);
  assert(layer_in < shape.size());

  int layer_shape = shape[layer_in];
  assert(0 <= character_in);
  assert(character_in < layer_shape);

  return bounds[layer_in][character_in];
}

bool DFALinearBound::check_fixed(int layer_in, int character_in) const
{
  assert(0 <= layer_in);
  assert(layer_in < shape.size());

  int layer_shape = shape[layer_in];
  assert(character_in < shape[layer_in]);
  for(int c = 0; c < layer_shape; ++c)
    {
      if(bounds[layer_in][c] && (c != character_in))
	{
	  return false;
	}
    }

  return true;
}

DFAString::DFAString(const dfa_shape_t& shape_in, const std::vector<int>& characters_in)
  : characters(characters_in)
{
  int ndim = int(shape_in.size());
  assert(characters.size() == ndim);

  for(int i = 0; i < ndim; ++i)
    {
      assert(characters.at(i) < shape_in.at(i));
    }
}

bool DFAString::operator<(const DFAString& right) const
{
  int ndim = int(characters.size());
  assert(right.characters.size() == ndim);

  for(int i = 0; i < ndim; ++i)
    {
      if(characters[i] < right.characters[i])
        {
          return true;
        }
      else if(characters[i] > right.characters[i])
        {
          return false;
        }
    }

  return false;
}

bool DFAString::operator==(const DFAString& right) const
{
  int ndim = int(characters.size());
  assert(right.characters.size() == ndim);

  for(int i = 0; i < ndim; ++i)
    {
      if(characters[i] != right.characters[i])
        {
          return false;
        }
    }

  return true;
}

int DFAString::operator[](int layer_in) const
{
  return characters.at(layer_in);
}

int DFAString::get_size() const
{
  return int(characters.size());
}

std::string DFAString::to_string() const
{
  std::string output("");

  output += "[";
  for(int i = 0; i < characters.size() - 1; ++i)
    {
      output += std::to_string(characters[i]) + ", ";
    }
  if(characters.size() > 0)
    {
      output += std::to_string(characters.back());
    }
  output += "]";

  return output;
}
