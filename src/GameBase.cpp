// GameBase.cpp

#include "GameBase.h"

#include "DFAUtil.h"
#include "Profile.h"

// Directory creation for whatever this game ends up saving is handled by
// DFA::save_impl/save_by_hash at the point of the actual save (see
// ensure_parent_directories in DFA.cpp), not here.
GameBase::GameBase(std::string name_in, const dfa_shape_t& shape_in, int sides_in)
  : name(name_in),
    shape(shape_in),
    sides(sides_in),
    move_graphs_forward(sides_in, 0),
    move_graphs_backward(sides_in, 0)
{
}

GameBase::~GameBase()
{
}

const MoveGraph& GameBase::get_move_graph_forward(int side_to_move) const
{
  assert(0 <= side_to_move);
  assert(side_to_move < sides);
  
  if(move_graphs_forward[side_to_move] == 0)
    {
      move_graphs_forward[side_to_move] = new MoveGraph(build_move_graph(side_to_move).optimize());
    }

  return *move_graphs_forward.at(side_to_move);
}

const MoveGraph& GameBase::get_move_graph_backward(int side_to_move) const
{
  assert(0 <= side_to_move);
  assert(side_to_move < sides);
  
  if(move_graphs_backward[side_to_move] == 0)
    {
      const MoveGraph& forward = get_move_graph_forward(side_to_move);
      move_graphs_backward[side_to_move] = new MoveGraph(forward.reverse().optimize());
    }

  return *move_graphs_backward.at(side_to_move);
}

shared_dfa_ptr GameBase::get_moves_backward(int side_to_move, shared_dfa_ptr positions_in) const
{
  Profile profile("get_moves_backward");

  assert(0 <= side_to_move);
  assert(side_to_move < 2);
  assert(positions_in);

  std::string name_prefix = std::format("{:s},backward,side_to_move={:d}", name, side_to_move);
  return get_move_graph_backward(side_to_move).get_moves(name_prefix, positions_in);
}

std::vector<DFAString> GameBase::get_moves_forward(int side_to_move, const DFAString& position_in) const
{
  assert(0 <= side_to_move);
  assert(side_to_move < 2);

  return get_move_graph_forward(side_to_move).get_moves(position_in);
}

shared_dfa_ptr GameBase::get_moves_forward(int side_to_move, shared_dfa_ptr positions_in) const
{
  Profile profile("get_moves_forward");

  assert(0 <= side_to_move);
  assert(side_to_move < 2);
  assert(positions_in);

  std::string name_prefix = std::format("{:s},forward,side_to_move={:d}", name, side_to_move);
  return get_move_graph_forward(side_to_move).get_moves(name_prefix, positions_in);
}

shared_dfa_ptr GameBase::load(std::string dfa_name_in) const
{
  std::string dfa_name = name + "/" + dfa_name_in;
  return DFAUtil::load_by_name(shape, dfa_name);
}

shared_dfa_ptr GameBase::load_by_hash(std::string hash_in) const
{
  return DFAUtil::load_by_hash(get_shape(), hash_in);
}

shared_dfa_ptr GameBase::load_by_name(std::string dfa_name_in) const
{
  // load by name, but return NULL when there's an issue
  try
    {
      std::string dfa_name = name + "/" + dfa_name_in;
      return DFAUtil::load_by_name(shape, dfa_name);
    }
  catch(const std::runtime_error& e)
    {
      return shared_dfa_ptr(0);
    }
}

shared_dfa_ptr GameBase::load_or_build(std::string dfa_name_in, std::function<shared_dfa_ptr ()> build_func) const
{
  Profile profile("load_or_build " + dfa_name_in);

  std::string dfa_name = name + "/" + dfa_name_in;
  return DFAUtil::load_or_build(shape, dfa_name, build_func);
}

std::vector<DFAString> GameBase::validate_moves(int, const DFAString&) const
{
  throw std::logic_error(get_name() + " did not implement validate_moves()");
}

std::optional<int> GameBase::validate_outcome(int, const DFAString&) const
{
  throw std::logic_error(get_name() + "did not implement validate_outcome()");
}
