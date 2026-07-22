#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

class MMU {
  public:
  using Interval = std::pair<uint16_t, uint16_t>;
  using Handler  = std::function<std::optional<uint8_t>(bool isWrite, uint16_t addr, uint8_t value)>;

  MMU(uint16_t bo);
  virtual ~MMU() = default;

  void           addRangeHandler(Interval memRange, Handler han);
  Handler const* findHandler(uint16_t address) const;

  private:
  uint16_t                 m_baseOffset = 0;
  std::array<Handler, 10>  m_handlers   = {};
  std::array<int8_t, 1792> m_buckets    = {};
};
