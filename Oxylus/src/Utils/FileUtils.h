#pragma once

#include <optional>

namespace Oxylus {
  class FileUtils {
  public:
    static std::optional<std::string> ReadFile(const std::string& filePath);

    /// Converts directory seperators in the preferred format.
    /// Windows: '\' POSIX: '/'
    static std::string GetPreferredPath(const std::string& path);
  };
}
