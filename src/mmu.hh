#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <stdexcept>
#include <utility>

template <typename _AddrType, _AddrType _MemOffset, _AddrType _MemEnd = std::numeric_limits<_AddrType>::max(), size_t _BucketSize = 32, size_t _MaxHandlers = 4>
class MMU {
  static_assert(_MemEnd > _MemOffset && (_MemEnd - _MemOffset >= _BucketSize));
  static_assert((_BucketSize > 0) && ((_BucketSize & (_BucketSize - 1)) == 0));
  static constexpr size_t _NumBuckets = ((_MemEnd - _MemOffset) / 2) + 1;

  public:
  using Interval = std::pair<_AddrType, _AddrType>;
  using Handler  = std::function<std::optional<uint8_t>(bool isWrite, _AddrType addr, uint8_t value)>;

  MMU() { std::fill(m_buckets.begin(), m_buckets.end(), -1); }

  virtual ~MMU() = default;

  void addRangeHandler(Interval memRange, Handler han) {
    auto const memStart = memRange.first - _MemOffset;
    auto const memEnd   = memRange.second - _MemOffset;
    auto const memSize  = (memEnd - memStart) + 1;
    if (((memStart & (_BucketSize - 1)) != 0x00) || ((memSize & (_BucketSize - 1)) != 0x00)) throw std::runtime_error("Misaligned memory range");

    int8_t hanNum = -1;
    for (int8_t i = 0; i < m_handlers.size(); ++i) {
      if (m_handlers[i] == nullptr) {
        m_handlers[i] = std::move(han);
        hanNum        = i;
        break;
      }
    }
    if (hanNum == -1) throw std::runtime_error("Too many handlers installed");

    for (_AddrType currBucket = memStart / _BucketSize; currBucket <= memEnd / _BucketSize; ++currBucket) {
      if (m_buckets[currBucket] != -1) throw std::runtime_error("MMU collision detected");
      m_buckets[currBucket] = hanNum;
    }
  }

  Handler const* findHandler(_AddrType address) const {
    if (address < _MemOffset) return nullptr;
    auto const handlerIdx = m_buckets[(address - _MemOffset) >> 5];
    if (handlerIdx == -1) return nullptr;
    return &m_handlers[handlerIdx];
  }

  private:
  std::array<Handler, _MaxHandlers> m_handlers = {};
  std::array<int8_t, _NumBuckets>   m_buckets  = {};
};
