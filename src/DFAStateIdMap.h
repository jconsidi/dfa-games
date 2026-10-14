// DFAStateIdMap.h

#ifndef DFA_STATE_ID_MAP_H
#define DFA_STATE_ID_MAP_H

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <variant>

#include "DFA.h"
#include "MemoryMap.h"

// An array of state ids, narrow (uint32_t) or wide (uint64_t) by the number
// of states its ids range over, the same rule as DFAStagingLayer.
class DFAStateIdMap
{
  std::variant<MemoryMap<uint32_t>, MemoryMap<uint64_t>> ids;

  static std::variant<MemoryMap<uint32_t>, MemoryMap<uint64_t>> make_anonymous(size_t size_in, size_t state_count)
  {
    if(dfa_state_count_is_wide(state_count))
      {
        return MemoryMap<uint64_t>(size_in);
      }
    return MemoryMap<uint32_t>(size_in);
  }

public:

  explicit DFAStateIdMap(MemoryMap<uint32_t>&& ids_in) : ids(std::move(ids_in)) {}
  explicit DFAStateIdMap(MemoryMap<uint64_t>&& ids_in) : ids(std::move(ids_in)) {}

  // Anonymous, size_in ids, each below state_count.
  DFAStateIdMap(size_t size_in, size_t state_count) : ids(make_anonymous(size_in, state_count)) {}

  template <class F>
  decltype(auto) visit(F&& f) const
  {
    return std::visit(std::forward<F>(f), ids);
  }

  size_t size() const
  {
    return visit([](const auto& map) {return map.size();});
  }

  dfa_state_t get(size_t i) const
  {
    return visit([&](const auto& map) {return dfa_state_t(map[i]);});
  }

  void set(size_t i, dfa_state_t value)
  {
    std::visit([&](auto& map)
    {
      typedef std::remove_cvref_t<decltype(map[i])> id_t;
      assert(std::in_range<id_t>(value));
      map[i] = id_t(value);
    }, ids);
  }

  void unlink()
  {
    std::visit([](auto& map) {map.unlink();}, ids);
  }
};

#endif
