// RowMajorOrderBase.h

#ifndef ROW_MAJOR_ORDER_BASE_H
#define ROW_MAJOR_ORDER_BASE_H

#include "RectangularBase.h"

class RowMajorOrderBase
: virtual public RectangularBase
{
 protected:

  RowMajorOrderBase(int, int);

  virtual int calculate_layer(int r, int c) const {return r * width + c;};
};

#endif
