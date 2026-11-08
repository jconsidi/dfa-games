// ConnectedDFA.cpp

#include "ConnectedDFA.h"

#include <algorithm>
#include <cassert>
#include <format>
#include <iomanip>
#include <map>
#include <numeric>
#include <set>
#include <sstream>
#include <stdexcept>

#include <openssl/evp.h>
#include <openssl/sha.h>

static connected_mask_t class_bit(int c)
{
  return connected_mask_t(1) << c;
}

static std::string sha256_hex(const std::string& input)
{
  unsigned char digest[SHA256_DIGEST_LENGTH];
  static const EVP_MD *hash_implementation = EVP_sha256();
  EVP_MD_CTX *hash_context = EVP_MD_CTX_create();
  EVP_DigestInit_ex(hash_context, hash_implementation, NULL);
  EVP_DigestUpdate(hash_context, input.data(), input.size());
  EVP_DigestFinal_ex(hash_context, digest, 0);
  EVP_MD_CTX_destroy(hash_context);

  std::ostringstream ss;
  for(size_t i = 0; i < SHA256_DIGEST_LENGTH; ++i)
    {
      ss << std::hex << std::setw(2) << std::setfill('0') << int(digest[i]);
    }
  return ss.str();
}

static std::vector<std::pair<int, int>> sorted_edges(const std::vector<std::pair<int, int>>& edges)
{
  std::vector<std::pair<int, int>> output;
  for(auto [a, b] : edges)
    {
      output.emplace_back(std::min(a, b), std::max(a, b));
    }
  std::sort(output.begin(), output.end());
  return output;
}

std::string ConnectedSpec::cache_key(const dfa_shape_t& shape) const
{
  std::ostringstream builder;

  builder << "shape";
  for(int layer_shape : shape)
    {
      builder << " " << layer_shape;
    }

  builder << "\nedges";
  for(auto [a, b] : sorted_edges(edges))
    {
      builder << " " << a << "-" << b;
    }

  builder << "\nclass_of";
  for(const auto& layer_classes : class_of)
    {
      builder << " ";
      for(size_t c = 0; c < layer_classes.size(); ++c)
        {
          builder << (c ? "," : "") << layer_classes[c];
        }
    }

  builder << "\nlabels";
  for(connected_mask_t layer_labels : labels)
    {
      builder << " " << layer_labels;
    }

  builder << "\ngroup_classes " << group_classes;
  builder << "\nlabels_required " << labels_required;
  builder << "\nlabels_forbidden " << labels_forbidden;
  builder << "\nadjacent_required " << adjacent_required;
  builder << "\nadjacent_forbidden " << adjacent_forbidden;
  builder << "\ncount " << count_min << " " << count_max << "\n";

  return sha256_hex(builder.str());
}

