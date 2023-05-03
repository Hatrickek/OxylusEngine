#pragma once

#include <optional>

namespace Oxylus {
  class FileUtils {
  public:
    static std::optional<std::string> ReadFile(const std::string& filePath);
  };
}
