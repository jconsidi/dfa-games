# Makefile

CXX=g++

CXXFLAGS=-Wall -Werror -Wno-c++11-extensions --pedantic -std=c++20

all : test_generate_moves test_perft

test : test_generate_moves test_perft
	./test_generate_moves
	./test_perft

divide: divide.o board.o utils.o
	$(CXX) -o $@ $^

print: print.o board.o utils.o
	$(CXX) -o $@ $^

test_generate_moves : test_generate_moves.o board.o
	$(CXX) -o $@ $^

test_perft : test_perft.o board.o utils.o
	$(CXX) -o $@ $^

%.o : %.cpp chess.h
