// DFA.cpp

#include <assert.h>
#include <dirent.h>
#include <openssl/evp.h>
#include <openssl/sha.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <iomanip>
#include <sstream>
#include <string>

#include "DFA.h"
#include "Profile.h"
#include "parallel.h"

static int next_dfa_id = 0;

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

DFATransitionsReference::DFATransitionsReference(const MemoryMap<dfa_state_t>& layer_transitions_in,
						 dfa_state_t state_in,
						 int layer_shape_in)
  : layer_transitions(layer_transitions_in),
    offset(size_t(state_in) * size_t(layer_shape_in)),
    layer_shape(layer_shape_in)
{
  assert(layer_shape > 0);
  size_t size_temp = layer_transitions.size();
  assert(offset < size_temp);
  assert(offset + layer_shape <= size_temp);
}

DFATransitionsReference::DFATransitionsReference(const DFATransitionsReference& reference_in)
  : layer_transitions(reference_in.layer_transitions),
    offset(reference_in.offset),
    layer_shape(reference_in.layer_shape)
{
  size_t size_temp = layer_transitions.size();
  assert(offset < size_temp);

  assert(layer_shape > 0);
  assert(offset + layer_shape <= size_temp);
}

DFA::DFA(const dfa_shape_t& shape_in)
  : shape(shape_in),
    ndim(shape.size()),
    directory(create_directory("scratch/temp/" + std::to_string(next_dfa_id++))),
    layer_file_names(get_layer_file_names(shape_in.size(), directory)),
    layer_sizes(),
    layer_transitions(),
    size_cache(directory + "/size_cache", 1),
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
      layer_transitions.emplace_back(layer_file_names.at(layer), 1024);

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

  // finish size_cache initialization
  size_cache[0] = 0.0;
}

DFA::DFA(const dfa_shape_t& shape_in, std::string name_in)
  : shape(shape_in),
    ndim(shape.size()),
    directory("scratch/" + name_in),
    name(name_in),
    layer_file_names(get_layer_file_names(ndim, directory)),
    layer_sizes(),
    layer_transitions(),
    size_cache(directory + "/size_cache", 1),
    temporary(false)
{
  assert(shape.size() == ndim);

  for(int layer = 0; layer < ndim; ++layer)
    {
      layer_transitions.emplace_back(layer_file_names.at(layer));

      int layer_shape = get_layer_shape(layer);
      assert(layer_transitions[layer].size() % layer_shape == 0);
      layer_sizes.push_back(layer_transitions[layer].size() / layer_shape);
    }

  MemoryMap<dfa_state_t> initial_state_mmap(directory + "/initial_state");
  if(initial_state_mmap.size() != 1)
    {
      throw std::runtime_error("initial_state file has an invalid size");
    }
  set_initial_state(initial_state_mmap[0]);

  hash = parse_hash(name_in);
  assert(hash);
  assert(hash->length() == 64);
}

