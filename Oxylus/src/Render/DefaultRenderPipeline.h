#pragma once
#include "RenderPipeline.h"
#include "Core/Entity.h"
#include "Scene/Scene.h"
#include "Vulkan/VulkanRenderer.h"

namespace Oxylus {
constexpr auto MAX_NUM_LIGHTS = 1000;
constexpr auto MAX_NUM_MESHES = 1000;
constexpr auto MAX_NUM_LIGHTS_PER_TILE = 128;
constexpr auto MAX_NUM_FRUSTUMS = 20000;
constexpr auto PIXELS_PER_TILE = 16;
constexpr auto TILES_PER_THREADGROUP = 16;

class DefaultRenderPipeline : public RenderPipeline {
public:
  explicit DefaultRenderPipeline(const std::string& name)
    : RenderPipeline(name) {}

  void Init(Scene* scene) override;
  void Shutdown() override;
  void Update(Scene* scene) override;
  Scope<vuk::Future> OnRender(vuk::Allocator& frameAllocator, const vuk::Future& target) override;
  void OnDispatcherEvents(EventDispatcher& dispatcher) override;

private:
  struct RendererContext {
    //Camera
    Camera* CurrentCamera = nullptr;
  } m_RendererContext;

  struct RendererData {
    struct Vertex {
      Vec3 Position{};
      Vec3 Normal{};
      Vec2 UV{};
      Vec4 Color{};
      Vec4 Joint0{};
      Vec4 Weight0{};
      Vec4 Tangent{};
    };

    struct Frustum {
      Vec4 planes[4] = {Vec4(0)};
    } Frustums[MAX_NUM_FRUSTUMS];

    struct UBOVS {
      Mat4 Projection;
      Mat4 View;
      Vec3 CamPos;
    } m_UBO_VS;

    struct FinalPassData {
      int Tonemapper = RendererConfig::TONEMAP_ACES;          // 0- Aces 1- Uncharted 2-Filmic 3- Reinhard
      float Exposure = 1.0f;
      float Gamma = 2.5f;
      int EnableSSAO = 1;
      int EnableBloom = 1;
      int EnableSSR = 1;
      Vec2 _pad{};
      Vec4 VignetteColor = Vec4(0.0f, 0.0f, 0.0f, 0.25f);     // rgb: color, a: intensity
      Vec4 VignetteOffset = Vec4(0.0f, 0.0f, 0.0f, 1.0f);     // xy: offset, z: useMask, w: enable effect
      Vec2 FilmGrain = {};                                    // x: enable, y: amount
      Vec2 ChromaticAberration = {};                          // x: enable, y: amount
      Vec2 Sharpen = {};                                      // x: enable, y: amount
    } m_FinalPassData;
  } m_RendererData = {};

  struct RendererResources {
    Ref<TextureAsset> CubeMap;
    Ref<vuk::Texture> LutBRDF;
    Ref<vuk::Texture> IrradianceCube;
    Ref<vuk::Texture> PrefilteredCube;
  } m_Resources;

  Scene* m_Scene = nullptr;

  bool m_ForceUpdateMaterials = false;

  // PBR Resources
  vuk::Unique<vuk::Image> m_BRDFImage;
  vuk::Unique<vuk::Image> m_IrradianceImage;
  vuk::Unique<vuk::Image> m_PrefilteredImage;

  // Mesh
  std::vector<VulkanRenderer::MeshData> m_MeshDrawList;
  std::vector<VulkanRenderer::MeshData> m_TransparentMeshDrawList;
  vuk::Unique<vuk::Buffer> m_ParametersBuffer;

  struct LightChangeEvent {};

  std::vector<Entity> m_SceneLights = {};
  std::vector<VulkanRenderer::LightingData> m_PointLightsData = {};
  EventDispatcher m_LightBufferDispatcher;
  Ref<Mesh> m_SkyboxCube = nullptr;

  void InitRenderGraph();
  void UpdateSkybox(const SceneRenderer::SkyboxLoadEvent& e);
  void GeneratePrefilter();
  void UpdateParameters(SceneRenderer::ProbeChangeEvent& e);
  void UpdateLightingData();
};
}
