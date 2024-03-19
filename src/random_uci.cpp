// random_uci.cpp

#include <chrono>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "utils.h"

// initial proof of concept UCI implementation

Board apply_moves(const Board& board, std::string uci_string);
std::string pick_move(const Board& board);

int main()
{
  Board board;
  std::string line;
  while(std::getline(std::cin, line))
    {
      if(line == "uci")
	{
	  std::cout << "id name random" << std::endl;
	  std::cout << "id author JC" << std::endl;
	  std::cout << "uciok" << std::endl;
	  std::cout.flush();
	  continue;
	}

      if(line == "ucinewgame")
	{
	  // ignore for now
	  continue;
	}

      if(line == "isready")
	{
	  // ignore for now
	  std::cout << "readyok" << std::endl;
	  std::cout.flush();
	  continue;
	}

      if(line == "position startpos")
	{
	  board = Board(INITIAL_FEN);
	  continue;
	}

      if(line.substr(0, 24) == "position startpos moves ")
	{
	  board = Board(INITIAL_FEN);
	  board = apply_moves(board, line.substr(24, line.size()));
	  continue;
	}

      if((line == "go") || (line.substr(0, 3) == "go "))
	{
	  // would start thinking thread for a real player here.
	  std::cout << "bestmove " << pick_move(board) << std::endl;
	  std::cout.flush();
	  continue;
	}

      if(line == "stop")
	{
	  // would signal the thinking thread and wait for it to stop.
	  std::cout << "bestmove " << pick_move(board) << std::endl;
	  std::cout.flush();
	  continue;
	}

      if(line == "quit")
	{
	  return 0;
	}

      std::cerr << "UNRECOGNIZED INPUT: --" << line << "--" << std::endl;
      std::cerr.flush();
    }

  return 0;
}

Board apply_moves(const Board& board, std::string uci_string)
{
  Board new_board = board;

  std::vector<std::string> uci_vector;
  int offset1 = 0;
  for(int offset2 = offset1 + 1; offset2 <= uci_string.size(); ++offset2)
    {
      if((uci_string[offset2] == ' ') || (offset2 == uci_string.size()))
	{
	  if(offset1 < offset2)
	    {
	      uci_vector.push_back(uci_string.substr(offset1, offset2 - offset1));
	    }

	  offset1 = offset2 + 1;
	}
    }

  for(int i = 0; i < uci_vector.size(); ++i)
    {
      Board moves[CHESS_MAX_MOVES];
      int num_moves = new_board.generate_moves(moves);

      bool found_move = false;
      for(int j = 0; j < num_moves; ++j)
	{
	  if(uci_move(new_board, moves[j]) == uci_vector[i])
	    {
	      new_board = moves[j];
	      found_move = true;
	      break;
	    }
	}

      if(!found_move)
	{
	  throw std::logic_error("could not find move " + uci_vector[i]);
	}
    }

  return new_board;
}

std::string pick_move(const Board& board)
{
  static unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
  static std::default_random_engine generator(seed);

  Board moves[CHESS_MAX_MOVES];
  int num_moves = board.generate_moves(moves);
  if(num_moves == 0)
    {
      return "NOMOVES";
    }

  std::uniform_int_distribution<int> distribution(0, num_moves - 1);
  const Board& next_board = moves[distribution(generator)];

  return uci_move(board, next_board);
}
