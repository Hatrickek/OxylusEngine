#pragma once

#include <string_view>

namespace Oxylus {
  class FileUtils {
  public:
    static std::string ReadFile(const std::string& filePath, bool logError = true);
  };
}
