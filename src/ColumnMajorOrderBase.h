// ColumnOrderBase.h

#ifndef COLUMN_MAJOR_ORDER_BASE_H
#define COLUMN_MAJOR_ORDER_BASE_H

#include "RectangularBase.h"

class ColumnMajorOrderBase
: virtual public RectangularBase
{
 protected:

  ColumnMajorOrderBase(int, int);

  virtual int calculate_layer(int r, int c) const {return c * height + r;};
};

#endif
