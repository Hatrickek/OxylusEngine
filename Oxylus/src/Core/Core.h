#pragma once
namespace Oxylus {
class VulkanContext;
struct AppSpec;

class Core {
public:
  bool init(const AppSpec& spec);
  void shutdown();
};
}
