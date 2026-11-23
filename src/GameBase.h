// GameBase.h

#ifndef GAME_BASE_H
#define GAME_BASE_H

#include <memory>
#include <optional>
#include <variant>
#include <vector>

#include "DFA.h"
#include "MoveGraph.h"

// The game is over for the side to move, with result 1 (won), 0 (tied) or
// -1 (lost).
struct GameOver
{
  int result;
};

// A position as the rules see it: either the game is over, or the side to
// move has at least one move.
using ValidatedPosition = std::variant<GameOver, std::vector<DFAString>>;

// The moves of a validated position, none if the game is over.
inline std::vector<DFAString> validated_moves(const ValidatedPosition& validated)
{
  const std::vector<DFAString> *moves = std::get_if<std::vector<DFAString>>(&validated);
  return moves ? *moves : std::vector<DFAString>();
}

class GameBase
{
private:

  std::string name;
  dfa_shape_t shape;
  int sides;

  mutable std::vector<std::unique_ptr<MoveGraph>> move_graphs_forward;
  mutable std::vector<std::unique_ptr<MoveGraph>> move_graphs_backward;

  void build_move_graphs(int) const;

protected:

  GameBase(std::string, const dfa_shape_t&, int);

  virtual MoveGraph build_move_graph(int) const = 0;

  // The rules' view of a position, checked by validate_position. The
  // default combines validate_outcome and validate_moves.
  virtual ValidatedPosition validate_position_impl(int, const DFAString&) const;
  
  int get_shape_size() const {return int(shape.size());}

public:

  virtual ~GameBase();

  const MoveGraph& get_move_graph_forward(int) const;
  const MoveGraph& get_move_graph_backward(int) const;

  shared_dfa_ptr get_moves_backward(int, shared_dfa_ptr) const;
  std::vector<DFAString> get_moves_forward(int, const DFAString&) const;
  shared_dfa_ptr get_moves_forward(int, shared_dfa_ptr) const;

  std::string get_name() const {return name;}
  const dfa_shape_t& get_shape() const {return shape;}
  int get_sides() const {return sides;}

  // saved position access

  shared_dfa_ptr load(std::string dfa_name_in) const;
  shared_dfa_ptr load_by_hash(std::string) const;
  shared_dfa_ptr load_by_name(std::string dfa_name_in) const;
  shared_dfa_ptr load_or_build(std::string dfa_name_in, std::function<shared_dfa_ptr()> build_func) const;

  // position evaluation

  virtual std::string position_to_string(const DFAString&) const = 0;

  // validation

  // Either the game is over (GameOver) or the side to move has at least
  // one move. Throws std::logic_error if the game's rules give neither or
  // both.
  ValidatedPosition validate_position(int, const DFAString&) const;

  virtual std::vector<DFAString> validate_moves(int, const DFAString&) const;
  virtual std::optional<int> validate_outcome(int, const DFAString&) const;
};

#endif
