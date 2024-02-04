#pragma once
#include <string>

namespace Ox {
class EmbedAsset {
public:
  static void EmbedTexture(const std::string& texFilePath, const std::string& outPath, const std::string& arrayName);
};
}
