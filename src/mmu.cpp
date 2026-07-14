#include "mmu.hh"

#include <algorithm>
#include <stdexcept>

void MMU::addRangeHandler(Interval memRange, Handler han) {
  for (auto it = m_handlers.begin(); it != m_handlers.end(); ++it) {
    if (std::max(memRange.first, it->first.first) <= std::min(memRange.second, it->first.second)) throw std::runtime_error("MMU collision detected");
  }
  m_handlers.emplace_back(std::make_pair(memRange, han));
}

MMU::List::const_iterator MMU::findHandler(uint16_t address) const {
  for (auto it = m_handlers.begin(); it != m_handlers.end(); ++it) {
    if (address >= it->first.first && address <= it->first.second) return it;
  }
  return m_handlers.end();
}
