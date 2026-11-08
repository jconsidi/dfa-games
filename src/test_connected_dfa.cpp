// test_connected_dfa.cpp

// Checks ConnectedDFA against brute force: every string of each spec's shape
// is classified directly by a flood fill over the spec's graph, and the DFA
// must agree on every one.

#include <cstdint>
#include <format>
#include <functional>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

#include "ConnectedDFA.h"

static uint64_t strings_checked = 0;

// Calls func on every string of the shape, as a character vector.
static void for_each_string(const dfa_shape_t& shape, std::function<void(const std::vector<int>&)> func)
{
  int ndim = int(shape.size());
  std::vector<int> characters(ndim, 0);
  while(true)
    {
      func(characters);

      int layer = ndim - 1;
      while((layer >= 0) && (++characters[layer] == shape[layer]))
        {
          characters[layer] = 0;
          --layer;
        }
      if(layer < 0)
        {
          return;
        }
    }
}

static bool brute_force(const dfa_shape_t& shape, const ConnectedSpec& spec, const std::vector<int>& characters)
{
  int ndim = int(shape.size());

  std::vector<std::vector<int>> neighbours(ndim);
  for(auto [a, b] : spec.edges)
    {
      neighbours[a].push_back(b);
      neighbours[b].push_back(a);
    }

  std::vector<int> cell_class(ndim);
  for(int layer = 0; layer < ndim; ++layer)
    {
      cell_class[layer] = spec.class_of[layer][characters[layer]];
    }

  int count = 0;
  std::vector<bool> seen(ndim, false);
  for(int start = 0; start < ndim; ++start)
    {
      int k = cell_class[start];
      if(seen[start] || (k < 0) || !(spec.group_classes & (connected_mask_t(1) << k)))
        {
          continue;
        }

      connected_mask_t group_labels = 0;
      connected_mask_t group_adjacent = 0;
      std::vector<int> stack = {start};
      seen[start] = true;
      while(stack.size() > 0)
        {
          int cell = stack.back();
          stack.pop_back();
          group_labels |= spec.labels[cell];

          for(int other : neighbours[cell])
            {
              int other_class = cell_class[other];
              if(other_class == k)
                {
                  if(!seen[other])
                    {
                      seen[other] = true;
                      stack.push_back(other);
                    }
                }
              else if(other_class >= 0)
                {
                  group_adjacent |= connected_mask_t(1) << other_class;
                }
            }
        }

      if(((group_labels & spec.labels_required) == spec.labels_required) &&
         ((group_labels & spec.labels_forbidden) == 0) &&
         ((group_adjacent & spec.adjacent_required) == spec.adjacent_required) &&
         ((group_adjacent & spec.adjacent_forbidden) == 0))
        {
          ++count;
        }
    }

  return (count >= spec.count_min) && ((spec.count_max < 0) || (count <= spec.count_max));
}

// Returns whether the spec accepted some strings and rejected others.
static bool check_spec(const std::string& test_name, const dfa_shape_t& shape, const ConnectedSpec& spec, std::function<bool(const std::vector<int>&)> expected_func)
{
  ConnectedDFA dfa(shape, spec);

  uint64_t accepted = 0;
  uint64_t checked = 0;
  for_each_string(shape, [&](const std::vector<int>& characters)
  {
    bool expected = expected_func(characters);
    bool actual = dfa.contains(DFAString(shape, characters));
    if(actual != expected)
      {
        std::string position;
        for(int c : characters)
          {
            position += std::to_string(c);
          }
        throw std::logic_error(std::format("{:s}: string {:s} expected {:d} but DFA gave {:d}", test_name, position, int(expected), int(actual)));
      }
    accepted += expected;
    ++checked;
  });

  strings_checked += checked;
  std::cout << test_name << ": " << accepted << " / " << checked << " strings accepted, all agree" << std::endl;

  return (0 < accepted) && (accepted < checked);
}

static bool check_spec(const std::string& test_name, const dfa_shape_t& shape, const ConnectedSpec& spec)
{
  return check_spec(test_name, shape, spec, [&](const std::vector<int>& characters)
  {
    return brute_force(shape, spec, characters);
  });
}

// rectangular grid, row-major, with the given neighbour offsets.
static std::vector<std::pair<int, int>> grid_edges(int width, int height, const std::vector<std::pair<int, int>>& offsets)
{
  std::vector<std::pair<int, int>> edges;
  for(int row = 0; row < height; ++row)
    {
      for(int col = 0; col < width; ++col)
        {
          for(auto [dr, dc] : offsets)
            {
              int row2 = row + dr;
              int col2 = col + dc;
              if((0 <= row2) && (row2 < height) && (0 <= col2) && (col2 < width))
                {
                  int a = row * width + col;
                  int b = row2 * width + col2;
                  if(a < b)
                    {
                      edges.emplace_back(a, b);
                    }
                }
            }
        }
    }
  return edges;
}

