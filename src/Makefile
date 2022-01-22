# Makefile

CXX=c++

CXXFLAGS=-Wall -Werror -Wno-c++11-extensions --pedantic -std=c++20 -g

TARGETS=divide generate_moves random random_uci test_chess_game test_dfa test_perft test_tictactoe_game

all : $(TARGETS)

clean :
	$(RM) $(TARGETS) *.a *.o

depend :
	makedepend -Y -f - -- $(CXXFLAGS) -- *.cpp > .depend 2> /dev/null

test : all
	./test_dfa
	./test_perft

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

test_chess_game : test_chess_game.o chess.a
	$(CXX) -o $@ $^

test_dfa : test_dfa.o chess.a
	$(CXX) -o $@ $^

test_perft : test_perft.o chess.a
	$(CXX) -o $@ $^

test_tictactoe_game : test_tictactoe_game.o TicTacToeGame.o chess.a
	$(CXX) -o $@ $^

chess.a : AcceptDFA.o BetweenMasks.o BinaryDFA.o Board.o ChangeDFA.o ChessGame.o CountCharacterDFA.o CountDFA.o DFA.o DifferenceDFA.o FixedDFA.o Game.o IntersectionDFA.o IntersectionManager.o InverseDFA.o MoveSet.o RejectDFA.o RewriteDFA.o UnionDFA.o utils.o
	$(AR) rcs $@ $^

############################################################
# include dependency file if it exists

ifneq ($(wildcard .depend),)
include .depend
endif
