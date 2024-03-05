#pragma once
#include <string>

#include "Core/Types.h"
#include "Core/UUID.h"

#include "Audio/AudioListener.h"
#include "Audio/AudioSource.h"

#include "Render/Camera.h"
#include "Render/Mesh.h"
#include "Render/ParticleSystem.h"

#include "Scripting/LuaSystem.h"

namespace JPH {
class Character;
}

namespace Ox {
class TextureAsset;
class Material;

struct IDComponent {
  UUID uuid;

  IDComponent() = default;

  IDComponent(const IDComponent&) = default;

  IDComponent(UUID id) : uuid(id) {}
};

struct TagComponent {
  std::string tag;
  uint16_t layer = BIT(1);
  bool enabled = true;

  // non-serialized data
  bool handled = true;

  TagComponent() = default;

  TagComponent(std::string tag) : tag(std::move(tag)) {}
};

struct RelationshipComponent {
  UUID parent = 0;
  std::vector<UUID> children{};
};

struct PrefabComponent {
  UUID id;
};

struct TransformComponent {
  Vec3 position = Vec3(0);
  Vec3 rotation = Vec3(0); // Stored in radians
  Vec3 scale = Vec3(1);

  TransformComponent() = default;
  TransformComponent(const TransformComponent&) = default;
  TransformComponent(const Vec3& translation) : position(translation) {}

  TransformComponent(const Mat4& transform_matrix) {
    OX_SCOPED_ZONE;
    Math::decompose_transform(transform_matrix, position, rotation, scale);
  }

  void set_from_matrix(const Mat4& transform_matrix) {
    OX_SCOPED_ZONE;
    Math::decompose_transform(transform_matrix, position, rotation, scale);
  }
};

// Rendering
struct MeshComponent {
  Shared<Mesh> mesh_base = nullptr;
  uint32_t node_index = 0;
  bool cast_shadows = true;

  // non-serialized data
  uint32_t mesh_id = Asset::INVALID_ID;
  std::vector<Shared<Material>> materials = {}; // node materials
  Mat4 transform = {};
  AABB aabb = {};

  MeshComponent() = default;

  MeshComponent(const Shared<Mesh>& mesh, const uint32_t node_idx = 0)
    : mesh_base(mesh), node_index(node_idx) {
    materials = mesh->get_materials(node_index);
  }
};

struct CameraComponent {
  Camera camera;
};

struct AnimationComponent {
  float animation_timer = 0.0;
  float animation_speed = 1.0f;
  uint32_t current_animation_index = 0;
  std::vector<Shared<Mesh::Animation>> animations;
};

struct ParticleSystemComponent {
  Shared<ParticleSystem> system = nullptr;

  ParticleSystemComponent() : system(create_shared<ParticleSystem>()) {}
};

struct SkyLightComponent {
  Shared<TextureAsset> cubemap = nullptr;
  float intensity = 0.7f;
  float rotation = 0.0f;
  float lod_bias = 0.0f;
};

struct LightComponent {
  enum class LightType { Directional = 0, Point, Spot };

  enum class ShadowQualityType { Hard = 0, Soft, UltraSoft };

  LightType type = LightType::Point;
  bool color_temperature_mode = false;
  uint32_t temperature = 6570;
  Vec3 color = Vec3(1.0f);
  float intensity = 1.0f;

  float range = 1.0f;
  float cut_off_angle = glm::radians(12.5f);
  float outer_cut_off_angle = glm::radians(17.5f);

  bool cast_shadows = true;
  ShadowQualityType shadow_quality = ShadowQualityType::UltraSoft;
};

struct PostProcessProbe {
  bool vignette_enabled = false;
  float vignette_intensity = 0.25f;

  bool film_grain_enabled = false;
  float film_grain_intensity = 0.2f;

  bool chromatic_aberration_enabled = false;
  float chromatic_aberration_intensity = 0.5f;

  bool sharpen_enabled = false;
  float sharpen_intensity = 0.5f;
};

// Physics
struct RigidbodyComponent {
  enum class BodyType { Static = 0, Kinematic, Dynamic };

  BodyType type = BodyType::Dynamic;
  float mass = 1.0f;
  float linear_drag = 0.0f;
  float angular_drag = 0.05f;
  float gravity_scale = 1.0f;
  bool allow_sleep = true;
  bool awake = true;
  bool continuous = false;
  bool interpolation = false;

