#pragma once

#include <string>
#include <cstdint>

namespace Ox {
namespace StringUtils {
constexpr uint32_t fnv1a_32(char const* s, std::size_t count) {
  return ((count ? fnv1a_32(s, count - 1) : 2166136261u) ^ s[count]) * 16777619u;
}

constexpr size_t const_strlen(const char* s) {
  size_t size = 0;
  while (s[size]) { size++; };
  return size;
}

struct StringHash {
  using is_transparent = void;

  uint32_t computed_hash = 0;

  constexpr StringHash(uint32_t hash) noexcept : computed_hash(hash) {}

  constexpr StringHash(const char* s) noexcept : computed_hash(0) {
    computed_hash = fnv1a_32(s, const_strlen(s));
  }

  constexpr StringHash(const char* s, std::size_t count) noexcept : computed_hash(0) {
    computed_hash = fnv1a_32(s, count);
  }

  constexpr StringHash(std::string_view s) noexcept : computed_hash(0) {
    computed_hash = fnv1a_32(s.data(), s.size());
  }

  StringHash(const StringHash& other) = default;

  constexpr operator uint32_t() const noexcept { return computed_hash; }
};

static void replace_string(std::string& subject, std::string_view search, std::string_view replace) {
  size_t pos = 0;
  while ((pos = subject.find(search, pos)) != std::string::npos) {
    subject.replace(pos, search.length(), replace);
    pos += replace.length();
  }
}

static const char* from_char8_t(const char8_t* c) {
  return reinterpret_cast<const char*>(c);
}
};
}
