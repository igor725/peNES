#include "mmu.hh"

// #include <algorithm>
#include <stdexcept>

MMU::MMU(uint16_t bo): m_baseOffset(bo) {
  std::fill(m_buckets.begin(), m_buckets.end(), -1);
}

void MMU::addRangeHandler(Interval memRange, Handler han) {
  auto const memStart = memRange.first - m_baseOffset;
  auto const memEnd   = memRange.second - m_baseOffset;
  auto const memSize  = (memEnd - memStart) + 1;
  if (((memStart & 0x1F) != 0x00) || ((memSize & 0x1F) != 0x00)) throw std::runtime_error("Misaligned memory range");

  int8_t hanNum = -1;
  for (uint8_t i = 0; i < m_handlers.size(); ++i) {
    if (m_handlers[i] == nullptr) {
      m_handlers[i] = std::move(han);
      hanNum        = i;
      break;
    }
  }
  if (hanNum == -1) throw std::runtime_error("Too many handlers installed");

  for (uint16_t currBucket = memStart / 32; currBucket <= memEnd / 32; ++currBucket) {
    if (m_buckets[currBucket] != -1) throw std::runtime_error("MMU collision detected");
    m_buckets[currBucket] = hanNum;
  }
}

MMU::Handler const* MMU::findHandler(uint16_t address) const {
  if (address < m_baseOffset) return nullptr;
  auto const handlerIdx = m_buckets[(address - m_baseOffset) >> 5];
  if (handlerIdx == -1) return nullptr;
  return &m_handlers[handlerIdx];
}