static const std::vector<std::pair<int, int>> hex_offsets = {{-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}};
static const std::vector<std::pair<int, int>> orthogonal_offsets = {{-1, 0}, {0, -1}, {0, 1}, {1, 0}};
static const std::vector<std::pair<int, int>> king_offsets = {{-1, -1}, {-1, 0}, {-1, 1}, {0, -1}, {0, 1}, {1, -1}, {1, 0}, {1, 1}};

// Hex: side 0 (character 1) joins row 0 to row n - 1, side 1 (character 2)
// joins column 0 to column n - 1.
static void test_hex(int n)
{
  dfa_shape_t shape(n * n, 3);

  for(int side = 0; side < 2; ++side)
    {
      ConnectedSpec spec;
      spec.edges = grid_edges(n, n, hex_offsets);
      spec.class_of.assign(n * n, {-1, -1, -1});
      spec.labels.assign(n * n, 0);
      for(int row = 0; row < n; ++row)
        {
          for(int col = 0; col < n; ++col)
            {
              int layer = row * n + col;
              spec.class_of[layer][side + 1] = 0;
              int along = (side == 0) ? row : col;
              spec.labels[layer] |= (along == 0) ? 1 : 0;
              spec.labels[layer] |= (along == n - 1) ? 2 : 0;
            }
        }
      spec.group_classes = 1;
      spec.labels_required = 3;

      // independent of the spec: flood fill from the starting edge.
      auto connected = [&](const std::vector<int>& characters)
      {
        std::vector<bool> seen(n * n, false);
        std::vector<std::pair<int, int>> stack;
        for(int i = 0; i < n; ++i)
          {
            int row = (side == 0) ? 0 : i;
            int col = (side == 0) ? i : 0;
            if(characters[row * n + col] == side + 1)
              {
                seen[row * n + col] = true;
                stack.emplace_back(row, col);
              }
          }
        while(stack.size() > 0)
          {
            auto [row, col] = stack.back();
            stack.pop_back();
            if(((side == 0) ? row : col) == n - 1)
              {
                return true;
              }
            for(auto [dr, dc] : hex_offsets)
              {
                int row2 = row + dr;
                int col2 = col + dc;
                if((0 <= row2) && (row2 < n) && (0 <= col2) && (col2 < n) &&
                   (characters[row2 * n + col2] == side + 1) && !seen[row2 * n + col2])
                  {
                    seen[row2 * n + col2] = true;
                    stack.emplace_back(row2, col2);
                  }
              }
          }
        return false;
      };

      check_spec(std::format("hex_{:d} side {:d} connected", n, side), shape, spec, connected);
    }
}

// Go: every group of character 1 has a liberty (an adjacent 0).
static void test_go(int width, int height)
{
  dfa_shape_t shape(width * height, 3);

  ConnectedSpec spec;
  spec.edges = grid_edges(width, height, orthogonal_offsets);
  spec.class_of.assign(width * height, {1, 0, -1});
  spec.labels.assign(width * height, 0);
  spec.group_classes = 1;
  spec.adjacent_forbidden = 2;
  spec.count_min = 0;
  spec.count_max = 0;

  check_spec(std::format("go {:d}x{:d} every black group lives", width, height), shape, spec);
}

// Lines of Action: all of character 1's pieces form one 8-connected group.
static void test_loa(int width, int height)
{
  dfa_shape_t shape(width * height, 3);

  ConnectedSpec spec;
  spec.edges = grid_edges(width, height, king_offsets);
  spec.class_of.assign(width * height, {-1, 0, -1});
  spec.labels.assign(width * height, 0);
  spec.group_classes = 1;
  spec.count_min = 1;
  spec.count_max = 1;

  check_spec(std::format("loa {:d}x{:d} one group", width, height), shape, spec);
}

