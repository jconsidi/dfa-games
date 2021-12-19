# Makefile

CXX=g++

CXXFLAGS=-Wall -Werror -Wno-c++11-extensions --pedantic -std=c++20

TARGETS=divide generate_moves test_perft

all : $(TARGETS)

clean :
	$(RM) $(TARGETS) *.o

test : all
	./test_perft

time : all
	time ./test_perft

divide: divide.o board.o utils.o
	$(CXX) -o $@ $^

generate_moves : generate_moves.o board.o
	$(CXX) -o $@ $^

print: print.o board.o utils.o
	$(CXX) -o $@ $^

test_perft : test_perft.o board.o utils.o
	$(CXX) -o $@ $^

%.o : %.cpp chess.h
