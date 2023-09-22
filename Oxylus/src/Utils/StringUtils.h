#pragma once

#include <string_view>

namespace Oxylus {
class StringUtils {
public:
  struct StringHash {
    using is_transparent = void;

    size_t operator()(const char* txt) const {
      return std::hash<std::string_view>{}(txt);
    }

    size_t operator()(std::string_view txt) const {
      return std::hash<std::string_view>{}(txt);
    }

    size_t operator()(const std::string& txt) const {
      return std::hash<std::string>{}(txt);
    }
  };

#define UM_StringTransparentEquality StringUtils::StringHash, std::equal_to<>

  static void ReplaceString(std::string& subject, std::string_view search, std::string_view replace) {
    size_t pos = 0;
    while ((pos = subject.find(search, pos)) != std::string::npos) {
      subject.replace(pos, search.length(), replace);
      pos += replace.length();
    }
  }

  static const char* FromChar8T(const char8_t* c) {
    return reinterpret_cast<const char*>(c);
  }
};
}
