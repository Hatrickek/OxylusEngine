#pragma once
#include "RendererConfig.h"
#include "RenderGraph.h"
#include "RenderPipeline.h"
#include "Core/Entity.h"
#include "Scene/Scene.h"
#include "Vulkan/VulkanFramebuffer.h"
#include "Vulkan/VulkanPipeline.h"

namespace Oxylus {
  constexpr auto MAX_NUM_LIGHTS = 1000;
  constexpr auto MAX_NUM_MESHES = 1000;
  constexpr auto MAX_NUM_LIGHTS_PER_TILE = 128;
  constexpr auto MAX_NUM_FRUSTUMS = 20000;
  constexpr auto PIXELS_PER_TILE = 16;
  constexpr auto TILES_PER_THREADGROUP = 16;
  constexpr auto SHADOW_MAP_CASCADE_COUNT = 4;

  class DefaultRenderPipeline : public RenderPipeline {
  public:
    void OnInit() override;
    void OnRender(Scene* scene) override;
    const VulkanImage& GetFinalImage() override;
    void OnDispatcherEvents(EventDispatcher& dispatcher) override;
    void OnShutdown() override;

  protected:
    void InitRenderGraph() override;

  private:
    struct RendererContext {
      VulkanCommandBuffer LightListCommandBuffer;
      VulkanCommandBuffer DirectShadowCommandBuffer;
      VulkanCommandBuffer PBRPassCommandBuffer;
      VulkanCommandBuffer PostProcessCommandBuffer;
      VulkanCommandBuffer BloomPassCommandBuffer;
      VulkanCommandBuffer FrustumCommandBuffer;
      VulkanCommandBuffer DepthPassCommandBuffer;
      VulkanCommandBuffer SSAOCommandBuffer;
      VulkanCommandBuffer SSRCommandBuffer;
      VulkanCommandBuffer CompositeCommandBuffer;
      VulkanCommandBuffer AtmosphereCommandBuffer;
      VulkanCommandBuffer DepthOfFieldCommandBuffer;

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
        Mat4 projection;
        Mat4 view;
        Vec3 camPos;
      } UBO_VS;

      struct PBRPassParams {
        int numLights = 0;
        int debugMode = 0;
        float lodBias = 1.0f;
        IVec2 numThreads;
        IVec2 screenDimensions;
        IVec2 numThreadGroups;
      } UBO_PbrPassParams;

      struct UBOPostProcess {
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
      } UBO_PostProcessParams;

      struct SSAOParamsUB {
        Vec4 ssaoSamples[64] = {};
        float radius = 0.2f;
      } UBO_SSAOParams;

      struct SSAOBlurParamsUB {
        Vec4 texelOffset = {};
        int texelRadius = 2;
      } UBO_SSAOBlur;

      struct SSR_UBO {
        int Samples = 30;
        int BinarySearchSamples = 8;
        float MaxDist = 50.0f;
      } UBO_SSR;

      struct DirectShadowUB {
        Mat4 cascadeViewProjMat[SHADOW_MAP_CASCADE_COUNT]{};
        float cascadeSplits[4]{};
      } UBO_DirectShadow;

      struct AtmosphereUB {
        Mat4 InvViews[6] = {};
        Mat4 InvProjection{};
        Vec4 LightPos{}; // w: Intensity
        int ISteps = 40;
        int JSteps = 8;
        float Time = 0.15f; // Not used by shader.
      } UBO_Atmosphere;

      VulkanBuffer SkyboxBuffer;
      VulkanBuffer ParametersBuffer;
      VulkanBuffer VSBuffer;
      VulkanBuffer LightsBuffer;
      VulkanBuffer FrustumBuffer;
      VulkanBuffer LighIndexBuffer;
      VulkanBuffer LighGridBuffer;
      VulkanBuffer SSAOBuffer;
      VulkanBuffer PostProcessBuffer;
      VulkanBuffer DirectShadowBuffer;
      VulkanBuffer SSRBuffer;
      VulkanBuffer AtmosphereBuffer;

      vk::DescriptorSetLayout ImageDescriptorSetLayout;
    } m_RendererData = {};

    struct RendererResources {
      Ref<VulkanImage> CubeMap = nullptr;
      VulkanImage LutBRDF;
      VulkanImage IrradianceCube;
      VulkanImage PrefilteredCube;
      VulkanImage DirectShadowsDepthArray;
    } m_Resources;

