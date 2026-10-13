// test_move_graph.cpp

#include <algorithm>
#include <format>
#include <iostream>
#include <set>

#include <nlohmann/json.hpp>

#include "DFAUtil.h"
#include "test_utils.h"

void test_position(const GameBase& game, const DFAString& position, const nlohmann::json& test_case)
{
  int side_to_move = test_case.at("side_to_move").get<int>();
  std::cout << "SIDE TO MOVE: " << side_to_move << std::endl;

  std::vector<DFAString> actual_moves = game.get_moves_forward(side_to_move, position);

  std::vector<DFAString> validate_moves = game.validate_moves(side_to_move, position);

  std::set<DFAString> actual_moves_set(actual_moves.begin(), actual_moves.end());
  std::set<DFAString> validate_moves_set(validate_moves.begin(), validate_moves.end());

  test_moves(game, actual_moves_set, validate_moves_set);
}

template <class E, class F>
void require_throws(std::string test_name, F f)
{
  try
    {
      f();
    }
  catch(const E& e)
    {
      std::cout << test_name << ": threw as expected: " << e.what() << std::endl;
      return;
    }

  throw std::logic_error(test_name + ": did not throw");
}

// begin -> 12 moves (layer 0 from a to b, a != b) -> end, over all 16
// positions of shape {4, 4}. split_joins must keep every move node and
// produce the same output positions, with the expected number of joins.
void test_split_joins()
{
  dfa_shape_t shape = {4, 4};

  MoveGraph graph(shape);
  graph.add_node("begin");
  std::vector<std::string> move_names;
  for(int a = 0; a < 4; ++a)
    {
      for(int b = 0; b < 4; ++b)
        {
          if(a != b)
            {
              move_names.push_back(std::format("move_{:d}_{:d}", a, b));
              graph.add_node(move_names.back(), 0, a, b);
            }
        }
    }
  graph.add_node("end");
  for(const std::string& move_name : move_names)
    {
      graph.add_edge("begin", move_name);
      graph.add_edge(move_name, "end");
    }

  std::vector<DFAString> positions;
  for(int x = 0; x < 4; ++x)
    {
      for(int y = 0; y < 4; ++y)
        {
          positions.emplace_back(shape, std::vector<int>({x, y}));
        }
    }

  std::vector<DFAString> expected = graph.get_moves(positions);
  std::sort(expected.begin(), expected.end());
  if(expected.size() != 48)
    {
      throw std::logic_error(std::format("test_split_joins: unsplit graph gave {:d} moves, expected 48", expected.size()));
    }

  // 12 inputs: fan-in 2 -> 6, 3, 2 joins; 3 -> 4, 2; 4 -> 3; 11 -> 2; 12 -> none.
  std::vector<std::pair<int, size_t>> cases = {{2, 11}, {3, 6}, {4, 3}, {11, 2}, {12, 0}};
  for(const auto& [max_fan_in, joins_expected] : cases)
    {
      std::string test_name = std::format("test_split_joins max_fan_in={:d}", max_fan_in);
      MoveGraph split = graph.split_joins(max_fan_in, test_name + ": ").optimize();

      size_t size_expected = graph.size() + joins_expected;
      if(split.size() != size_expected)
        {
          throw std::logic_error(std::format("{:s}: {:d} nodes, expected {:d}", test_name, split.size(), size_expected));
        }

      for(const std::string& move_name : move_names)
        {
          // throws if the node is missing
          split.get_node_pre_conditions(move_name);
        }

      std::vector<DFAString> actual = split.get_moves(positions);
      std::sort(actual.begin(), actual.end());
      if(actual != expected)
        {
          throw std::logic_error(test_name + ": moves differ from the unsplit graph");
        }

      std::cout << test_name << ": " << split.size() << " nodes, " << actual.size() << " moves match: passed" << std::endl;
    }

  require_throws<std::invalid_argument>("test_split_joins max_fan_in=1", [&]()
  {
    graph.split_joins(1, "");
  });
}

void test_max_fan_in_env()
{
  const char *name = "DFA_MOVE_GRAPH_MAX_FAN_IN";

  unsetenv(name);
  if((move_graph_max_fan_in() != 8) || !move_graph_max_fan_in_is_default())
    {
      throw std::logic_error("test_max_fan_in_env: unset did not give the default 8");
    }

  setenv(name, "3", 1);
  if((move_graph_max_fan_in() != 3) || move_graph_max_fan_in_is_default())
    {
      throw std::logic_error("test_max_fan_in_env: 3 did not parse");
    }

  for(std::string bad : {"", "1", "0", "-4", "abc", "8x", " 8", "99999999999"})
    {
      setenv(name, bad.c_str(), 1);
      require_throws<std::runtime_error>("test_max_fan_in_env \"" + bad + "\"", []()
      {
        move_graph_max_fan_in();
      });
    }

  unsetenv(name);
  std::cout << "test_max_fan_in_env: passed" << std::endl;
}

int main(int argc, char **argv)
{
  if(argc > 2)
    {
      std::cerr << "usage: test_move_graph [GAME_NAME]\n";
      return 1;
    }

  test_split_joins();
  test_max_fan_in_env();

  run_test_positions((argc >= 2) ? std::string(argv[1]) : std::string(""),
                     test_position);

  return 0;
}
