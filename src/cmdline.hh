#pragma once

#include <charconv>
#include <cstdint>
#include <exception>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>
#include <vector>

class UnknownCmdlineParameter: public std::exception {};

class CmdlineParser {
  template <typename T1, typename T2>
  static constexpr size_t HASH(T1 begin, T2 end) {
    static_assert(std::input_iterator<T1> && std::input_iterator<T2> && std::sentinel_for<T1, T2>);
    size_t hash = 0xcbf29ce484222325;
    for (auto it = begin; it != end; ++it) {
      hash ^= static_cast<size_t>(*it);
      hash *= 0x100000001b3;
    }
    return hash;
  }

  static consteval size_t HASH_STR(std::string_view str) { return HASH(str.begin(), str.end()); }

  enum class ArgType { Int, Double, String, Bool };
  using ArgVariant = std::variant<int64_t, double, std::string, bool>;

  template <ArgType T>
  struct ArgTypeTraits;

  template <>
  struct ArgTypeTraits<ArgType::Int> {
    using type = int64_t;
  };

  template <>
  struct ArgTypeTraits<ArgType::Double> {
    using type = double;
  };

  template <>
  struct ArgTypeTraits<ArgType::String> {
    using type = std::string;
  };

  template <>
  struct ArgTypeTraits<ArgType::Bool> {
    using type = bool;
  };

  template <ArgType T>
  using ArgType_t = typename ArgTypeTraits<T>::type;

  static constexpr ArgType getExpectedType(size_t hash) {
    switch (hash) {
      case HASH_STR("volume"): return ArgType::Double;
      case HASH_STR("hook"): return ArgType::String;
      case HASH_STR("hmthreshold"): return ArgType::Int;
      case HASH_STR("skipvalid"): return ArgType::Bool;
      default: throw UnknownCmdlineParameter();
    }
  }

  template <size_t N>
  struct ArgName {
    char buf[N];

    constexpr ArgName(char const (&s)[N]) { std::ranges::copy_n(s, N, buf); }

    constexpr size_t hash() const { return HASH_STR(buf); }
  };

  public:
  void parse(int32_t argc, char* argv[]) {
    bool numMode = false;
    for (int32_t i = 1; i < argc; ++i) {
      std::string_view arg = argv[i];
      if (arg.empty()) continue;
      if (numMode || !arg.starts_with("--")) {
        numMode = true;
        m_seq.emplace_back(std::string(arg));
        continue;
      }

      auto const hash = HASH(arg.begin() + 2, arg.end());

      if (++i < argc) {
        std::string_view valStr = argv[i];
        switch (getExpectedType(hash)) {
          case ArgType::Int: {
            int64_t v = 0;
            std::from_chars(valStr.data(), valStr.end(), v);
            m_list[hash] = v;
          } break;
          case ArgType::Double: {
            double v = 0.0;
            std::from_chars(valStr.data(), valStr.end(), v);
            m_list[hash] = v;
          } break;
          case ArgType::Bool: {
            m_list[hash] = (valStr == "true" || valStr == "1");
          } break;

          default: m_list[hash] = std::string(valStr); break;
        }
      } else {
        m_list[hash] = true;
      }
    }
  }

  template <ArgName Str>
  constexpr auto getNamedArg(std::optional<ArgType_t<getExpectedType(Str.hash())>> deflt = {}) const {
    constexpr ArgType argType = getExpectedType(Str.hash());
    using ExpectedType        = ArgType_t<argType>;

    if (auto it = m_list.find(Str.hash()); it != m_list.end()) {
      if (const ExpectedType* val = std::get_if<ExpectedType>(&it->second)) {
        return std::optional<ExpectedType>(*val);
      }
    }

    return std::move(deflt);
  }

  template <typename T>
  constexpr std::optional<T> getSeqArg(size_t index, std::optional<T> deflt = {}) const {
    if (index < m_seq.size()) {
      if constexpr (std::is_same_v<T, std::string>) {
        return m_seq.at(index);
      } else if constexpr (std::is_constructible_v<T, std::string>) {
        return T(m_seq.at(index));
      }
    }

    return std::move(deflt);
  }

  private:
  std::map<size_t, ArgVariant> m_list;
  std::vector<std::string>     m_seq;
};
