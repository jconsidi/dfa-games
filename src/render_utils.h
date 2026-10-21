// render_utils.h

// Helpers shared by the programs that write standalone HTML pages from a
// template with placeholders (render_move_graph, render_test_positions).

#ifndef RENDER_UTILS_H
#define RENDER_UTILS_H

#include <string>

// JSON placed inside a <script> element must not contain "</", or a
// string in it could end the element early.
std::string script_safe(std::string json);

// replace the single occurrence of placeholder in text, throwing unless
// it occurs exactly once.
void replace_once(std::string& text, const std::string& placeholder, const std::string& value);

// write page to output_path, throwing if it cannot be opened or written.
void write_page(const std::string& output_path, const std::string& page);

#endif
