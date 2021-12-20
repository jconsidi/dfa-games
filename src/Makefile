# Makefile

CXX=g++

CXXFLAGS=-Wall -Werror -Wno-c++11-extensions --pedantic -std=c++20

TARGETS=divide generate_moves random test_perft

all : $(TARGETS)

clean :
	$(RM) $(TARGETS) *.o

test : all
	./test_perft

time : all
	time ./test_perft

divide : divide.o Board.o utils.o
	$(CXX) -o $@ $^

generate_moves : generate_moves.o Board.o
	$(CXX) -o $@ $^

print: print.o Board.o utils.o
	$(CXX) -o $@ $^

random : random.o Board.o utils.o
	$(CXX) -o $@ $^

test_perft : test_perft.o Board.o utils.o
	$(CXX) -o $@ $^
