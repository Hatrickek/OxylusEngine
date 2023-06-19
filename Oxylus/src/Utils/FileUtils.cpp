#include "FileUtils.h"

#include <fstream>

#include "Utils/Log.h"

namespace Oxylus {

  std::optional<std::string> FileUtils::ReadFile(const std::string& filePath) {
    const std::ifstream file(filePath);

    std::stringstream buffer;
    buffer << file.rdbuf();

    if (buffer.str().empty()) {
      return {};
    }

    return buffer.str();
  }

  std::string FileUtils::GetPreferredPath(const std::string& path) {
    return std::filesystem::path(path).make_preferred().string();
  }
}
