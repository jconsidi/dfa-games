// test_union_dfa.cpp

#include <iostream>
#include <string>

#include "ChessDFA.h"
#include "CountCharacterDFA.h"
#include "CountDFA.h"
#include "IntersectionDFA.h"
#include "UnionDFA.h"

void test_helper(std::string test_name, const ChessDFA& test_dfa, int expected_boards)
{
  int actual_boards = test_dfa.size();
  if(actual_boards != expected_boards)
    {
      std::cerr << test_name << ": expected " << expected_boards << std::endl;
      std::cerr << test_name << ":   actual " << actual_boards << std::endl;

      throw std::logic_error("test failed");
    }
  std::cout << " passed" << std::endl;
  std::cout.flush();
}

void test_intersection_pair(std::string test_name, const ChessDFA& left, const ChessDFA& right, int expected_boards)
{
  std::cout << "checking intersection pair " << test_name << std::endl;
  std::cout.flush();

  IntersectionDFA test_dfa(left, right);
  test_helper("intersection pair " + test_name, test_dfa, expected_boards);
}

void test_intersection_vector(std::string test_name, const std::vector<const ChessDFA *> dfas_in, int expected_boards)
{
  std::cout << "checking intersection vector " << test_name << std::endl;
  std::cout.flush();

  ChessIntersectionDFA test_dfa(dfas_in);
  test_helper("intersection vector " + test_name, test_dfa, expected_boards);
}

void test_union_pair(std::string test_name, const ChessDFA& left, const ChessDFA& right, int expected_boards)
{
  std::cout << "checking union pair " << test_name << std::endl;
  std::cout.flush();

  ChessUnionDFA test_dfa(left, right);
  test_helper("union pair " + test_name, test_dfa, expected_boards);
}

void test_union_vector(std::string test_name, const std::vector<const ChessDFA *> dfas_in, int expected_boards)
{
  std::cout << "checking union vector " << test_name << std::endl;
  std::cout.flush();

  UnionDFA test_dfa(dfas_in);
  test_helper("union vector " + test_name, test_dfa, expected_boards);
}

int main()
{
  try
    {
      // count tests

      ChessCountDFA count0(0);
      test_helper("count0", count0, 1);

      ChessCountDFA count1(1);
      test_helper("count1", count1, 12 * 64);

      ChessCountDFA count2(2);
      test_helper("count2", count2, (12 * 12) * (64 * 63 / 2));

      ChessCountDFA count3(3);
      test_helper("count3", count3, (12 * 12 * 12) * (64 * 63 * 62 / 3 / 2));

      // intersection tests

      test_intersection_pair("count0+count0", count0, count0, count0.size());
      test_intersection_pair("count0+count1", count0, count1, 0);
      test_intersection_pair("count1+count1", count1, count1, count1.size());
      test_intersection_pair("count2+count3", count2, count3, 0);

      // union tests

      test_union_pair("count0+count0", count0, count0, count0.size());
      test_union_pair("count0+count1", count0, count1, count0.size() + count1.size());
      test_union_pair("count1+count0", count1, count0, count0.size() + count1.size());
      test_union_pair("count1+count1", count1, count1, count1.size());
      test_union_pair("count2+count3", count2, count3, count2.size() + count3.size());

      test_union_vector("count0", std::vector<const ChessDFA *>({&count0}), count0.size());
      test_union_vector("count0+count1", std::vector<const ChessDFA *>({&count0, &count1}), count0.size() + count1.size());
      test_union_vector("count0+count1+count2", std::vector<const ChessDFA *>({&count0, &count1, &count2}), count0.size() + count1.size() + count2.size());
      test_union_vector("count0+count1+count2+count3", std::vector<const ChessDFA *>({&count0, &count1, &count2, &count3}), count0.size() + count1.size() + count2.size() + count3.size());

      // count character tests

      ChessCountCharacterDFA white_king_0(DFA_WHITE_KING, 0);
      test_intersection_pair("white king 0 + count1", white_king_0, count1, 11 * 64);

      ChessCountCharacterDFA white_king_1(DFA_WHITE_KING, 1);
      test_intersection_pair("white king 1 + count1", white_king_1, count1, 64);

      ChessCountCharacterDFA black_king_1(DFA_BLACK_KING, 1);
      test_intersection_pair("black king 1 + count1", black_king_1, count1, 64);

      test_intersection_vector("white king 1 + black king 1 + count2", std::vector<const ChessDFA *>({&white_king_1, &black_king_1, &count2}), 64 * 63);
    }
  catch(std::logic_error e)
    {
      std::cerr << e.what() << std::endl;
      std::cerr.flush();
      return 1;
    }

  return 0;
}
