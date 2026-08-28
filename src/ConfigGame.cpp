// ConfigGame.cpp

#include "ConfigGame.h"

#include <sys/stat.h>

#include <format>
#include <fstream>
#include <stdexcept>

#include "DFA.h"
#include "DFAUtil.h"

ConfigBase::ConfigBase(std::string name_in)
  : game_name(name_in),
    game_config(read_config(name_in, "game.json")),
    components_config(read_config(name_in, "components.json"))
{
  if(game_config.at("game") != game_name)
    {
      throw std::runtime_error("game config is for " + std::string(game_config.at("game")) + " instead of " + game_name);
    }

  std::string components_directory = "scratch/" + name_in + "/components";
  int mkdir_ret = mkdir(components_directory.c_str(), 0700);
  if(mkdir_ret && (errno != EEXIST))
    {
      perror("components mkdir");
      throw std::runtime_error("components mkdir failed");
    }
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

bool ConfigBase::check_game_config(std::string key_in) const
{
  return game_config.contains(key_in);
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

dfa_shape_t ConfigBase::get_shape_config() const
{
  return get_game_config("shape").get<dfa_shape_t>();
}

ConfigGame::ConfigGame(std::string name_in)
  : ConfigBase(name_in),
    Game(name_in, ConfigBase::get_shape_config())
{
}

MoveGraph ConfigGame::build_move_graph(int side_to_move) const
{
  std::string config_file = std::format("move_graph_{:d}.json", side_to_move);
  auto config = read_config(get_name(), config_file);

  MoveGraph move_graph(get_shape());

  for(auto node_config : config.at("nodes"))
    {
      std::string node_name = node_config.at("node").get<std::string>();

      change_vector changes(get_shape().size());
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
          edge_conditions.push_back(get_component(config_condition));
        }

      move_graph.add_edge(edge_name, from_node, to_node, edge_conditions);
    }

  // done

  return move_graph;
}

shared_dfa_ptr ConfigGame::build_positions_lost(int side_to_move) const
{
  const nlohmann::json components = get_component_config("components");

  std::string key = "lost,side_to_move=" + std::to_string(side_to_move);
  if(!components.contains(key))
    {
      throw std::runtime_error("no lost config for " + key);
      return DFAUtil::get_reject(get_shape());
    }

  return get_component(key);
}

shared_dfa_ptr ConfigGame::get_component(std::string key_in) const
{
  const nlohmann::json components = get_component_config("components");

  if(!components.contains(key_in))
    {
      throw std::runtime_error("component " + key_in + " is not configured.");
    }

  const nlohmann::json component_config = components.at(key_in);
  std::string dfa_name = "components/" + key_in;
  return load_or_build(dfa_name, [&]()
  {
    std::string component_type = component_config.at("type").get<std::string>();
    const nlohmann::json component_inputs = component_config.at("inputs");

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

    if(component_type == "inverse")
      {
        return DFAUtil::get_inverse(get_component(component_inputs.get<std::string>()));
      }

    if(component_type == "union")
      {
        std::vector<shared_dfa_ptr> dfa_inputs;
        for(std::string input_name : component_inputs.get<std::vector<std::string>>())
          {
            dfa_inputs.push_back(get_component(input_name));
          }

        return DFAUtil::get_union_vector(get_shape(), dfa_inputs);
      }

    throw std::runtime_error("unrecognized component type " + component_type);
  });
}

DFAString ConfigGame::get_position_initial() const
{
  std::vector<int> characters = get_game_config("initial_position").get<std::vector<int>>();

  return DFAString(get_shape(), characters);
}