void ConnectedSpec::validate(const dfa_shape_t& shape) const
{
  auto fail = [](const std::string& message)
  {
    throw std::invalid_argument("ConnectedSpec: " + message);
  };

  int ndim = int(shape.size());
  if(ndim < 1)
    {
      fail("shape has no layers");
    }
  if(int(class_of.size()) != ndim)
    {
      fail(std::format("class_of has {:d} layers, shape has {:d}", class_of.size(), ndim));
    }
  if(int(labels.size()) != ndim)
    {
      fail(std::format("labels has {:d} layers, shape has {:d}", labels.size(), ndim));
    }

  connected_mask_t classes_seen = 0;
  connected_mask_t labels_seen = 0;
  for(int layer = 0; layer < ndim; ++layer)
    {
      if(int(class_of[layer].size()) != shape[layer])
        {
          fail(std::format("class_of layer {:d} has {:d} characters, shape has {:d}", layer, class_of[layer].size(), shape[layer]));
        }
      for(int k : class_of[layer])
        {
          if((k < -1) || (k >= 32))
            {
              fail(std::format("class_of layer {:d} has class {:d}, outside [-1, 32)", layer, k));
            }
          if(k >= 0)
            {
              classes_seen |= class_bit(k);
            }
        }
      labels_seen |= labels[layer];
    }

  std::set<std::pair<int, int>> edges_seen;
  for(auto [a, b] : edges)
    {
      if((a < 0) || (a >= ndim) || (b < 0) || (b >= ndim))
        {
          fail(std::format("edge {:d}-{:d} is outside layers [0, {:d})", a, b, ndim));
        }
      if(a == b)
        {
          fail(std::format("edge {:d}-{:d} is a self loop", a, b));
        }
      if(!edges_seen.emplace(std::min(a, b), std::max(a, b)).second)
        {
          fail(std::format("edge {:d}-{:d} is listed more than once", a, b));
        }
    }

  if(group_classes == 0)
    {
      fail("group_classes is empty");
    }
  if((group_classes | adjacent_required | adjacent_forbidden) & ~classes_seen)
    {
      fail("the condition names a class that no character has");
    }
  if((labels_required | labels_forbidden) & ~labels_seen)
    {
      fail("the condition names a label that no layer carries");
    }
  if(labels_required & labels_forbidden)
    {
      fail("a label is both required and forbidden");
    }
  if(adjacent_required & adjacent_forbidden)
    {
      fail("an adjacent class is both required and forbidden");
    }

  if(count_min < 0)
    {
      fail(std::format("count_min {:d} is negative", count_min));
    }
  if((count_max >= 0) && (count_max < count_min))
    {
      fail(std::format("count_max {:d} is below count_min {:d}", count_max, count_min));
    }
  if((count_max < 0) && (count_min == 0))
    {
      fail("count range [0, unbounded) accepts every position");
    }
}

namespace
{
  // One logical state, entering a layer. Cells are the frontier cells in
  // layer order; groups are numbered by first appearance among them.
  struct LogicalState
  {
    int count = 0;
    std::vector<int> cell_class;
    std::vector<int> cell_group;
    std::vector<connected_mask_t> group_labels;
    std::vector<connected_mask_t> group_adjacent;

    std::vector<uint32_t> encode() const
    {
      std::vector<uint32_t> output;
      output.push_back(uint32_t(count));
      output.push_back(uint32_t(group_labels.size()));
      output.insert(output.end(), group_labels.begin(), group_labels.end());
      output.insert(output.end(), group_adjacent.begin(), group_adjacent.end());
      for(size_t i = 0; i < cell_class.size(); ++i)
        {
          output.push_back(uint32_t(cell_class[i] + 1));
          output.push_back(uint32_t(cell_group[i] + 1));
        }
      return output;
    }

    static LogicalState decode(const std::vector<uint32_t>& input, size_t frontier_size)
    {
      LogicalState output;
      output.count = int(input.at(0));
      size_t num_groups = input.at(1);
      assert(input.size() == 2 + 2 * num_groups + 2 * frontier_size);

      size_t offset = 2;
      for(size_t g = 0; g < num_groups; ++g)
        {
          output.group_labels.push_back(input[offset++]);
        }
      for(size_t g = 0; g < num_groups; ++g)
        {
          output.group_adjacent.push_back(input[offset++]);
        }
      for(size_t i = 0; i < frontier_size; ++i)
        {
          output.cell_class.push_back(int(input[offset++]) - 1);
          output.cell_group.push_back(int(input[offset++]) - 1);
        }
      return output;
    }
  };
}

// Runs before the DFA base class starts a build, so a rejected spec throws
// without leaving a build half started.
static const dfa_shape_t& validated_shape(const dfa_shape_t& shape_in, const ConnectedSpec& spec)
{
  spec.validate(shape_in);
  return shape_in;
}

