#pragma once
namespace Ox {
class VulkanContext;
struct AppSpec;

class Core {
public:
  bool init(const AppSpec& spec);
  void shutdown();
};
}