// Checks random specs until num_mixed of them accepted some strings and
// rejected others, since a spec accepting all or none says little.
static void test_random(int num_mixed)
{
  std::mt19937 rng(20261108);
  auto uniform = [&](int low, int high)
  {
    return std::uniform_int_distribution<int>(low, high)(rng);
  };

  // count ranges cover unbounded, exact, zero and upper bounded.
  const std::vector<std::pair<int, int>> count_ranges = {{1, -1}, {2, -1}, {0, 0}, {1, 1}, {2, 2}, {0, 1}, {1, 2}};

  int num_built = 0;
  int num_built_mixed = 0;
  while(num_built_mixed < num_mixed)
    {
      int ndim = uniform(1, 9);
      dfa_shape_t shape(ndim);
      for(int layer = 0; layer < ndim; ++layer)
        {
          shape[layer] = uniform(2, 3);
        }

      ConnectedSpec spec;
      int edge_percent = uniform(10, 70);
      for(int a = 0; a < ndim; ++a)
        {
          for(int b = a + 1; b < ndim; ++b)
            {
              if(uniform(1, 100) <= edge_percent)
                {
                  // either orientation, to exercise normalization
                  spec.edges.push_back(uniform(0, 1) ? std::pair<int, int>(a, b) : std::pair<int, int>(b, a));
                }
            }
        }

      int num_classes = uniform(1, 3);
      connected_mask_t classes_seen = 0;
      connected_mask_t labels_seen = 0;
      spec.class_of.resize(ndim);
      spec.labels.resize(ndim);
      for(int layer = 0; layer < ndim; ++layer)
        {
          for(int c = 0; c < shape[layer]; ++c)
            {
              int k = uniform(-1, num_classes - 1);
              spec.class_of[layer].push_back(k);
              if(k >= 0)
                {
                  classes_seen |= connected_mask_t(1) << k;
                }
            }
          spec.labels[layer] = connected_mask_t(uniform(0, 3));
          labels_seen |= spec.labels[layer];
        }

      // each condition bit is drawn only from what occurs, and kept out of
      // the opposite mask.
      auto random_subset = [&](connected_mask_t from)
      {
        connected_mask_t output = 0;
        for(int bit = 0; bit < 32; ++bit)
          {
            if((from & (connected_mask_t(1) << bit)) && uniform(0, 2) == 0)
              {
                output |= connected_mask_t(1) << bit;
              }
          }
        return output;
      };

      spec.group_classes = random_subset(classes_seen);
      if(spec.group_classes == 0)
        {
          continue;
        }
      spec.labels_required = random_subset(labels_seen);
      spec.labels_forbidden = random_subset(labels_seen & ~spec.labels_required);
      spec.adjacent_required = random_subset(classes_seen);
      spec.adjacent_forbidden = random_subset(classes_seen & ~spec.adjacent_required);

      auto [count_min, count_max] = count_ranges[uniform(0, int(count_ranges.size()) - 1)];
      spec.count_min = count_min;
      spec.count_max = count_max;

      num_built_mixed += check_spec(std::format("random spec {:d} ({:d} layers, {:d} edges)", num_built, ndim, spec.edges.size()), shape, spec);
      ++num_built;
    }

  std::cout << "random specs: " << num_built << " checked, " << num_built_mixed << " accepting some strings and not others" << std::endl;
}

static void expect_invalid(const std::string& test_name, const dfa_shape_t& shape, const ConnectedSpec& spec)
{
  try
    {
      ConnectedDFA dfa(shape, spec);
    }
  catch(const std::invalid_argument& e)
    {
      std::cout << test_name << ": rejected as expected (" << e.what() << ")" << std::endl;
      return;
    }

  throw std::logic_error(test_name + ": invalid spec was accepted");
}

static void test_invalid()
{
  dfa_shape_t shape(3, 2);

  ConnectedSpec base;
  base.edges = {{0, 1}, {1, 2}};
  base.class_of.assign(3, {-1, 0});
  base.labels = {1, 0, 2};
  base.group_classes = 1;

  {
    ConnectedSpec spec = base;
    spec.edges.push_back({2, 1});
    expect_invalid("duplicate edge", shape, spec);
  }
  {
    ConnectedSpec spec = base;
    spec.edges.push_back({1, 1});
    expect_invalid("self loop", shape, spec);
  }
  {
    ConnectedSpec spec = base;
    spec.edges.push_back({0, 3});
    expect_invalid("edge out of range", shape, spec);
  }
  {
    ConnectedSpec spec = base;
    spec.class_of[1] = {-1, 0, 0};
    expect_invalid("class_of layer sized wrong", shape, spec);
  }
  {
    ConnectedSpec spec = base;
    spec.labels.pop_back();
    expect_invalid("labels sized wrong", shape, spec);
  }
  {
    ConnectedSpec spec = base;
    spec.class_of[0][1] = 32;
    expect_invalid("class out of range", shape, spec);
  }
  {
    ConnectedSpec spec = base;
    spec.group_classes = 0;
    expect_invalid("no group classes", shape, spec);
  }
  {
    ConnectedSpec spec = base;
    spec.adjacent_forbidden = 2;
    expect_invalid("condition names a class that never occurs", shape, spec);
  }
  {
    ConnectedSpec spec = base;
    spec.labels_required = 4;
    expect_invalid("condition names a label that never occurs", shape, spec);
  }
  {
    ConnectedSpec spec = base;
    spec.labels_required = 1;
    spec.labels_forbidden = 1;
    expect_invalid("label required and forbidden", shape, spec);
  }
  {
    ConnectedSpec spec = base;
    spec.count_min = 2;
    spec.count_max = 1;
    expect_invalid("count_max below count_min", shape, spec);
  }
  {
    ConnectedSpec spec = base;
    spec.count_min = 0;
    spec.count_max = -1;
    expect_invalid("accepts everything", shape, spec);
  }
}

int main()
{
  test_invalid();

  test_random(300);

  test_hex(2);
  test_hex(3);
  test_hex(4);

  test_go(3, 3);
  test_go(4, 3);

  test_loa(3, 3);
  test_loa(4, 3);

  std::cout << "checked " << strings_checked << " strings, all agree" << std::endl;

  return 0;
}
