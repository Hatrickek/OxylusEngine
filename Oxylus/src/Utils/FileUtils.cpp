#include "src/oxpch.h"
#include "FileUtils.h"

#include <fstream>

#include "Utils/Log.h"

namespace Oxylus {
  std::string FileUtils::ReadFile(const std::string& filePath, bool logError) {
    const std::ifstream file(filePath);

    std::stringstream buffer;
    buffer << file.rdbuf();

    if (buffer.str().empty() && logError)
      OX_CORE_FATAL("Couldn't load file: {0}", filePath);

    return buffer.str();
  }
}
