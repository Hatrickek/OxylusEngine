#pragma once
#include <string>

#include "Base.h"


namespace vuk {
struct Texture;
}

namespace Ox {
class TextureAsset;

class Resources {
public:
  static struct EditorRes {
    Shared<TextureAsset> engine_icon;
  } editor_resources;

  static void init_editor_resources();

  static std::string get_resources_path(const std::string& path);
  static bool resources_path_exists();
};
}
