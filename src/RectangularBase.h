// RectangularBase.h

#ifndef RECTANGULAR_BASE_H
#define RECTANGULAR_BASE_H

class RectangularBase
{
 protected:

  int width;
  int height;

  RectangularBase(int, int);

  virtual int calculate_layer(int row, int column) const = 0;
};

#endif
