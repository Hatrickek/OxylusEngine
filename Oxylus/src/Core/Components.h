#pragma once
#define GLM_ENABLE_EXPERIMENTAL

#include <string>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

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
    Vec3 Translation = Vec3(0);
    Vec3 Rotation = Vec3(0); // Stored in radians
    Vec3 Scale = Vec3(1);

    TransformComponent() = default;

    TransformComponent(const TransformComponent&) = default;

    TransformComponent(const Vec3& translation) : Translation(translation) { }
  };

  // Rendering
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
    Vec3 Color = Vec3(1.0f);
    float Intensity = 1.0f;

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

  // Physics
  struct RigidbodyComponent {
    enum class BodyType { Static = 0, Kinematic, Dynamic };

    BodyType Type = BodyType::Dynamic;
    float Mass = 1.0f;
    float LinearDrag = 0.0f;
    float AngularDrag = 0.05f;
    float GravityScale = 1.0f;
    bool AllowSleep = true;
    bool Awake = true;
    bool Continuous = false;
    bool Interpolation = true;

    bool IsSensor = false;

    // Stored as JPH::BodyID
    void* RuntimeBody = nullptr;

    // For interpolation/extrapolation
    Vec3 PreviousTranslation = Vec3(0.0f);
    glm::quat PreviousRotation = Vec3(0.0f);
    Vec3 Translation = Vec3(0.0f);
    glm::quat Rotation = Vec3(0.0f);
  };

  struct BoxColliderComponent {
    Vec3 Size = {0.5f, 0.5f, 0.5f};
    Vec3 Offset = {0.0f, 0.0f, 0.0f};
    float Density = 1.0f;

    float Friction = 0.5f;
    float Restitution = 0.0f;
  };

  struct SphereColliderComponent {
    float Radius = 0.5f;
    Vec3 Offset = {0.0f, 0.0f, 0.0f};
    float Density = 1.0f;

    float Friction = 0.5f;
    float Restitution = 0.0f;
  };

  struct CapsuleColliderComponent {
    float Height = 1.0f;
    float Radius = 0.5f;
    Vec3 Offset = {0.0f, 0.0f, 0.0f};
    float Density = 1.0f;

    float Friction = 0.5f;
    float Restitution = 0.0f;
  };

  struct TaperedCapsuleColliderComponent {
    float Height = 1.0f;
    float TopRadius = 0.5f;
    float BottomRadius = 0.5f;
    Vec3 Offset = {0.0f, 0.0f, 0.0f};
    float Density = 1.0f;

    float Friction = 0.5f;
    float Restitution = 0.0f;
  };

  struct CylinderColliderComponent {
    float Height = 1.0f;
    float Radius = 0.5f;
    Vec3 Offset = {0.0f, 0.0f, 0.0f};
    float Density = 1.0f;

    float Friction = 0.5f;
    float Restitution = 0.0f;
  };

  struct CharacterControllerComponent {
    JPH::Ref<JPH::Character> Character = nullptr;

    // Size
    float CharacterHeightStanding = 1.35f;
    float CharacterRadiusStanding = 0.3f;
    float CharacterHeightCrouching = 0.8f;
    float CharacterRadiusCrouching = 0.3f;

    // Movement
    bool ControlMovementDuringJump = true;
    float CharacterSpeed = 6.0f;
    float JumpSpeed = 6.0f;

    float Friction = 0.5f;
    float CollisionTolerance = 0.05f;

    CharacterControllerComponent() = default;
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

  template <typename... Component>
  struct ComponentGroup { };

  using AllComponents = ComponentGroup<TransformComponent, RelationshipComponent, PrefabComponent, CameraComponent,

                                       // Render
                                       LightComponent, MeshRendererComponent, SkyLightComponent, ParticleSystemComponent, MaterialComponent,

                                       //  Physics
                                       RigidbodyComponent,
                                       BoxColliderComponent,
                                       SphereColliderComponent,
                                       CapsuleColliderComponent,
                                       TaperedCapsuleColliderComponent,
                                       CylinderColliderComponent,

                                       // Audio
                                       AudioSourceComponent, AudioListenerComponent>;
}
