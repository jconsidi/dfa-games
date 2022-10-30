// BinaryFunction.h

#ifndef BINARY_FUNCTION_H
#define BINARY_FUNCTION_H

#include <functional>

class BinaryFunction
{
 private:

  std::function<bool(bool, bool)> function;

 public:

  BinaryFunction(std::function<bool(bool, bool)>);

  bool operator()(bool, bool) const;

  bool has_left_sink(bool) const;
  bool has_right_sink(bool) const;

  bool is_commutative() const;
};

#endif
