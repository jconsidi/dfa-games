# Makefile

CXX=c++

CXXFLAGS=-Wall -Werror -Wno-c++11-extensions --pedantic -std=c++20 -g

TARGETS=divide generate_moves prototype random random_uci test_dfa test_perft

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

test_dfa : test_dfa.o chess.a
	$(CXX) -o $@ $^

test_perft : test_perft.o chess.a
	$(CXX) -o $@ $^

chess.a : AcceptDFA.o BetweenMasks.o BinaryDFA.o Board.o CheckDFA.o CountCharacterDFA.o CountDFA.o DFA.o FixedDFA.o ForcedDFA.o HasMovesDFA.o IntersectionDFA.o InverseDFA.o LegalDFA.o MoveSet.o PreviousDFA.o UnionDFA.o ThreatDFA.o utils.o
	$(AR) rcs $@ $^

############################################################
# include dependency file if it exists

ifneq ($(wildcard .depend),)
include .depend
endif
