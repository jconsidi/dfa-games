# Makefile

CXX=g++

CXXFLAGS=-Wall -Werror -Wno-c++11-extensions --pedantic -std=c++20

all : perft test_generate_moves

test : perft test_generate_moves
	./test_generate_moves
	./perft

perft : perft.o board.o
	$(CXX) -o $@ $^

test_generate_moves : test_generate_moves.o board.o
	$(CXX) -o $@ $^

%.o : %.cpp chess.h
