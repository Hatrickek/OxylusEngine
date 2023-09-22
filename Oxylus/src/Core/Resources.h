#pragma once
#include <string>

#include "Base.h"


namespace vuk {
struct Texture;
}

namespace Oxylus {
class TextureAsset;

class Resources {
public:
  static struct EditorRes {
    Ref<TextureAsset> EngineIcon;
  } s_EditorResources;

  static void InitEditorResources();

  static std::string GetResourcesPath(const std::string& path);
  static bool ResourcesPathExists();
};
}
