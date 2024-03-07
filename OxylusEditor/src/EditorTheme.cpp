#include "EditorTheme.h"

#include <icons/IconsMaterialDesignIcons.h>

#include "Scene/Components.h"

namespace ox {
ankerl::unordered_dense::map<size_t, const char8_t*> EditorTheme::component_icon_map = {};

void EditorTheme::init() {
  component_icon_map[typeid(RelationshipComponent).hash_code()] = ICON_MDI_LIBRARY_SHELVES;
  component_icon_map[typeid(LightComponent).hash_code()] = ICON_MDI_LIGHTBULB;
  component_icon_map[typeid(Camera).hash_code()] = ICON_MDI_CAMERA;
  component_icon_map[typeid(AudioSourceComponent).hash_code()] = ICON_MDI_VOLUME_HIGH;
  component_icon_map[typeid(TransformComponent).hash_code()] = ICON_MDI_VECTOR_LINE;
  component_icon_map[typeid(MeshComponent).hash_code()] = ICON_MDI_VECTOR_SQUARE;
  component_icon_map[typeid(AnimationComponent).hash_code()] = ICON_MDI_ANIMATION;
  component_icon_map[typeid(SkyLightComponent).hash_code()] = ICON_MDI_WEATHER_SUNNY;
  component_icon_map[typeid(LuaScriptComponent).hash_code()] = ICON_MDI_LANGUAGE_LUA;
  component_icon_map[typeid(PostProcessProbe).hash_code()] = ICON_MDI_SPRAY;
  component_icon_map[typeid(AudioListenerComponent).hash_code()] = ICON_MDI_CIRCLE_SLICE_8;
  component_icon_map[typeid(RigidbodyComponent).hash_code()] = ICON_MDI_SOCCER;
  component_icon_map[typeid(BoxColliderComponent).hash_code()] = ICON_MDI_CHECKBOX_BLANK_OUTLINE;
  component_icon_map[typeid(SphereColliderComponent).hash_code()] = ICON_MDI_CIRCLE_OUTLINE;
  component_icon_map[typeid(CapsuleColliderComponent).hash_code()] = ICON_MDI_CIRCLE_OUTLINE;
  component_icon_map[typeid(TaperedCapsuleColliderComponent).hash_code()] = ICON_MDI_CIRCLE_OUTLINE;
  component_icon_map[typeid(CylinderColliderComponent).hash_code()] = ICON_MDI_CIRCLE_OUTLINE;
  component_icon_map[typeid(MeshColliderComponent).hash_code()] = ICON_MDI_CHECKBOX_BLANK_OUTLINE;
  component_icon_map[typeid(CharacterControllerComponent).hash_code()] = ICON_MDI_CIRCLE_OUTLINE;
  component_icon_map[typeid(CameraComponent).hash_code()] = ICON_MDI_CAMERA;
  component_icon_map[typeid(ParticleSystemComponent).hash_code()] = ICON_MDI_LAMP;
}
}