DFA::~DFA() noexcept(false)
{
  if(temporary)
    {
      remove_directory(directory);
    }

  if(linear_bound)
    {
      delete linear_bound;
      linear_bound = 0;
    }
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

  assert(layer_sizes[layer] < UINT32_MAX);

  size_t current_offset = size_t(layer_sizes[layer]) * size_t(layer_shape);
  size_t next_offset = current_offset + size_t(layer_shape);

  MemoryMap<dfa_state_t>& current_transitions = layer_transitions[layer];
  size_t current_size = current_transitions.size();
  if(next_offset > current_size)
    {
      size_t next_size = current_size * 2;
      assert(next_size <= size_t(UINT32_MAX));
      current_transitions = MemoryMap<dfa_state_t>(layer_file_names[layer], next_size);
    }

  dfa_state_t transition_bound = this->get_layer_size(layer + 1);
  for(int i = 0; i < layer_shape; ++i)
    {
      assert(transitions[i] < transition_bound);
      current_transitions[current_offset + i] = transitions[i];
    }

  return layer_sizes[layer]++;
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

void DFA::copy_layer(int layer, const DFA& dfa_in)
{
  assert(dfa_in.ready());

  set_layer_size(layer, dfa_in.get_layer_size(layer));
  assert(layer_transitions[layer].size() == dfa_in.layer_transitions[layer].size());

  // make sure the source layer is mapped
  dfa_in.layer_transitions[layer].mmap();

  TRY_PARALLEL_3(std::copy,
                 dfa_in.layer_transitions[layer].begin(),
                 dfa_in.layer_transitions[layer].end(),
                 layer_transitions[layer].begin());
}

void DFA::set_initial_state(dfa_state_t initial_state_in)
{
  assert(initial_state == ~dfa_state_t(0));

  assert(initial_state_in < get_layer_size(0));
  initial_state = initial_state_in;

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

void DFA::set_layer_size(int layer, dfa_state_t layer_size_in)
{
  assert(initial_state == ~dfa_state_t(0));
  assert(0 <= layer);
  assert(layer < ndim);

  assert(layer_sizes[layer] == 2);
  assert(2 <= layer_size_in);
  assert(layer_size_in < ~dfa_state_t(0));

  layer_sizes[layer] = layer_size_in;
  layer_transitions[layer] = MemoryMap<dfa_state_t>(layer_file_names[layer], layer_size_in * get_layer_shape(layer));
}

void DFA::set_state_transitions(int layer, dfa_state_t state_in, const dfa_state_t *transitions_in)
{
  assert(initial_state == ~dfa_state_t(0));
  assert(0 <= layer);
  assert(layer < ndim);

  assert(2 <= state_in);
  assert(state_in < layer_sizes[layer]);

  int layer_shape = get_layer_shape(layer);
  dfa_state_t *transitions = layer_transitions[layer].begin() + state_in * layer_shape;
  for(int i = 0; i < layer_shape; ++i)
    {
      *transitions++ = *transitions_in++;
    }
}

std::string DFA::calculate_hash() const
{
  Profile profile("calculate_hash");

  assert(ready());

  unsigned char hash_output[SHA256_DIGEST_LENGTH];
  static const EVP_MD *hash_implementation = EVP_sha256();
  static EVP_MD_CTX *hash_context = EVP_MD_CTX_create();

  EVP_DigestInit_ex(hash_context, hash_implementation, NULL);
  EVP_DigestUpdate(hash_context, &initial_state, sizeof(initial_state));
  EVP_DigestUpdate(hash_context, layer_sizes.data(), layer_sizes.size() * sizeof(layer_sizes[0]));
  for(int layer = 0; layer < ndim; ++layer)
    {
      EVP_DigestUpdate(hash_context, &(layer_transitions[layer][0]), layer_transitions[layer].length());
    }
  EVP_DigestFinal_ex(hash_context, hash_output, 0);

  std::stringstream ss;
  for(int i = 0; i < SHA256_DIGEST_LENGTH; i++)
    {
      ss << std::hex << std::setw(2) << std::setfill('0') << (int)hash_output[i];
    }

  return ss.str();
}

DFAIterator DFA::cbegin() const
{
  if(initial_state == 0)
    {
      return cend();
    }

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

std::string DFA::get_hash() const
{
  assert(ready());

  if(!hash)
    {
      hash = calculate_hash();
    }

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

dfa_state_t DFA::get_layer_size(int layer) const
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
	  const MemoryMap<dfa_state_t>& curr_transitions = layer_transitions[layer];

	  size_t num_transitions = get_layer_size(layer) * layer_shape;
	  for(size_t i = 2 * layer_shape; i < num_transitions; ++i)
	    {
	      dfa_state_t t = curr_transitions[i];
	      if(t == 1)
		{
		  reached_accept_all = true;
		}
	      if(t)
		{
		  curr_bounds[i % layer_shape] = true;
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
  return shape.size();
}

DFATransitionsReference DFA::get_transitions(int layer, dfa_state_t state_index) const
{
  assert(layer < ndim);
  assert(state_index < layer_sizes[layer]);
  return DFATransitionsReference(layer_transitions[layer], state_index, get_layer_shape(layer));
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

void DFA::munmap() const
{
  for(int layer = 0; layer < ndim; ++layer)
    {
      layer_transitions[layer].munmap();
    }
}

std::optional<std::string> DFA::parse_hash(std::string name_in)
{
  std::string hash_prefix = "dfas_by_hash/";
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
      int hash_offset = link_target_string.length() - hash_prefix.length() - 64;
      assert(hash_offset >= 0);
      if((hash_offset >= 0) &&
	 (link_target_string.substr(hash_offset, hash_prefix.length()) == hash_prefix))
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

  // add symbolic link to existing directory in dfas_by_hash/
  std::string symlink_target = "dfas_by_hash/" + get_hash();
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

  std::string directory_new = std::string("scratch/dfas_by_hash/") + get_hash();
  // TODO: check if hash directory already exists and share carefully

  // flush state to disk

  for(int layer = 0; layer < ndim; ++layer)
    {
      layer_transitions.at(layer).msync();
    }

  size_cache.msync();

  // write initial state to disk
  MemoryMap<dfa_state_t> initial_state_mmap(directory + "/initial_state", 1);
  initial_state_mmap[0] = initial_state;
  initial_state_mmap.msync();

  remove_directory(directory_new);

  int ret = rename(directory.c_str(), directory_new.c_str());
  if(ret)
    {
      perror("DFA save rename");
      throw std::runtime_error("DFA save rename failed");
    }

  // repoint internal state at new directory

  directory = directory_new;
  layer_file_names = get_layer_file_names(shape.size(), directory_new);

  for(int layer = 0; layer < ndim; ++layer)
    {
      // switch layer memory maps to the new file names so they do not
      // break on munmap. has an implied flush.
      layer_transitions[layer] = MemoryMap<dfa_state_t>(layer_file_names[layer]);
      assert(layer_transitions[layer].size() == layer_sizes[layer] * shape[layer]);
    }

  temporary = false;
}

void DFA::set_name(std::string name_in) const
{
  name = name_in;
}

double DFA::size() const
{
  assert(ready());

  if(initial_state == 0)
    {
      assert(size_cache[0] == 0.0);
      return 0.0;
    }

  if(size_cache[0] == 0.0)
    {
      std::vector<double> previous_counts({0, 1}); // reject, accept
      for(int layer = ndim - 1; layer >= 0; --layer)
	{
	  int layer_shape = this->get_layer_shape(layer);

	  std::vector<double> current_counts;
	  dfa_state_t layer_size = get_layer_size(layer);
	  for(dfa_state_t state_index = 0; state_index < layer_size; ++state_index)
	    {
	      DFATransitionsReference transitions = this->get_transitions(layer, state_index);

	      double state_count = 0;
	      for(int i = 0; i < layer_shape; ++i)
		{
		  state_count += previous_counts.at(transitions[i]);
		}
	      current_counts.push_back(state_count);
	    }

	  previous_counts = current_counts;
	}

      size_cache[0] = previous_counts.at(initial_state);
    }

  assert(size_cache[0] >= 1.0);

  return size_cache[0];
}

size_t DFA::states() const
{
  size_t states_out = 0;

  for(int layer = 0; layer < ndim; ++layer)
    {
      dfa_state_t layer_size = get_layer_size(layer);
      assert(layer_size > 0);
      states_out += layer_size;
    }

  return states_out;
}

DFAIterator::DFAIterator(const DFA& dfa_in, const std::vector<int>& characters_in)
  : shape(dfa_in.get_shape()),
    ndim(shape.size()),
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

      int layer = states.size() - 1;
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

  for(int layer = characters.size(); layer < ndim; ++layer)
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

  int ndim = shape.size();
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
  int ndim = shape.size();
  assert(characters.size() == ndim);

  for(int i = 0; i < ndim; ++i)
    {
      assert(characters.at(i) < shape.at(i));
    }
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
