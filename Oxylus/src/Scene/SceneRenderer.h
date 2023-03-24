#pragma once
namespace Oxylus {
  class Scene;

  class SceneRenderer {
  public:
    SceneRenderer() = default;
    ~SceneRenderer() = default;

    void Render(Scene& scene) const;
  };
}
