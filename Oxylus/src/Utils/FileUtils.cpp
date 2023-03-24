#include "src/oxpch.h"
#include "FileUtils.h"

#include <fstream>

#include "Utils/Log.h"

namespace Oxylus {
  std::string FileUtils::ReadFile(const std::string& filePath) {
    const std::ifstream file(filePath);

    std::stringstream buffer;
    buffer << file.rdbuf();

    if (buffer.str().empty())
      OX_CORE_FATAL("Couldn't load file: {0}", filePath);

    return buffer.str();
  }
}