ConnectedDFA::ConnectedDFA(const dfa_shape_t& shape_in, const ConnectedSpec& spec)
  : DedupedDFA(validated_shape(shape_in, spec))
{
  int ndim = get_shape_size();

  connected_mask_t relevant_adjacent = spec.adjacent_required | spec.adjacent_forbidden;
  connected_mask_t relevant_classes = spec.group_classes | relevant_adjacent;
  connected_mask_t relevant_labels = spec.labels_required | spec.labels_forbidden;

  auto matches = [&](connected_mask_t group_labels, connected_mask_t group_adjacent)
  {
    return (((group_labels & spec.labels_required) == spec.labels_required) &&
            ((group_labels & spec.labels_forbidden) == 0) &&
            ((group_adjacent & spec.adjacent_required) == spec.adjacent_required) &&
            ((group_adjacent & spec.adjacent_forbidden) == 0));
  };

  // the class of each character as far as the condition can tell, -1 when
  // nothing depends on it.
  auto effective_class = [&](int layer, int c)
  {
    int k = spec.class_of[layer][c];
    return ((k >= 0) && (relevant_classes & class_bit(k))) ? k : -1;
  };

  // geometry: neighbours read earlier, the last layer each cell is needed
  // through, and the frontier entering each layer.

  std::vector<std::vector<int>> earlier_neighbours(ndim);
  std::vector<int> last_needed(ndim);
  std::iota(last_needed.begin(), last_needed.end(), 0);
  for(auto [a, b] : sorted_edges(spec.edges))
    {
      earlier_neighbours[b].push_back(a);
      last_needed[a] = std::max(last_needed[a], b);
    }

  std::vector<std::vector<int>> frontier(ndim + 1);
  std::vector<std::map<int, int>> frontier_index(ndim + 1);
  for(int layer = 0; layer <= ndim; ++layer)
    {
      for(int u = 0; u < layer; ++u)
        {
          if(last_needed[u] >= layer)
            {
              frontier_index[layer][u] = int(frontier[layer].size());
              frontier[layer].push_back(u);
            }
        }
    }
  assert(frontier[ndim].size() == 0);

  // forward pass: reachable logical states per layer, and their transitions.
  // a transition is 0 (reject), 1 (accept) or 2 + the index of a state
  // entering the next layer.

  std::vector<std::vector<std::vector<uint32_t>>> layer_states(ndim);
  std::vector<std::vector<size_t>> layer_transitions(ndim);

  layer_states[0].push_back(LogicalState().encode());

  for(int layer = 0; layer < ndim; ++layer)
    {
      int layer_shape = get_layer_shape(layer);
      const std::vector<int>& frontier_in = frontier[layer];
      std::map<std::vector<uint32_t>, size_t> next_lookup;

      for(size_t state_index = 0; state_index < layer_states[layer].size(); ++state_index)
        {
          LogicalState state_in = LogicalState::decode(layer_states[layer][state_index], frontier_in.size());
          int num_groups_in = int(state_in.group_labels.size());

          for(int c = 0; c < layer_shape; ++c)
            {
              int cls = effective_class(layer, c);
              bool grouped = (cls >= 0) && (spec.group_classes & class_bit(cls));

              // raw group ids: the incoming groups, then the new cell's own
              std::vector<connected_mask_t> raw_labels = state_in.group_labels;
              std::vector<connected_mask_t> raw_adjacent = state_in.group_adjacent;
              int new_group = -1;
              if(grouped)
                {
                  new_group = num_groups_in;
                  raw_labels.push_back(spec.labels[layer] & relevant_labels);
                  raw_adjacent.push_back(0);
                }
              int num_raw = int(raw_labels.size());

              std::vector<int> parent(num_raw);
              std::iota(parent.begin(), parent.end(), 0);
              auto find = [&](int g)
              {
                while(parent[g] != g)
                  {
                    parent[g] = parent[parent[g]];
                    g = parent[g];
                  }
                return g;
              };

              for(int u : earlier_neighbours[layer])
                {
                  int i = frontier_index[layer].at(u);
                  int cu = state_in.cell_class[i];
                  int gu = state_in.cell_group[i];
                  if(cu < 0)
                    {
                      continue;
                    }

                  if(grouped && (cu == cls))
                    {
                      assert(gu >= 0);
                      parent[find(gu)] = find(new_group);
                      continue;
                    }

                  if(grouped)
                    {
                      raw_adjacent[new_group] |= class_bit(cu) & relevant_adjacent;
                    }
                  if((gu >= 0) && (cls >= 0))
                    {
                      raw_adjacent[gu] |= class_bit(cls) & relevant_adjacent;
                    }
                }

              std::vector<connected_mask_t> root_labels(num_raw, 0);
              std::vector<connected_mask_t> root_adjacent(num_raw, 0);
              for(int g = 0; g < num_raw; ++g)
                {
                  int r = find(g);
                  root_labels[r] |= raw_labels[g];
                  root_adjacent[r] |= raw_adjacent[g];
                }

              // frontier leaving this layer, with groups renumbered by first
              // appearance.

              LogicalState state_out;
              state_out.count = state_in.count;

              std::vector<int> renumber(num_raw, -1);
              auto add_cell = [&](int cell_class, int raw_group)
              {
                int group_out = -1;
                if(raw_group >= 0)
                  {
                    int r = find(raw_group);
                    if(renumber[r] < 0)
                      {
                        renumber[r] = int(state_out.group_labels.size());
                        state_out.group_labels.push_back(root_labels[r]);
                        state_out.group_adjacent.push_back(root_adjacent[r]);
                      }
                    group_out = renumber[r];
                  }
                state_out.cell_class.push_back(cell_class);
                state_out.cell_group.push_back(group_out);
              };

              for(size_t i = 0; i < frontier_in.size(); ++i)
                {
                  if(last_needed[frontier_in[i]] > layer)
                    {
                      add_cell(state_in.cell_class[i], state_in.cell_group[i]);
                    }
                }
              if(last_needed[layer] > layer)
                {
                  add_cell(cls, new_group);
                }
              assert(state_out.cell_class.size() == frontier[layer + 1].size());

              // groups with no cell left on the frontier are complete

              bool open_match = false;
              for(int g = 0; g < num_raw; ++g)
                {
                  if(find(g) != g)
                    {
                      continue;
                    }

                  bool group_matches = matches(root_labels[g], root_adjacent[g]);
                  if(renumber[g] < 0)
                    {
                      state_out.count += group_matches;
                    }
                  else
                    {
                      open_match = open_match || group_matches;
                    }
                }

              size_t transition = 0;
              if((spec.count_max >= 0) && (state_out.count > spec.count_max))
                {
                  transition = 0;
                }
              else if((spec.count_max < 0) && (state_out.count >= spec.count_min))
                {
                  transition = 1;
                }
              else if((spec.count_max < 0) &&
                      (spec.labels_forbidden == 0) &&
                      (spec.adjacent_forbidden == 0) &&
                      open_match &&
                      (state_out.count + 1 >= spec.count_min))
                {
                  // open groups only gain labels and neighbours, so a match
                  // stays one, and merging leaves at least one to close.
                  transition = 1;
                }
              else if(layer + 1 == ndim)
                {
                  assert(state_out.group_labels.size() == 0);
                  transition = (state_out.count >= spec.count_min) ? 1 : 0;
                }
              else
                {
                  std::vector<uint32_t> encoded = state_out.encode();
                  auto [it, inserted] = next_lookup.try_emplace(encoded, layer_states[layer + 1].size());
                  if(inserted)
                    {
                      layer_states[layer + 1].push_back(encoded);
                    }
                  transition = 2 + it->second;
                }

              layer_transitions[layer].push_back(transition);
            }
        }

      if(layer + 1 < ndim)
        {
          layer_states[layer + 1].shrink_to_fit();
        }
    }

  // backward pass

  std::vector<dfa_state_t> next_ids;
  for(int layer = ndim - 1; layer >= 0; --layer)
    {
      int layer_shape = get_layer_shape(layer);
      size_t num_states = layer_states[layer].size();

      std::vector<dfa_state_t> current_ids(num_states);
      for(size_t state_index = 0; state_index < num_states; ++state_index)
        {
          DFATransitionsStaging next_states(layer_shape);
          for(int c = 0; c < layer_shape; ++c)
            {
              size_t transition = layer_transitions[layer][state_index * size_t(layer_shape) + size_t(c)];
              next_states[c] = (transition < 2) ? dfa_state_t(transition) : next_ids.at(transition - 2);
            }
          current_ids[state_index] = add_state(layer, next_states);
        }

      next_ids = std::move(current_ids);
    }

  assert(next_ids.size() == 1);
  set_initial_state(next_ids[0]);
}
