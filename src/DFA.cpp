// DFA.cpp

#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <iomanip>
#include <numeric>
#include <ranges>
#include <sstream>
#include <string>

#include "DFA.h"
#include "DFAFormat.h"
#include "Profile.h"
#include "parallel.h"
#include "utils.h"

static int next_dfa_id = 0;

// Staging directory for a DFA under construction.
//
// The counter is per process, so it alone does not make the name unique:
// two concurrent processes both start at zero and march through the same
// directories, overwriting each other's layer files. Mixing in the pid is
// what makes the name actually unique, and a pid cannot be reused while
// this process holds it.
static std::string get_temp_directory()
{
  return ("scratch/temp/" +
	  std::to_string(getpid()) + "-" +
	  std::to_string(next_dfa_id++));
}

std::vector<std::string> get_layer_file_names(int ndim, std::string directory)
{
  std::vector<std::string> output;

  for(int layer = 0; layer < ndim; ++layer)
    {
      output.push_back(directory + "/layer=" + std::to_string(layer));
    }

  return output;
}

std::string create_directory(std::string directory)
{
  mkdir(directory.c_str(), 0700);
  return directory;
}

void remove_directory(std::string directory)
{
  DIR *dir = opendir(directory.c_str());
  if(dir)
    {
      // DFA was previously saved with this name?

      for(struct dirent *dirent = readdir(dir);
	  dirent;
	  dirent = readdir(dir))
	{
	  if(strncmp(dirent->d_name, ".", sizeof(dirent->d_name)) &&
	     strncmp(dirent->d_name, "..", sizeof(dirent->d_name)))
	    {
	      std::string old_file_name = directory + "/" + dirent->d_name;
	      int unlink_ret = unlink(old_file_name.c_str());
	      if(unlink_ret)
		{
		  perror("DFA save unlink");
		  throw std::runtime_error("DFA save unlink failed");
		}
	    }
	}

      closedir(dir);

      int rmdir_ret = rmdir(directory.c_str());
      if(rmdir_ret)
	{
	  perror("DFA save rmdir");
	  throw std::runtime_error("DFA save rmdir failed");
	}
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
  assert(ndim > 0);

  mkdir(directory.c_str(), 0700);

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
// directly; any other name is a symbolic link to one.
static std::string get_file_name(std::string name_in)
{
  if(name_in.starts_with("dfas_by_hash/"))
    {
      return "scratch/" + name_in + ".dfa";
    }

  return "scratch/" + name_in;
}

DFA::DFA(const dfa_shape_t& shape_in, std::string name_in)
  : shape(),
    ndim(0),
    name(name_in),
    layer_sizes(),
    directory(),
    layer_file_names(),
    layer_transitions(),
    temporary(false)
{
  load_file(get_file_name(name_in));

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
// Every check FORMAT-DFA.md section 7 requires of a reader is done here.
// Failures throw, which is what DFAUtil::_try_load turns into "not found".
void DFA::load_file(std::string file_name_in)
{
  file_name = file_name_in;
  file_map = new MemoryMap<uint8_t>(file_name_in, true);

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

  if(fsync(fildes) || close(fildes))
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

std::string DFA::get_hash() const
{
  assert(ready());

  // The hash is the file's own digest, so it does not exist until the DFA has
  // been written.
  if(!hash)
    {
      save_by_hash();
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

      std::vector<std::vector<bool>> bounds;
      bool reached_accept_all = initial_state == 1;

      mmap();

      for(int layer = 0; layer < ndim; ++layer)
	{
	  size_t layer_size = get_layer_size(layer);
	  int layer_shape = get_layer_shape(layer);
	  if(reached_accept_all)
	    {
	      bounds.emplace_back(layer_shape, true);
	      continue;
	    }

	  bounds.emplace_back(layer_shape, false);

	  std::vector<bool>& curr_bounds = bounds[layer];

	  // narrow shape case

	  if(layer_shape <= 32)
	    {
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

	      std::ranges::iota_view state_view(size_t(2), layer_size);

	      std::pair<bool, uint32_t> combined_bounds =
		TRY_PARALLEL_5(std::transform_reduce,
			       state_view.begin(),
			       state_view.end(),
			       (std::pair<bool, uint32_t>(false, 0)),
			       reduce_local,
			       get_local);

	      if(std::get<0>(combined_bounds))
		{
		  reached_accept_all = true;
		}
	      for(int i = 0; i < 32; ++i)
		{
		  if(std::get<1>(combined_bounds) & (1 << i))
		    {
		      curr_bounds[i] = true;
		    }
		}

	      continue;
	    }

	  // general shape case

	  for(size_t state_id = 2; state_id < layer_size; ++state_id)
	    {
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
      // Saved: entries are stored at the width the format derives.
      file_map->mmap();
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

std::optional<std::string> DFA::parse_hash(std::string name_in)
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

  // read symbolic link which should be pointing to hash directory.

  std::string directory = "scratch/" + name_in;
  char link_target[1024] = {0};
  ssize_t ret = readlink(directory.c_str(), link_target, sizeof(link_target) - 1);
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

void DFA::save(std::string name_in) const
{
  assert(!name_in.starts_with("dfas_by_hash/"));

  save_by_hash();

  std::string symlink_path = std::string("scratch/") + name_in;

  // add symbolic link to the existing file in dfas_by_hash/
  std::string symlink_target = "dfas_by_hash/" + get_hash() + ".dfa";
  for(char c : name_in)
    {
      if(c == '/')
	{
	  symlink_target = "../" + symlink_target;
	}
    }

  unlink(symlink_path.c_str());

  int ret = symlink(symlink_target.c_str(), symlink_path.c_str());
  if(ret)
    {
      perror(("DFA save symlink " + symlink_path).c_str());
      throw std::runtime_error("DFA save symlink failed");
    }

  name = name_in;
}

void DFA::save_by_hash() const
{
  assert(ready());
  if(!temporary)
    {
      return;
    }

  mkdir("scratch/dfas_by_hash", 0700);

  // Write under a temporary name, since the final name is the digest of the
  // bytes and is not known until they have all been written.
  static int next_serialize_id = 0;
  std::string temporary_name = ("scratch/dfas_by_hash/.tmp-" +
				std::to_string(getpid()) + "-" +
				std::to_string(next_serialize_id++) + ".dfa");

  std::string digest = serialize(temporary_name);
  std::string file_name_new = "scratch/dfas_by_hash/" + digest + ".dfa";

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

  hash = digest;

  // Switch this object over to the file, and drop the staging directory.
  attach_file(file_name_new);

  layer_transitions.clear();
  layer_file_names.clear();
  remove_directory(directory);
  directory = "";
  temporary = false;
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
  return "scratch/sizes/" + hash_in;
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
	  mkdir("scratch/sizes", 0700);
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
  : shape(dfa_in.get_shape()),
    ndim(int(shape.size())),
    dfa(dfa_in),
    characters(characters_in)
{
  assert(characters.size() == ndim);

  if(characters[0] < shape[0])
    {
      // not at end
      for(int i = 1; i < ndim; ++i)
	{
	  assert(characters[i] < shape[i]);
	}
    }
  else
    {
      // unique end
      assert(characters[0] == shape[0]);
      for(int i = 1; i < ndim; ++i)
	{
	  assert(characters[i] == 0);
	}
    }
}

DFAString DFAIterator::operator*() const
{
  assert(characters[0] < shape[0]);

  return DFAString(shape, characters);
}

DFAIterator& DFAIterator::operator++()
{
  assert(characters[0] < shape[0]);

  std::vector<dfa_state_t> states;
  states.push_back(dfa.get_initial_state());
  for(int layer = 0; layer < ndim; ++layer)
    {
      assert(characters.at(layer) < shape[layer]);
      states.push_back(dfa.get_transitions(layer, states[layer]).at(characters[layer]));
      assert(states.at(layer + 1) < dfa.get_layer_size(layer + 1));
    }
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
      // no more accepting strings
      characters.push_back(shape[0]);
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
  : shape(shape_in),
    characters(characters_in)
{
  int ndim = int(shape.size());
  assert(characters.size() == ndim);

  for(int i = 0; i < ndim; ++i)
    {
      assert(characters.at(i) < shape.at(i));
    }
}

bool DFAString::operator<(const DFAString& right) const
{
  int ndim = int(shape.size());
  assert(right.shape.size() == ndim);

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
  int ndim = int(shape.size());
  assert(right.shape.size() == ndim);

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

const dfa_shape_t& DFAString::get_shape() const
{
  return shape;
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
