#pragma once

#include <functional>
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

    static std::string GetExtension(std::string_view filepath) {
      auto lastDot = filepath.find_last_of('.');
      return static_cast<std::string>(filepath.substr(lastDot + 1, filepath.size() - lastDot));
    }

    static std::string GetName(std::string_view filepath) {
      auto lastSlash = filepath.find_last_of("/\\");
      lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
      auto lastDot = filepath.rfind('.');
      auto count = lastDot == std::string::npos ? filepath.size() - lastSlash : lastDot - lastSlash;
      return static_cast<std::string>(filepath.substr(lastSlash, count));
    }

    static std::string GetNameWithExtension(std::string_view filepath) {
      auto lastSlash = filepath.find_last_of("/\\");
      lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
      return static_cast<std::string>(filepath.substr(lastSlash, filepath.size()));
    }

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
