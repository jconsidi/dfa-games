# Makefile

CXX=g++

CXXFLAGS=-Wall -Werror -Wno-c++11-extensions --pedantic -std=c++20

TARGETS=divide generate_moves random random_uci test_count_dfa test_union_dfa test_perft

all : $(TARGETS)

clean :
	$(RM) $(TARGETS) *.a *.o

depend :
	makedepend -Y -f - -- $(CXXFLAGS) -- *.cpp > .depend 2> /dev/null

test : all
	./test_count_dfa
	./test_union_dfa
	./test_perft

time : all
	time ./test_perft

divide : divide.o chess.a
	$(CXX) -o $@ $^

generate_moves : generate_moves.o chess.a
	$(CXX) -o $@ $^

print: print.o chess.a
	$(CXX) -o $@ $^

random : random.o chess.a
	$(CXX) -o $@ $^

random_uci : random_uci.o chess.a
	$(CXX) -o $@ $^

test_count_dfa : test_count_dfa.o chess.a
	$(CXX) -o $@ $^

test_union_dfa : test_union_dfa.o chess.a
	$(CXX) -o $@ $^

test_perft : test_perft.o chess.a
	$(CXX) -o $@ $^

chess.a : BinaryDFA.o Board.o CountDFA.o DFA.o MoveSet.o UnionDFA.o utils.o
	$(AR) rcs $@ $^

############################################################
# include dependency file if it exists

ifneq ($(wildcard .depend),)
include .depend
endif
