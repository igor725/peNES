#pragma once

#include <cstdint>
#include <functional>
#include <list>
#include <optional>
#include <utility>

class MMU {
  public:
  using Interval = std::pair<uint16_t, uint16_t>;
  using Handler  = std::function<std::optional<uint8_t>(bool isWrite, uint16_t addr, uint8_t value)>;
  using List     = std::list<std::pair<Interval, Handler>>;

  MMU()          = default;
  virtual ~MMU() = default;

  void                 addRangeHandler(Interval memRange, Handler han);
  List::const_iterator findHandler(uint16_t address) const;

  bool isValidHandler(List::const_iterator it) const { return it != m_handlers.end(); }

  private:
  List m_handlers;
};
