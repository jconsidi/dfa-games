// ConfigBase.cpp

#include "ConfigBase.h"

#include <format>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>

#include "ConnectedDFA.h"
#include "DFAUtil.h"

// Builds a ConnectedSpec from a "connected" component's inputs, where
// classes and labels are named. Names are numbered in sorted order. Any key,
// name, character or layer that does not fit throws.
static ConnectedSpec build_connected_spec(const dfa_shape_t& shape, const nlohmann::json& inputs, const std::string& key)
{
  auto fail = [&](const std::string& message)
  {
    throw std::runtime_error("component " + key + ": " + message);
  };

  static const std::set<std::string> known_keys = {
    "edges", "classes", "labels", "group_classes",
    "labels_required", "labels_forbidden",
    "adjacent_required", "adjacent_forbidden",
    "count_min", "count_max"
  };
  for(const auto& item : inputs.items())
    {
      if(!known_keys.contains(item.key()))
        {
          fail("unknown input " + item.key());
        }
    }

  int ndim = int(shape.size());
  ConnectedSpec spec;

  for(const auto& edge : inputs.at("edges"))
    {
      std::vector<int> ends = edge.get<std::vector<int>>();
      if(ends.size() != 2)
        {
          fail("an edge does not have two ends");
        }
      spec.edges.emplace_back(ends[0], ends[1]);
    }

  // classes: name -> characters, the same at every layer

  std::map<std::string, int> class_index;
  spec.class_of.resize(ndim);
  for(int layer = 0; layer < ndim; ++layer)
    {
      spec.class_of[layer].assign(shape[layer], -1);
    }
  for(const auto& item : inputs.at("classes").items())
    {
      int k = int(class_index.size());
      if(k >= 32)
        {
          fail("more than 32 classes");
        }
      class_index[item.key()] = k;

      for(int c : item.value().get<std::vector<int>>())
        {
          for(int layer = 0; layer < ndim; ++layer)
            {
              if((c < 0) || (c >= shape[layer]))
                {
                  fail(std::format("class {:s} has character {:d}, outside layer {:d}'s alphabet", item.key(), c, layer));
                }
              if(spec.class_of[layer][c] >= 0)
                {
                  fail(std::format("character {:d} is in more than one class", c));
                }
              spec.class_of[layer][c] = k;
            }
        }
    }

  // labels: name -> layers

  std::map<std::string, int> label_index;
  spec.labels.assign(ndim, 0);
  if(inputs.contains("labels"))
    {
      for(const auto& item : inputs.at("labels").items())
        {
          int bit = int(label_index.size());
          if(bit >= 32)
            {
              fail("more than 32 labels");
            }
          label_index[item.key()] = bit;

          for(int layer : item.value().get<std::vector<int>>())
            {
              if((layer < 0) || (layer >= ndim))
                {
                  fail(std::format("label {:s} has layer {:d}, outside [0, {:d})", item.key(), layer, ndim));
                }
              spec.labels[layer] |= connected_mask_t(1) << bit;
            }
        }
    }

  auto mask_of = [&](const std::string& input_key, const std::map<std::string, int>& index)
  {
    connected_mask_t mask = 0;
    if(inputs.contains(input_key))
      {
        for(const std::string& name : inputs.at(input_key).get<std::vector<std::string>>())
          {
            auto search = index.find(name);
            if(search == index.end())
              {
                fail(input_key + " names unknown " + name);
              }
            mask |= connected_mask_t(1) << search->second;
          }
      }
    return mask;
  };

  spec.group_classes = mask_of("group_classes", class_index);
  spec.labels_required = mask_of("labels_required", label_index);
  spec.labels_forbidden = mask_of("labels_forbidden", label_index);
  spec.adjacent_required = mask_of("adjacent_required", class_index);
  spec.adjacent_forbidden = mask_of("adjacent_forbidden", class_index);

  if(inputs.contains("count_min"))
    {
      spec.count_min = inputs.at("count_min").get<int>();
    }
  if(inputs.contains("count_max"))
    {
      spec.count_max = inputs.at("count_max").get<int>();
    }

  try
    {
      spec.validate(shape);
    }
  catch(const std::invalid_argument& e)
    {
      fail(e.what());
    }

  return spec;
}

// Directory creation for whatever this game ends up saving -- durable
// results under the archive root, component_<key> under the local root --
// is handled by DFA::save_impl/save_by_hash at the point of the actual
// save, not here.
ConfigBase::ConfigBase(std::string name_in)
  : game_name(name_in),
    shape(get_shape_config(name_in)),
    game_config(read_config(name_in, "game.json")),
    components_config(read_config(name_in, "components.json"))
{
  if(game_config.at("game") != game_name)
    {
      throw std::runtime_error("game config is for " + std::string(game_config.at("game")) + " instead of " + game_name);
    }
}

