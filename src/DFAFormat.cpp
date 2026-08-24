// DFAFormat.cpp

#include "DFAFormat.h"

#include <stdexcept>
#include <string>

namespace dfa_format
{
  uint64_t align8(uint64_t x)
  {
    if(x > UINT64_MAX - 7)
      {
	throw std::runtime_error("DFA layout overflow rounding up to 8 bytes");
      }
    return (x + 7) & ~uint64_t(7);
  }

  int width_for(uint64_t next_layer_size)
  {
    if(next_layer_size <= (uint64_t(1) << 8))
      {
	return 1;
      }
    if(next_layer_size <= (uint64_t(1) << 16))
      {
	return 2;
      }
    if(next_layer_size <= (uint64_t(1) << 32))
      {
	return 4;
      }
    return 8;
  }

  // Every product below goes through these, so a shape that cannot be
  // addressed fails loudly here rather than producing a wrong offset. The C++
  // has been bitten by exactly that before, in get_linear_bound.
  static uint64_t checked_mul(uint64_t a, uint64_t b, const char *what)
  {
    if(a != 0 && b > UINT64_MAX / a)
      {
	throw std::runtime_error(std::string("DFA layout overflow multiplying ") + what);
      }
    return a * b;
  }

  static uint64_t checked_add(uint64_t a, uint64_t b, const char *what)
  {
    if(b > UINT64_MAX - a)
      {
	throw std::runtime_error(std::string("DFA layout overflow adding ") + what);
      }
    return a + b;
  }

  Layout::Layout(const std::vector<int>& shape_in,
		 const std::vector<uint64_t>& layer_size_in)
    : shape(shape_in),
      layer_size(layer_size_in),
      width(),
      block_bytes(),
      layer_offset(),
      tables_end_value(0),
      file_length(0)
  {
    int ndim = int(shape.size());
    if(ndim < 1)
      {
	throw std::runtime_error("DFA layout needs at least one layer");
      }
    if(layer_size.size() != shape.size())
      {
	throw std::runtime_error("DFA layout shape and layer size tables disagree");
      }

    for(int layer = 0; layer < ndim; ++layer)
      {
	if(shape[layer] < 1)
	  {
	    throw std::runtime_error("DFA layout shape must be at least 1");
	  }
	if(layer_size[layer] < 2)
	  {
	    throw std::runtime_error("DFA layout layer size must be at least 2");
	  }
      }

    // header + layer_size[] + layer_offset[] + shape[] = 64 + 20 * ndim
    tables_end_value = checked_add(off_tables,
				   checked_mul(20, uint64_t(ndim), "table bytes"),
				   "end of tables");

    width.reserve(size_t(ndim));
    block_bytes.reserve(size_t(ndim));
    layer_offset.reserve(size_t(ndim));

    uint64_t offset = align8(tables_end_value);
    for(int layer = 0; layer < ndim; ++layer)
      {
	int layer_width = width_for(next_layer_size(layer));
	uint64_t entries = checked_mul(layer_size[layer],
				       uint64_t(shape[layer]),
				       "block entries");
	uint64_t bytes = checked_mul(entries, uint64_t(layer_width), "block bytes");

	layer_offset.push_back(offset);
	width.push_back(layer_width);
	block_bytes.push_back(bytes);

	uint64_t end = checked_add(offset, bytes, "block end");
	// Blocks start on an 8 byte boundary; the last one is followed by EOF.
	offset = (layer + 1 < ndim) ? align8(end) : end;
      }

    file_length = offset;
  }

  uint64_t Layout::next_layer_size(int layer) const
  {
    int ndim = int(shape.size());
    if(layer + 1 < ndim)
      {
	return layer_size.at(size_t(layer) + 1);
      }
    return terminal_layer_size;
  }

  uint64_t Layout::row_bytes(int layer) const
  {
    return uint64_t(shape.at(layer)) * uint64_t(width.at(layer));
  }

  uint64_t Layout::row_offset(int layer, uint64_t row) const
  {
    return layer_offset.at(layer) + row * row_bytes(layer);
  }
}
