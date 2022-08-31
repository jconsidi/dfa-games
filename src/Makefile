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

# see PerftTestCases.cpp for origins of each position
test_perft_u_suite : ./test_perft_u
	./test_perft_u "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1" 5
	./test_perft_u "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq -" 4
	./test_perft_u "8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - -" 6
	./test_perft_u "r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1" 5
	./test_perft_u "r2q1rk1/pP1p2pp/Q4n2/bbp1p3/Np6/1B3NBn/pPPP1PPP/R3K2R b KQ - 0 1" 5
	./test_perft_u "rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8" 4
	./test_perft_u "r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10" 4
	./test_perft_u "n1n5/PPPk4/8/8/8/8/4Kppp/5N1N b - - 0 1" 5

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

chess.a : AcceptDFA.o BetweenMasks.o BinaryDFA.o Board.o ChangeDFA.o ChessBoardDFA.o ChessGame.o CompactBitSet.o CountCharacterDFA.o CountDFA.o DFA.o DedupedDFA.o DifferenceDFA.o ExplicitDFA.o FixedDFA.o Flashsort.o Game.o IntersectionDFA.o IntersectionManager.o InverseDFA.o MemoryMap.o MoveSet.o Profile.o RejectDFA.o UnionDFA.o VectorBitSet.o utils.o
	$(AR) rcs $@ $^

############################################################
# include dependency file if it exists

ifneq ($(wildcard .depend),)
include .depend
endif
