#pragma once
#define GLM_ENABLE_EXPERIMENTAL

#include <string>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include "Types.h"
#include "UUID.h"
#include "Audio/AudioListener.h"
#include "Audio/AudioSource.h"
#include "Physics/Physics.h"
#include "Render/Camera.h"
#include "Render/Mesh.h"
#include "Render/ParticleSystem.h"
#include "Render/Window.h"

namespace Oxylus {
  struct IDComponent {
    UUID ID;

    IDComponent() = default;

    IDComponent(const IDComponent&) = default;

    IDComponent(UUID id) : ID(id) { }
  };

  struct TagComponent {
    std::string Tag;
    uint16_t Layer = BIT(1);
    bool Enabled = true;

    bool handled = true;

    TagComponent() = default;

    explicit TagComponent(std::string tag) : Tag(std::move(tag)) { }
  };

  struct RelationshipComponent {
    UUID Parent = 0;
    std::vector<UUID> Children{};
  };

  struct PrefabComponent {
    UUID ID;
  };

  struct TransformComponent {
    glm::vec3 Translation = glm::vec3(0);
    glm::vec3 Rotation = glm::vec3(0); //Stored in radians
    glm::vec3 Scale = glm::vec3(1);

    TransformComponent() = default;

    TransformComponent(const TransformComponent&) = default;

    TransformComponent(const glm::vec3& translation) : Translation(translation) { }

    glm::mat4 GetTransform() const {
      const glm::mat4 rotation = glm::toMat4(glm::quat(Rotation));

      return glm::translate(glm::mat4(1.0f), Translation) * rotation * glm::scale(glm::mat4(1.0f), Scale);
    }
  };

  //Rendering
  struct MaterialComponent {
    std::vector<Ref<Material>> Materials{};
    bool UsingMaterialAsset = false;
  };

  struct CameraComponent {
    Ref<Camera> System;

    CameraComponent() : System(CreateRef<Camera>()) { };
  };

  struct MeshRendererComponent {
    Ref<Mesh> MeshGeometry = nullptr;
    uint32_t SubmesIndex = 0;

    MeshRendererComponent() = default;

    MeshRendererComponent(const Ref<Mesh>& model) : MeshGeometry(model) { }

    ~MeshRendererComponent() {
      MeshGeometry.reset();
    }
  };

  struct ParticleSystemComponent {
    Ref<ParticleSystem> System = nullptr;

    ParticleSystemComponent() : System(CreateRef<ParticleSystem>()) { }
  };

  struct SkyLightComponent {
    Ref<VulkanImage> Cubemap = nullptr;
    float CubemapLodBias;
    float Intensity = 0.7f;
    float Rotation = 0.0f;
    bool FlipImage = false;
  };

  struct LightComponent {
    enum class LightType { Directional = 0, Point, Spot };

    enum class ShadowQualityType { Hard = 0, Soft, UltraSoft };

    LightType Type = LightType::Point;
    bool UseColorTemperatureMode = false;
    uint32_t Temperature = 6570;
    glm::vec3 Color = glm::vec3(1.0f);
    float Intensity = 20.0f;

    float Range = 1.0f;
    float CutOffAngle = glm::radians(12.5f);
    float OuterCutOffAngle = glm::radians(17.5f);

    ShadowQualityType ShadowQuality = ShadowQualityType::UltraSoft;
  };

  struct PostProcessProbe {
    bool VignetteEnabled = false;
    float VignetteIntensity = 0.25f;

    bool FilmGrainEnabled = false;
    float FilmGrainIntensity = 0.2f;

    bool ChromaticAberrationEnabled = false;
    float ChromaticAberrationIntensity = 0.5f;

    bool SharpenEnabled = false;
    float SharpenIntensity = 0.5f;
  };

  //Physics
  struct BoxColliderComponent {
    Vec3 Size = Vec3(1.0f);
  };

  struct MeshColliderComponent {
    Vec3 Size = Vec3(1.0f);
  };

  struct RigidBodyComponent {
    JPH::BodyID BodyID;

    RigidBodyComponent() { }

    ~RigidBodyComponent() { }
  };

  //Audio
  struct AudioSourceComponent {
    AudioSourceConfig Config;

    Ref<AudioSource> Source = nullptr;
  };

  struct AudioListenerComponent {
    bool Active = true;
    AudioListenerConfig Config;

    Ref<AudioListener> Listener;
  };

  template<typename... Component>
  struct ComponentGroup { };

  using AllComponents = ComponentGroup<TransformComponent, RelationshipComponent, PrefabComponent, CameraComponent,

          //Render
          LightComponent, MeshRendererComponent, SkyLightComponent, ParticleSystemComponent, MaterialComponent,

          //Physics
          RigidBodyComponent, BoxColliderComponent, MeshColliderComponent,

          //Audio
          AudioSourceComponent, AudioListenerComponent>;
}
