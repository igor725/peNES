#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

template <typename _AddrType, _AddrType _HanStart, _AddrType _HanEnd = std::numeric_limits<_AddrType>::max(), size_t _UngovSize = _HanStart,
          bool _UngovAtEnd = false, size_t _NumBits = 5, size_t _MaxHandlers = 4>
class MMU {
  static constexpr size_t _BucketSize = 1 << _NumBits;

  static_assert(_UngovAtEnd || (_UngovSize <= _HanStart));
  static_assert(_HanEnd > _HanStart && (_HanEnd - _HanStart >= _BucketSize));
  static_assert((_BucketSize > 0) && ((_BucketSize & (_BucketSize - 1)) == 0));
  static constexpr size_t _NumBuckets = ((_HanEnd - _HanStart) + (_UngovAtEnd ? 0 : 1)) / _BucketSize;

  public:
  using Interval     = std::pair<_AddrType, _AddrType>;
  using Handler      = std::function<std::optional<uint8_t>(bool isWrite, _AddrType addr, uint8_t value)>;
  using UngovMemType = std::array<uint8_t, _UngovSize>;

  static constexpr size_t UngovernedMemorySize = _UngovSize;

  MMU() { std::fill(m_memBuckets.begin(), m_memBuckets.end(), -1); }

  virtual ~MMU() = default;

  void addRangeHandler(Interval memRange, Handler han) {
    auto const memStart = memRange.first - _HanStart;
    auto const memEnd   = memRange.second - _HanStart;
    auto const memSize  = (memEnd - memStart) + 1;
    if (((memStart & (_BucketSize - 1)) != 0x00) || ((memSize & (_BucketSize - 1)) != 0x00)) throw std::runtime_error("Misaligned memory range");

    int8_t hanNum = -1;
    for (int8_t i = 0; i < m_memHandlers.size(); ++i) {
      if (m_memHandlers[i] == nullptr) {
        m_memHandlers[i] = std::move(han);
        hanNum           = i;
        break;
      }
    }
    if (hanNum == -1) throw std::runtime_error("Too many handlers installed");

    for (_AddrType currBucket = memStart / _BucketSize; currBucket <= memEnd / _BucketSize; ++currBucket) {
      if (m_memBuckets[currBucket] != -1) throw std::runtime_error("MMU collision detected");
      m_memBuckets[currBucket] = hanNum;
    }
  }

  UngovMemType dumpUngoverned() const { return m_ungovMemory; }

  void restoreUngoverned(UngovMemType const& state) { m_ungovMemory = state; }

  protected:
  std::optional<uint8_t> readByte(_AddrType address) const {
    if constexpr (_UngovAtEnd) {
      if (address >= _HanEnd) return m_ungovMemory[((address - _HanEnd) & (_UngovSize - 1))];
    } else {
      if (address < _HanStart) return m_ungovMemory[address & (_UngovSize - 1)];
    }
    auto const handlerIdx = m_memBuckets[(address - _HanStart) >> _NumBits];
    if (handlerIdx == -1) return {};
    return m_memHandlers[handlerIdx](false, address, 0);
  }

  uint8_t writeByte(_AddrType address, uint8_t value) {
    if constexpr (_UngovAtEnd) {
      if (address >= _HanEnd) return m_ungovMemory[((address - _HanEnd) & (_UngovSize - 1))] = value;
    } else {
      if (address < _HanStart) return m_ungovMemory[address & (_UngovSize - 1)] = value;
    }
    auto const handlerIdx = m_memBuckets[(address - _HanStart) >> _NumBits];
    if (handlerIdx == -1) return {};
    if (auto const ret = m_memHandlers[handlerIdx](true, address, value); ret.has_value()) return ret.value();
    return value;
  }

  UngovMemType                      m_ungovMemory = {};
  std::array<Handler, _MaxHandlers> m_memHandlers = {};
  std::array<int8_t, _NumBuckets>   m_memBuckets  = {};
};