    struct Pipelines {
      VulkanPipeline SkyboxPipeline;
      VulkanPipeline PBRPipeline;
      VulkanPipeline PostProcessPipeline;
      VulkanPipeline DepthPrePassPipeline;
      VulkanPipeline SSAOPassPipeline;
      VulkanPipeline FrustumGridPipeline;
      VulkanPipeline LightListPipeline;
      VulkanPipeline DirectShadowDepthPipeline;
      VulkanPipeline GaussianBlurPipeline;
      VulkanPipeline BloomPipeline;
      VulkanPipeline SSRPipeline;
      VulkanPipeline CompositePipeline;
      VulkanPipeline AtmospherePipeline;
      VulkanPipeline DepthOfFieldPipeline;
      VulkanPipeline DebugRenderPipeline;
    } m_Pipelines;

    struct FrameBuffers {
      VulkanFramebuffer PBRPassFB;
      VulkanFramebuffer PostProcessPassFB;
      VulkanFramebuffer DepthNormalPassFB;
      VulkanImage SSAOPassImage;
      VulkanImage SSAOBlurPassImage;
      VulkanImage SSRPassImage;
      VulkanImage CompositePassImage;
      VulkanImage BloomUpsampleImage;
      VulkanImage BloomDownsampleImage;
      VulkanImage AtmosphereImage;
      VulkanImage DepthOfFieldImage;
      std::vector<VulkanFramebuffer> DirectionalCascadesFB;
    } m_FrameBuffers = {};

    // Descriptor Sets
    VulkanDescriptorSet m_PostProcessDescriptorSet;
    VulkanDescriptorSet m_SkyboxDescriptorSet;
    VulkanDescriptorSet m_LightListDescriptorSet;
    VulkanDescriptorSet m_SSAODescriptorSet;
    VulkanDescriptorSet m_SSAOBlurDescriptorSet;
    VulkanDescriptorSet m_SSRDescriptorSet;
    VulkanDescriptorSet m_DepthDescriptorSet;
    VulkanDescriptorSet m_ShadowDepthDescriptorSet;
    VulkanDescriptorSet m_BloomDescriptorSet;
    VulkanDescriptorSet m_CompositeDescriptorSet;
    VulkanDescriptorSet m_AtmosphereDescriptorSet;
    VulkanDescriptorSet m_DepthOfFieldDescriptorSet;

    Scene* m_Scene = nullptr;

    bool m_ForceUpdateMaterials = false;

    // Mesh
    struct MeshData {
      Mesh& MeshGeometry;
      std::vector<Ref<Material>>& Materials;
      Mat4 Transform;
      uint32_t SubmeshIndex = 0;

      MeshData(Mesh& mesh,
               const Mat4& transform,
               std::vector<Ref<Material>>& materials,
               const uint32_t submeshIndex) : MeshGeometry(mesh), Materials(materials), Transform(transform),
                                              SubmeshIndex(submeshIndex) {}
    };

    std::vector<MeshData> m_MeshDrawList;

    void RenderNode(const Mesh::Node* node,
                    const vk::CommandBuffer& commandBuffer,
                    const VulkanPipeline& pipeline,
                    const std::function<bool(Mesh::Primitive* prim)>& perMeshFunc);
    void RenderMesh(const MeshData& mesh,
                    const vk::CommandBuffer& commandBuffer,
                    const VulkanPipeline& pipeline,
                    const std::function<bool(Mesh::Primitive* prim)>& perMeshFunc);

    // Lighting
    struct LightingData {
      Vec4 PositionAndIntensity;
      Vec4 ColorAndRadius;
      Vec4 Rotation;
    };

    struct LightChangeEvent {};

    Mesh m_SkyboxCube = {};
    std::vector<Entity> m_SceneLights = {};
    std::vector<LightingData> m_PointLightsData = {};
    EventDispatcher m_LightBufferDispatcher;

    void UpdateSkybox(const SceneRenderer::SkyboxLoadEvent& e);
    void UpdateCascades(const Entity& dirLightEntity, Camera* camera, RendererData::DirectShadowUB& cascadesUbo) const;
    void UpdateLightingData();
    void GeneratePrefilter();

    void CreateGraphicsPipelines();
    void CreateFramebuffers();

    void UpdateUniformBuffers();

    void UpdateProbes();
  };
}
