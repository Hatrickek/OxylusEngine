#pragma once
#include "Base.h"

namespace Oxylus {
class VulkanContext;
struct AppSpec;

class Core {
public:
  bool Init(const AppSpec& spec);
  void Shutdown();
};
}