  bool is_sensor = false;

  // Stored as JPH::Body
  void* runtime_body = nullptr;

  // For interpolation/extrapolation
  Vec3 previous_translation = Vec3(0.0f);
  glm::quat previous_rotation = Vec3(0.0f);
  Vec3 translation = Vec3(0.0f);
  glm::quat rotation = Vec3(0.0f);
};

struct BoxColliderComponent {
  Vec3 size = {0.5f, 0.5f, 0.5f};
  Vec3 offset = {0.0f, 0.0f, 0.0f};
  float density = 1.0f;

  float friction = 0.5f;
  float restitution = 0.0f;
};

struct SphereColliderComponent {
  float radius = 0.5f;
  Vec3 offset = {0.0f, 0.0f, 0.0f};
  float density = 1.0f;

  float friction = 0.5f;
  float restitution = 0.0f;
};

struct CapsuleColliderComponent {
  float height = 1.0f;
  float radius = 0.5f;
  Vec3 offset = {0.0f, 0.0f, 0.0f};
  float density = 1.0f;

  float friction = 0.5f;
  float restitution = 0.0f;
};

struct TaperedCapsuleColliderComponent {
  float height = 1.0f;
  float top_radius = 0.5f;
  float bottom_radius = 0.5f;
  Vec3 offset = {0.0f, 0.0f, 0.0f};
  float density = 1.0f;

  float friction = 0.5f;
  float restitution = 0.0f;
};

struct CylinderColliderComponent {
  float height = 1.0f;
  float radius = 0.5f;
  Vec3 offset = {0.0f, 0.0f, 0.0f};
  float density = 1.0f;

  float friction = 0.5f;
  float restitution = 0.0f;
};

struct MeshColliderComponent {
  Vec3 offset = {0.0f, 0.0f, 0.0f};
  float friction = 0.5f;
  float restitution = 0.0f;
};

struct CharacterControllerComponent {
  Shared<JPH::Character> character = nullptr;

  // Size
  float character_height_standing = 1.35f;
  float character_radius_standing = 0.3f;
  float character_height_crouching = 0.8f;
  float character_radius_crouching = 0.3f;

  // Movement
  struct MovementSettings {
    float max_speed;
    float acceleration;
    float deceleration;

    MovementSettings(const float maxSpeed, float accel, float decel)
      : max_speed(maxSpeed), acceleration(accel), deceleration(decel) {}
  };

  bool interpolation = true;

  bool control_movement_during_jump = true;
  float jump_force = 8.0f;
  bool auto_bunny_hop = true;
  float air_control = 0.3f;
  MovementSettings ground_settings = MovementSettings(7, 14, 10);
  MovementSettings air_settings = MovementSettings(7, 2, 2);
  MovementSettings strafe_settings = MovementSettings(0.0f, 50, 50);

  float friction = 6.0f;
  float gravity = 20;
  float collision_tolerance = 0.05f;

  // For interpolation/extrapolation
  Vec3 previous_translation = Vec3(0.0f);
  Quat previous_rotation = Vec3(0.0f);
  Vec3 translation = Vec3(0.0f);
  Quat rotation = Vec3(0.0f);

  CharacterControllerComponent() = default;
};

//Audio
struct AudioSourceComponent {
  AudioSourceConfig config;

  Shared<AudioSource> source = nullptr;
};

struct AudioListenerComponent {
  bool active = true;
  AudioListenerConfig config;

  Shared<AudioListener> listener;
};

// Scripting
struct LuaScriptComponent {
  std::vector<Shared<LuaSystem>> lua_systems = {};
};

template <typename... Component>
struct ComponentGroup {};

using AllComponents = ComponentGroup<TransformComponent, RelationshipComponent, PrefabComponent, CameraComponent,

                                     // Render
                                     LightComponent, MeshComponent, SkyLightComponent, ParticleSystemComponent,

                                     //  Physics
                                     RigidbodyComponent,
                                     BoxColliderComponent,
                                     SphereColliderComponent,
                                     CapsuleColliderComponent,
                                     TaperedCapsuleColliderComponent,
                                     CylinderColliderComponent,
                                     MeshColliderComponent,

                                     // Audio
                                     AudioSourceComponent, AudioListenerComponent,

                                     // Scripting
                                     LuaScriptComponent>;
}
