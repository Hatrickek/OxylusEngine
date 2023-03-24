#pragma once

namespace Oxylus {
  struct AppSpec;

  class Core {
  public:
    enum class RenderBackend {
      Vulkan
    };

    static void Init(const AppSpec& spec);
    static void Shutdown();
    static RenderBackend s_Backend;
  };
}
