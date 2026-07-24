#pragma once

#include <cstdint>
#include <cstring>
#include <ranges>
#include <stdexcept>
#include <type_traits>
#include <vector>

class MapperDumper {
  private:
  std::vector<uint8_t> m_dump;

  public:
  MapperDumper()          = default;
  virtual ~MapperDumper() = default;
  MapperDumper(std::vector<uint8_t>& state): m_dump(state) {};

  std::vector<uint8_t> extract() { return std::move(m_dump); }

  template <typename T>
  void push(T const& value) {
    using nrT = std::remove_cvref_t<T>;
    static_assert(std::is_trivially_copyable_v<nrT> || std::ranges::range<nrT>, "Type must be trivially copyable or an iterable range");

    if constexpr (std::is_trivially_copyable_v<nrT>) {
      const uint8_t* ptr = reinterpret_cast<const uint8_t*>(&value);
      m_dump.insert(m_dump.end(), ptr, ptr + sizeof(nrT));
    } else if constexpr (std::ranges::range<nrT>) {
      size_t sz = std::ranges::size(value);
      push(sz);

      for (auto const& item: value) {
        push(item);
      }
    }
  }

  template <typename T>
  T pop() {
    using nrT = std::remove_cvref_t<T>;
    static_assert(std::is_trivially_copyable_v<nrT> || std::ranges::range<nrT>, "Type must be trivially copyable or an iterable range");

    if constexpr (std::is_trivially_copyable_v<nrT>) {
      if (m_dump.size() < sizeof(nrT)) {
        throw std::runtime_error("Insufficient bytes in dump to pop type");
      }

      nrT val;
      std::memcpy(&val, m_dump.data(), sizeof(nrT));
      m_dump.erase(m_dump.begin(), m_dump.begin() + sizeof(nrT));

      return val;
    } else if constexpr (std::ranges::range<nrT>) {
      size_t sz = pop<size_t>();
      nrT    container;

      if constexpr (requires { container.reserve(sz); }) {
        container.reserve(sz);
      }

      using ValueType = std::ranges::range_value_t<nrT>;
      if constexpr (requires { container.push_back(std::declval<ValueType>()); }) {
        for (size_t i = 0; i < sz; ++i) {
          container.push_back(pop<ValueType>());
        }
      } else {
        if (std::ranges::size(container) != sz) throw std::runtime_error("Iterable container size mismatch");

        for (auto& item: container) {
          item = pop<ValueType>();
        }
      }

      return container;
    }
  }
};
