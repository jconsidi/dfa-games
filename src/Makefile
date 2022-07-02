# Makefile

CXX=c++

CXXFLAGS=-Wall -Werror -Wno-c++11-extensions --pedantic -std=c++20 -g -ferror-limit=1

TARGETS=divide generate_moves random random_uci test_bitset test_chess_game test_dfa test_perft test_perft_u test_tictactoe_game

all : $(TARGETS)

clean :
	$(RM) $(TARGETS) *.a *.o

depend :
	makedepend -Y -f - -- $(CXXFLAGS) -- *.cpp > .depend 2> /dev/null

test : all
	./test_perft
	./test_bitset
	./test_dfa
	./test_tictactoe_game
	./test_chess_game

time : all
	time ./test_perft

divide : divide.o chess.a
	$(CXX) -o $@ $^

generate_moves : generate_moves.o chess.a
	$(CXX) -o $@ $^

print: print.o chess.a
	$(CXX) -o $@ $^

prototype: prototype.o chess.a
	$(CXX) -o $@ $^

random : random.o chess.a
	$(CXX) -o $@ $^

random_uci : random_uci.o chess.a
	$(CXX) -o $@ $^

test_bitset : test_bitset.o chess.a
	$(CXX) -o $@ $^

test_chess_game : test_chess_game.o chess.a test_utils.o
	$(CXX) -o $@ $^

test_dfa : test_dfa.o chess.a
	$(CXX) -o $@ $^

test_memorymap : test_memorymap.o chess.a
	$(CXX) -o $@ $^

test_perft : test_perft.o chess.a PerftTestCases.o
	$(CXX) -o $@ $^

test_perft_u : test_perft_u.o chess.a PerftTestCases.o
	$(CXX) -o $@ $^

test_tictactoe_game : test_tictactoe_game.o TicTacToeGame.o chess.a test_utils.o
	$(CXX) -o $@ $^

chess.a : AcceptDFA.o BetweenMasks.o BinaryDFA.o Board.o ChangeDFA.o ChessBoardDFA.o ChessGame.o CompactBitSet.o CountCharacterDFA.o CountDFA.o DFA.o DedupedDFA.o DifferenceDFA.o ExplicitDFA.o FixedDFA.o Flashsort.o Game.o IntersectionDFA.o IntersectionManager.o InverseDFA.o MemoryMap.o MoveSet.o Profile.o RejectDFA.o RewriteDFA.o UnionDFA.o VectorBitSet.o utils.o
	$(AR) rcs $@ $^

############################################################
# include dependency file if it exists

ifneq ($(wildcard .depend),)
include .depend
endif