MoveGraph ConfigBase::build_move_graph(const GameBase& game, int side_to_move) const
{
  std::string config_file = std::format("move_graph_{:d}.json", side_to_move);
  auto config = read_config(config_file);

  MoveGraph move_graph(shape);

  for(auto node_config : config.at("nodes"))
    {
      std::string node_name = node_config.at("node").get<std::string>();

      change_vector changes(shape.size());
      for(auto change : node_config.at("changes"))
        {
          int layer = change.at("layer").get<int>();
          int before = change.at("before").get<int>();
          int after = change.at("after").get<int>();
          changes[layer] = change_type(before, after);
        }

      move_graph.add_node(node_name, changes);
    }

  for(auto edge_config : config.at("edges"))
    {
      std::string edge_name = edge_config.at("edge");
      std::string from_node = edge_config.at("from");
      std::string to_node = edge_config.at("to");

      std::vector<shared_dfa_ptr> edge_conditions;
      for(auto config_condition : edge_config.at("conditions"))
        {
          edge_conditions.push_back(get_component(game, config_condition));
        }

      move_graph.add_edge(edge_name, from_node, to_node, edge_conditions);
    }

  // done

  return move_graph;
}

void ConfigBase::check_game(const GameBase& game) const
{
  const dfa_shape_t& game_shape = game.get_shape();

  if(game_shape.size() != shape.size())
    {
      throw std::logic_error(std::format("incompatible game shape length ({:d} game vs {:d} expected)",
                                         game_shape.size(), shape.size()));
    }

  for(int layer = 0; layer < shape.size(); ++layer)
    {
      if(game_shape[layer] != shape[layer])
        {
          throw std::logic_error("incompatible game shape");
        }
    }
}

bool ConfigBase::check_game_config(std::string key_in) const
{
  return game_config.contains(key_in);
}

shared_dfa_ptr ConfigBase::get_component(const GameBase& game, std::string key_in) const
{
  check_game(game);

  const nlohmann::json components = get_component_config("components");

  if(!components.contains(key_in))
    {
      throw std::runtime_error("component " + key_in + " is not configured.");
    }

  const nlohmann::json component_config = components.at(key_in);
  std::string dfa_name = "component_" + key_in;
  return game.load_or_build(dfa_name, [&]()
  {
    std::string component_type = component_config.at("type").get<std::string>();
    const nlohmann::json component_inputs = component_config.at("inputs");

    if(component_type == "connected")
      {
        return DFAUtil::get_connected(get_shape(), build_connected_spec(get_shape(), component_inputs, key_in));
      }

    if(component_type == "count")
      {
        int c_in = component_inputs.at("c").get<int>();
        int count_min = component_inputs.at("count_min").get<int>();
        int count_max = component_inputs.at("count_max").get<int>();
        int layer_min = component_inputs.at("layer_min").get<int>();
        int layer_max = component_inputs.at("layer_max").get<int>();

        return DFAUtil::get_count_character(get_shape(),
                                            c_in,
                                            count_min, count_max,
                                            layer_min, layer_max);
      }

    if(component_type == "fixed")
      {
        std::vector<shared_dfa_ptr> dfa_inputs;
        for (auto it : component_inputs.items())
          {
            int k = std::stoi(it.key());
            int v = it.value().get<int>();

            dfa_inputs.push_back(DFAUtil::get_fixed(get_shape(), k, v));
          }

        return DFAUtil::get_intersection_vector(get_shape(), dfa_inputs);
      }

    if(component_type == "intersection")
      {
        std::vector<shared_dfa_ptr> dfa_inputs;
        for(std::string input_name : component_inputs.get<std::vector<std::string>>())
          {
            dfa_inputs.push_back(get_component(game, input_name));
          }

        // an empty intersection would silently accept every position
        if(dfa_inputs.empty())
          {
            throw std::runtime_error("intersection component " + key_in + " has no inputs");
          }

        return DFAUtil::get_intersection_vector(get_shape(), dfa_inputs);
      }

    if(component_type == "inverse")
      {
        return DFAUtil::get_inverse(get_component(game, component_inputs.get<std::string>()));
      }

    if(component_type == "union")
      {
        std::vector<shared_dfa_ptr> dfa_inputs;
        for(std::string input_name : component_inputs.get<std::vector<std::string>>())
          {
            dfa_inputs.push_back(get_component(game, input_name));
          }

        return DFAUtil::get_union_vector(get_shape(), dfa_inputs);
      }

    throw std::runtime_error("unrecognized component type " + component_type);
  });
}

const nlohmann::json& ConfigBase::get_component_config(std::string key_in) const
{
  if(!components_config.contains(key_in))
    {
      throw std::runtime_error("component config for " + game_name + " is missing key: " + key_in);
    }

  return components_config.at(key_in);
}

const nlohmann::json& ConfigBase::get_game_config(std::string key_in) const
{
  if(!game_config.contains(key_in))
    {
      throw std::runtime_error("game config for " + game_name + " is missing key: " + key_in);
    }

  return game_config.at(key_in);
}

dfa_shape_t ConfigBase::get_shape_config(std::string game_name_in)
{
  nlohmann::json config = read_config(game_name_in, "game.json");
  return config.at("shape").get<dfa_shape_t>();
}

nlohmann::json ConfigBase::read_config(std::string game_name_in, std::string config_filename_in)
{
  std::string config_path = "config/" + game_name_in + "/" + config_filename_in;

  std::ifstream config_file(config_path);
  if(!config_file)
    {
      throw std::runtime_error(config_path + " could not be opened");
    }

  return nlohmann::json::parse(config_file);
}

nlohmann::json ConfigBase::read_config(std::string config_filename_in) const
{
  return read_config(game_name, config_filename_in);
}
