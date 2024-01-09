# Makefile

CXX=g++

CXXFLAGS_SHARED=-Wall -Werror -Wno-c++11-extensions --pedantic -std=c++20 -g
LDFLAGS_SHARED=-L/usr/local/opt/openssl/lib -lcrypto

ifeq ($(CXX), g++)
# enable parallel execution when using g++ since available in libstdc++
CXXFLAGS=$(CXXFLAGS_SHARED) -I/usr/local/include -Wno-sign-compare
LDFLAGS=$(LDFLAGS_SHARED) -L/usr/local/lib -ltbb
else
CXXFLAGS=$(CXXFLAGS_SHARED)
LDFLAGS=$(LDFLAGS_SHARED)
endif

TARGETS=build_chess_database build_forward divide generate_moves print random random_uci solve_forward_backward states stats test_backward test_bitset test_breakthrough test_change_dfa test_chess_game test_dfa test_get_intersection test_get_union test_get_union_vector test_normal_nim_game test_perft test_perft_u test_reachable test_solved test_sort_unique test_tictactoe_game validate_dfa validate_forward

all : $(TARGETS)

clean :
	$(RM) $(TARGETS) *.a *.o

depend :
	makedepend -Y -f - -- $(CXXFLAGS) -- *.cpp > .depend 2> /dev/null

test : all
	./test_perft
	./test_bitset
	./test_sort_unique
	./test_dfa
	./test_change_dfa
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

build_chess_database : build_chess_database.o test_utils.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

build_forward : build_forward.o test_utils.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

divide : divide.o dfagames.a
	$(CXX) -o $@ $^

generate_moves : generate_moves.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

print: print.o test_utils.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

prototype: prototype.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

random : random.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

random_uci : random_uci.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

solve_forward_backward : solve_forward_backward.o test_utils.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

states : states.o test_utils.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

stats : stats.o test_utils.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

test_backward : test_backward.o test_utils.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

test_breakthrough : test_breakthrough.o test_utils.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

test_bitset : test_bitset.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

test_change_dfa : test_change_dfa.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

test_chess_game : test_chess_game.o test_utils.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

test_chess_backward : test_chess_backward.o test_utils.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

test_dfa : test_dfa.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

test_get_intersection : test_get_intersection.o test_utils.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

test_get_union : test_get_union.o test_utils.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

test_get_union_vector : test_get_union_vector.o test_utils.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

test_memorymap : test_memorymap.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

test_normal_nim_game : test_normal_nim_game.o test_utils.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

test_perft : test_perft.o dfagames.a PerftTestCases.o
	$(CXX) -o $@ $^ $(LDFLAGS)

test_perft_u : test_perft_u.o dfagames.a PerftTestCases.o
	$(CXX) -o $@ $^ $(LDFLAGS)

test_reachable : test_reachable.o test_utils.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

test_solved : test_solved.o test_utils.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

test_sort_unique : test_sort_unique.o test_utils.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

test_tictactoe_game : test_tictactoe_game.o TicTacToeGame.o test_utils.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

validate_dfa : validate_dfa.o test_utils.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

validate_forward : validate_forward.o test_utils.o dfagames.a
	$(CXX) -o $@ $^ $(LDFLAGS)

dfagames.a : AcceptDFA.o AmazonsGame.o BetweenMasks.o BinaryDFA.o BinaryFunction.o Board.o BreakthroughGame.o ChangeDFA.o ChessGame.o CompactBitSet.o CountCharacterDFA.o CountDFA.o CountManager.o DFA.o DFAUtil.o DNFBuilder.o DedupedDFA.o DifferenceDFA.o FixedDFA.o Flashsort.o FlexBitSet.o Game.o GameUtil.o IntersectionDFA.o InverseDFA.o MemoryMap.o MoveGraph.o MoveSet.o NormalNimGame.o NormalPlayGame.o OrderedBitSet.o OthelloGame.o Profile.o RejectDFA.o StringDFA.o TicTacToeGame.o UnionDFA.o UnorderedBitSet.o VectorBitSet.o sort.o utils.o
	$(AR) rcs $@ $^

############################################################
# include dependency file if it exists

ifneq ($(wildcard .depend),)
include .depend
endif
