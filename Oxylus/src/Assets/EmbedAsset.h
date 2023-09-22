#pragma once
#include <string>

namespace Oxylus {
class EmbedAsset {
public:
  static void EmbedTexture(const std::string& texFilePath, const std::string& outPath, const std::string& arrayName);
};
}
