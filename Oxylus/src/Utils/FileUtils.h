#pragma once

#include <string_view>

namespace Oxylus {
  class FileUtils {
  public:
    static std::optional<std::string> ReadFile(const std::string& filePath);
  };
}
