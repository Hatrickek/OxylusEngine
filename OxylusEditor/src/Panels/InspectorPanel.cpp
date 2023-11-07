#include "InspectorPanel.h"

#include <icons/IconsMaterialDesignIcons.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <fmt/format.h>
#include <misc/cpp/imgui_stdlib.h>

#include <Assets/AssetManager.h>

#include "Utils/ColorUtils.h"

#include "EditorLayer.h"
#include "Core/Entity.h"
#include "UI/OxUI.h"
#include "Utils/StringUtils.h"
#include "Utils/UIUtils.h"
#include "Assets/MaterialSerializer.h"

#include "Render/SceneRendererEvents.h"

namespace Oxylus {
static bool s_RenameEntity = false;

InspectorPanel::InspectorPanel() : EditorPanel("Inspector", ICON_MDI_INFORMATION, true) { }

void InspectorPanel::on_imgui_render() {
  m_SelectedEntity = EditorLayer::get()->get_selected_entity();
  m_Scene = EditorLayer::get()->get_selected_scene().get();

  on_begin();
  if (m_SelectedEntity) {
    draw_components(m_SelectedEntity);
  }

  //OxUI::draw_gradient_shadow_bottom();

  on_end();
}

template <typename T, typename UIFunction>
static void DrawComponent(const char8_t* name,
                          Entity entity,
                          UIFunction uiFunction,
                          const bool removable = true) {
  if (entity.has_component<T>()) {
    static constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_DefaultOpen
                                                    | ImGuiTreeNodeFlags_SpanAvailWidth |
                                                    ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_Framed |
                                                    ImGuiTreeNodeFlags_FramePadding;

    auto& component = entity.get_component<T>();

    const float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + lineHeight * 0.25f);

    const size_t id = entt::type_id<T>().hash();
    const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(id), treeFlags, "%s", name);

    bool removeComponent = false;
    if (removable) {
      ImGui::PushID((int)id);

      const float frameHeight = ImGui::GetFrameHeight();
      ImGui::SameLine(ImGui::GetContentRegionMax().x - frameHeight * 1.2f);
      if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_SETTINGS), ImVec2{frameHeight * 1.2f, frameHeight}))
        ImGui::OpenPopup("ComponentSettings");

      if (ImGui::BeginPopup("ComponentSettings")) {
        if (ImGui::MenuItem("Remove Component"))
          removeComponent = true;

        ImGui::EndPopup();
      }

      ImGui::PopID();
    }

    if (open) {
      uiFunction(component);
      ImGui::TreePop();
    }

    if (removeComponent)
      entity.remove_component<T>();
  }
}

bool InspectorPanel::draw_material_properties(Ref<Material>& material, bool saveToCurrentPath) {
  bool loadAsset = false;

  if (ImGui::Button("Save")) {
    std::string path;
    if (saveToCurrentPath)
      path = FileDialogs::save_file({{"Material file", "oxmat"}}, "NewMaterial");
    else
      path = material->path;
    MaterialSerializer(*material).Serialize(path);
  }
  ImGui::SameLine();
  if (ImGui::Button("Load")) {
    const auto& path = FileDialogs::open_file({{"Material file", "oxmat"}});
    if (!path.empty()) {
      MaterialSerializer(material).Deserialize(path);
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset")) {
    material = create_ref<Material>();
    material->create();
  }
  ImGui::SameLine();
  const float x = ImGui::GetContentRegionAvail().x;
  const float y = ImGui::GetFrameHeight();
  ImGui::Button("Drop a material file", {x, y});
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
      std::filesystem::path path = OxUI::get_path_from_imgui_payload(payload);
      const std::string ext = path.extension().string();
      if (ext == ".oxmat") {
        MaterialSerializer(material).Deserialize(path.string());
        loadAsset = true;
      }
    }
    ImGui::EndDragDropTarget();
  }

  OxUI::begin_properties();
  OxUI::text("Alpha mode: ", material->alpha_mode_to_string());
  OxUI::property("UV Scale", &material->parameters.uv_scale);
  OxUI::property("Use Albedo", (bool*)&material->parameters.use_albedo);
  OxUI::property("Albedo", material->albedo_texture);
  OxUI::property_vector("Color", material->parameters.color, true, true);

  OxUI::property("Specular", &material->parameters.specular);
  OxUI::property_vector("Emmisive", material->parameters.emmisive, true, true);

  OxUI::property("Use Normal", (bool*)&material->parameters.use_normal);
  OxUI::property("Normal", material->normal_texture);

  OxUI::property("Use PhysicalMap(Roughness/Metallic)", (bool*)&material->parameters.use_physical_map);
  OxUI::property("PhysicalMap", material->metallic_roughness_texture);

  OxUI::property("Roughness", &material->parameters.roughness);

  OxUI::property("Metallic", &material->parameters.metallic);

  OxUI::property("Use AO", (bool*)&material->parameters.use_ao);
  OxUI::property("AO", material->ao_texture);

  OxUI::end_properties();

  return loadAsset;
}

template <typename T>
static void DrawParticleOverLifetimeModule(std::string_view moduleName,
                                           OverLifetimeModule<T>& propertyModule,
                                           bool color = false,
                                           bool rotation = false) {
  static constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_DefaultOpen
                                                  | ImGuiTreeNodeFlags_SpanAvailWidth |
                                                  ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_Framed |
                                                  ImGuiTreeNodeFlags_FramePadding;

  if (ImGui::TreeNodeEx(moduleName.data(), treeFlags, "%s", moduleName.data())) {
    OxUI::begin_properties();
    OxUI::property("Enabled", &propertyModule.enabled);

    if (rotation) {
      T degrees = glm::degrees(propertyModule.start);
      if (OxUI::property_vector("Start", degrees))
        propertyModule.start = glm::radians(degrees);

      degrees = glm::degrees(propertyModule.end);
      if (OxUI::property_vector("End", degrees))
        propertyModule.end = glm::radians(degrees);
    }
    else {
      OxUI::property_vector("Start", propertyModule.start, color);
      OxUI::property_vector("End", propertyModule.end, color);
    }

    OxUI::end_properties();

    ImGui::TreePop();
  }
}

template <typename T>
static void DrawParticleBySpeedModule(std::string_view moduleName,
                                      BySpeedModule<T>& propertyModule,
                                      bool color = false,
                                      bool rotation = false) {
  static constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_DefaultOpen
                                                  | ImGuiTreeNodeFlags_SpanAvailWidth |
                                                  ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_Framed |
                                                  ImGuiTreeNodeFlags_FramePadding;

  if (ImGui::TreeNodeEx(moduleName.data(), treeFlags, "%s", moduleName.data())) {
    OxUI::begin_properties();
    OxUI::property("Enabled", &propertyModule.enabled);

    if (rotation) {
      T degrees = glm::degrees(propertyModule.start);
      if (OxUI::property_vector("Start", degrees))
        propertyModule.start = glm::radians(degrees);

      degrees = glm::degrees(propertyModule.end);
      if (OxUI::property_vector("End", degrees))
        propertyModule.end = glm::radians(degrees);
    }
    else {
      OxUI::property_vector("Start", propertyModule.start, color);
      OxUI::property_vector("End", propertyModule.end, color);
    }

    OxUI::property("Min Speed", &propertyModule.min_speed);
    OxUI::property("Max Speed", &propertyModule.max_speed);
    OxUI::end_properties();
    ImGui::TreePop();
  }
}

template <typename Component>
void InspectorPanel::draw_add_component(Entity entity, const char* name) const {
  if (ImGui::MenuItem(name)) {
    if (!entity.has_component<Component>())
      entity.add_component_internal<Component>();
    else
      OX_CORE_ERROR("Entity already has the {}!", typeid(Component).name());
    ImGui::CloseCurrentPopup();
  }
}

void InspectorPanel::draw_components(Entity entity) const {
  if (entity.has_component<TagComponent>()) {
    auto& tag = entity.get_component<TagComponent>().tag;
    char buffer[256] = {};
    tag.copy(buffer, sizeof buffer);
    if (s_RenameEntity)
      ImGui::SetKeyboardFocusHere();
    if (ImGui::InputText("##Tag", buffer, sizeof(buffer), ImGuiInputTextFlags_EnterReturnsTrue)) {
      tag = std::string(buffer);
    }
  }
  ImGui::SameLine();
  ImGui::PushItemWidth(-1);

  if (ImGui::Button("Add Component")) {
    ImGui::OpenPopup("Add Component");
  }
  if (ImGui::BeginPopup("Add Component")) {
    draw_add_component<MeshRendererComponent>(m_SelectedEntity, "Mesh Renderer");
    draw_add_component<MaterialComponent>(m_SelectedEntity, "Material");
    draw_add_component<AudioSourceComponent>(m_SelectedEntity, "Audio Source");
    draw_add_component<LightComponent>(m_SelectedEntity, "Light");
    draw_add_component<ParticleSystemComponent>(m_SelectedEntity, "Particle System");
    draw_add_component<CameraComponent>(m_SelectedEntity, "Camera");
    draw_add_component<PostProcessProbe>(m_SelectedEntity, "PostProcess Probe");
    draw_add_component<RigidbodyComponent>(m_SelectedEntity, "Rigidbody");
    draw_add_component<BoxColliderComponent>(entity, "Box Collider");
    draw_add_component<SphereColliderComponent>(entity, "Sphere Collider");
    draw_add_component<CapsuleColliderComponent>(entity, "Capsule Collider");
    draw_add_component<TaperedCapsuleColliderComponent>(entity, "Tapered Capsule Collider");
    draw_add_component<CylinderColliderComponent>(entity, "Cylinder Collider");
    draw_add_component<CharacterControllerComponent>(entity, "Character Controller");
    draw_add_component<CustomComponent>(entity, "Custom Component");
    draw_add_component<LuaScriptComponent>(entity, "Lua Script Component");

    ImGui::EndPopup();
  }
  ImGui::PopItemWidth();

  DrawComponent<TransformComponent>(ICON_MDI_VECTOR_LINE " Transform Component",
    entity,
    [](TransformComponent& component) {
      OxUI::begin_properties();
      OxUI::draw_vec3_control("Translation", component.position);
      Vec3 rotation = glm::degrees(component.rotation);
      OxUI::draw_vec3_control("Rotation", rotation);
      component.rotation = glm::radians(rotation);
      OxUI::draw_vec3_control("Scale", component.scale, nullptr, 1.0f);
      OxUI::end_properties();
    });

  DrawComponent<MeshRendererComponent>(ICON_MDI_VECTOR_SQUARE " Mesh Renderer Component",
    entity,
    [](const MeshRendererComponent& component) {
      if (!component.mesh_geometry)
        return;
      const char* fileName = component.mesh_geometry->name.empty()
                               ? "Empty"
                               : component.mesh_geometry->name.c_str();
      ImGui::Text("Loaded Mesh: %s", fileName);
      ImGui::Text("Material Count: %d", (uint32_t)component.mesh_geometry->get_materials_as_ref().size());
    });

  DrawComponent<MaterialComponent>(ICON_MDI_SPRAY " Material Component",
    entity,
    [](MaterialComponent& component) {
      if (component.materials.empty())
        return;

      constexpr ImGuiTreeNodeFlags flags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth |
        ImGuiTreeNodeFlags_FramePadding;

      const float filter_cursor_pos_x = ImGui::GetCursorPosX();
      ImGuiTextFilter name_filter;

      name_filter.Draw("##material_filter", ImGui::GetContentRegionAvail().x - (OxUI::get_icon_button_size(ICON_MDI_PLUS, "").x + 2.0f * ImGui::GetStyle().FramePadding.x));

      if (!name_filter.IsActive()) {
        ImGui::SameLine();
        ImGui::SetCursorPosX(filter_cursor_pos_x + ImGui::GetFontSize() * 0.5f);
        ImGui::TextUnformatted(StringUtils::from_char8_t(ICON_MDI_MAGNIFY " Search..."));
      }

      for (auto& material : component.materials) {
        if (name_filter.PassFilter(material->name.c_str())) {
          if (ImGui::TreeNodeEx(material->name.c_str(),
            flags,
            "%s %s",
            StringUtils::from_char8_t(ICON_MDI_CIRCLE),
            material->name.c_str())) {
            if (draw_material_properties(material)) {
              component.using_material_asset = true;
            }
            ImGui::TreePop();
          }
        }
      }
    });

  DrawComponent<SkyLightComponent>(ICON_MDI_WEATHER_SUNNY " Sky Light Component",
    entity,
    [this](SkyLightComponent& component) {
      auto file_name = FileSystem::get_file_name(component.cubemap->get_path());
      const char* name = component.cubemap
                           ? file_name.c_str()
                           : "Drop a hdr file";
      auto loadCubeMap = [](Scene* scene, const std::string& path, SkyLightComponent& comp) {
        if (path.empty())
          return;
        const auto ext = std::filesystem::path(path).extension().string();
        if (ext == ".hdr") {
          comp.cubemap = AssetManager::get_texture_asset({.path = path, .format = vuk::Format::eR8G8B8A8Srgb});
          scene->get_renderer()->dispatcher.trigger(SkyboxLoadEvent{comp.cubemap});
        }
      };
      const float x = ImGui::GetContentRegionAvail().x;
      const float y = ImGui::GetFrameHeight();
      if (ImGui::Button(name, {x, y})) {
        const std::string filePath = FileDialogs::open_file({{"HDR File", "hdr"}});
        loadCubeMap(m_Scene, filePath, component);
      }
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
          const auto path = OxUI::get_path_from_imgui_payload(payload).string();
          loadCubeMap(m_Scene, path, component);
        }
        ImGui::EndDragDropTarget();
      }
      ImGui::Spacing();
      OxUI::begin_properties();
      OxUI::property("Lod Bias", &component.lod_bias);
      OxUI::end_properties();
    });

  DrawComponent<PostProcessProbe>(ICON_MDI_SPRAY " PostProcess Probe Component",
    entity,
    [this](PostProcessProbe& component) {
      ImGui::Text("Vignette");
      OxUI::begin_properties();
      pp_probe_property(OxUI::property("Enable", &component.vignette_enabled), component);
      pp_probe_property(OxUI::property("Intensity", &component.vignette_intensity), component);
      OxUI::end_properties();
      ImGui::Separator();

      ImGui::Text("FilmGrain");
      OxUI::begin_properties();
      pp_probe_property(OxUI::property("Enable", &component.film_grain_enabled), component);
      pp_probe_property(OxUI::property("Intensity", &component.film_grain_intensity), component);
      OxUI::end_properties();
      ImGui::Separator();

      ImGui::Text("ChromaticAberration");
      OxUI::begin_properties();
      pp_probe_property(OxUI::property("Enable", &component.chromatic_aberration_enabled), component);
      pp_probe_property(OxUI::property("Intensity", &component.chromatic_aberration_intensity), component);
      OxUI::end_properties();
      ImGui::Separator();

      ImGui::Text("Sharpen");
      OxUI::begin_properties();
      pp_probe_property(OxUI::property("Enable", &component.sharpen_enabled), component);
      pp_probe_property(OxUI::property("Intensity", &component.sharpen_intensity), component);
      OxUI::end_properties();
      ImGui::Separator();
    });

  DrawComponent<AudioSourceComponent>(ICON_MDI_VOLUME_MEDIUM " Audio Source Component",
    entity,
    [&entity](AudioSourceComponent& component) {
      auto& config = component.config;

      const char* filepath = component.source
                               ? component.source->GetPath()
                               : "Drop an audio file";
      const float x = ImGui::GetContentRegionAvail().x;
      const float y = ImGui::GetFrameHeight();
      ImGui::Button(filepath, {x, y});
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
          const std::filesystem::path path = OxUI::get_path_from_imgui_payload(payload);
          const std::string ext = path.extension().string();
          if (ext == ".mp3" || ext == ".wav")
            component.source = create_ref<AudioSource>(AssetManager::get_asset_file_system_path(path).string().c_str());
        }
        ImGui::EndDragDropTarget();
      }
      ImGui::Spacing();

      OxUI::begin_properties();
      OxUI::property("Volume Multiplier", &config.VolumeMultiplier);
      OxUI::property("Pitch Multiplier", &config.PitchMultiplier);
      OxUI::property("Play On Awake", &config.PlayOnAwake);
      OxUI::property("Looping", &config.Looping);
      OxUI::end_properties();

      ImGui::Spacing();
      if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_PLAY "Play ")) &&
          component.source)
        component.source->Play();
      ImGui::SameLine();
      if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_PAUSE "Pause ")) &&
          component.source)
        component.source->Pause();
      ImGui::SameLine();
      if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_STOP "Stop ")) &&
          component.source)
        component.source->Stop();
      ImGui::Spacing();

      OxUI::begin_properties();
      OxUI::property("Spatialization", &config.Spatialization);

      if (config.Spatialization) {
        ImGui::Indent();
        const char* attenuationTypeStrings[] = {
          "None",
          "Inverse",
          "Linear",
          "Exponential"
        };
        int attenuationType = static_cast<int>(config.AttenuationModel);
        if (OxUI::property("Attenuation Model", &attenuationType, attenuationTypeStrings, 4))
          config.AttenuationModel = static_cast<AttenuationModelType>(attenuationType);
        OxUI::property("Roll Off", &config.RollOff);
        OxUI::property("Min Gain", &config.MinGain);
        OxUI::property("Max Gain", &config.MaxGain);
        OxUI::property("Min Distance", &config.MinDistance);
        OxUI::property("Max Distance", &config.MaxDistance);
        float degrees = glm::degrees(config.ConeInnerAngle);
        if (OxUI::property("Cone Inner Angle", &degrees))
          config.ConeInnerAngle = glm::radians(degrees);
        degrees = glm::degrees(config.ConeOuterAngle);
        if (OxUI::property("Cone Outer Angle", &degrees))
          config.ConeOuterAngle = glm::radians(degrees);
        OxUI::property("Cone Outer Gain", &config.ConeOuterGain);
        OxUI::property("Doppler Factor", &config.DopplerFactor);
        ImGui::Unindent();
      }
      OxUI::end_properties();

      if (component.source) {
        const glm::mat4 inverted = glm::inverse(entity.get_world_transform());
        const Vec3 forward = normalize(Vec3(inverted[2]));
        component.source->SetConfig(config);
        component.source->SetPosition(entity.get_transform().position);
        component.source->SetDirection(-forward);
      }
    });

  DrawComponent<AudioListenerComponent>(ICON_MDI_CIRCLE_SLICE_8 " Audio Listener Component",
    entity,
    [](AudioListenerComponent& component) {
      auto& config = component.config;
      OxUI::begin_properties();
      OxUI::property("Active", &component.active);
      float degrees = glm::degrees(config.ConeInnerAngle);
      if (OxUI::property("Cone Inner Angle", &degrees))
        config.ConeInnerAngle = glm::radians(degrees);
      degrees = glm::degrees(config.ConeOuterAngle);
      if (OxUI::property("Cone Outer Angle", &degrees))
        config.ConeOuterAngle = glm::radians(degrees);
      OxUI::property("Cone Outer Gain", &config.ConeOuterGain);
      OxUI::end_properties();
    });

  DrawComponent<LightComponent>(ICON_MDI_LAMP " Light Component",
    entity,
    [](LightComponent& component) {
      OxUI::begin_properties();
      const char* lightTypeStrings[] = {"Directional", "Point", "Spot"};
      int lightType = static_cast<int>(component.type);
      if (OxUI::property("Light Type", &lightType, lightTypeStrings, 3))
        component.type = static_cast<LightComponent::LightType>(lightType);

      if (OxUI::property("Use color temperature mode", &component.use_color_temperature_mode) && component.use_color_temperature_mode) {
        ColorUtils::TempratureToColor(component.temperature, component.color);
      }

      if (component.use_color_temperature_mode) {
        if (OxUI::property<uint32_t>("Temperature (K)",
          &component.temperature,
          1000,
          40000))
          ColorUtils::TempratureToColor(component.temperature, component.color);
      }
      else {
        OxUI::property_vector("Color", component.color, true);
      }

      if (OxUI::property("Intensity", &component.intensity) &&
          component.intensity < 0.0f) {
        component.intensity = 0.0f;
      }

      ImGui::Spacing();

      if (component.type == LightComponent::LightType::Point) {
        OxUI::property("Range", &component.range);
      }
      else if (component.type == LightComponent::LightType::Spot) {
        OxUI::property("Range", &component.range);
        float degrees = glm::degrees(component.outer_cut_off_angle);
        if (OxUI::property("Outer Cut-Off Angle", &degrees, 1.0f, 90.0f))
          component.outer_cut_off_angle = glm::radians(degrees);
        degrees = glm::degrees(component.cut_off_angle);
        if (OxUI::property("Cut-Off Angle", &degrees, 1.0f, 90.0f))
          component.cut_off_angle = glm::radians(degrees);

        if (component.range < 0.1f)
          component.range = 0.1f;
        if (component.outer_cut_off_angle < component.cut_off_angle)
          component.cut_off_angle = component.outer_cut_off_angle;
        if (component.cut_off_angle > component.outer_cut_off_angle)
          component.outer_cut_off_angle = component.cut_off_angle;
      }
      else if (component.type == LightComponent::LightType::Directional) {
        const char* shadowQualityTypeStrings[] = {"Hard", "Soft", "Ultra Soft"};
        int shadowQualityType = static_cast<int>(component.shadow_quality);

        if (OxUI::property("Shadow Quality Type", &shadowQualityType, shadowQualityTypeStrings, 3))
          component.shadow_quality = static_cast<LightComponent::ShadowQualityType>(shadowQualityType);
      }

      OxUI::end_properties();
    });

  DrawComponent<RigidbodyComponent>(ICON_MDI_SOCCER " Rigidbody Component",
    entity,
    [this](RigidbodyComponent& component) {
      OxUI::begin_properties();

      const char* bodyTypeStrings[] = {"Static", "Kinematic", "Dynamic"};
      int bodyType = static_cast<int>(component.type);
      if (OxUI::property("Body Type", &bodyType, bodyTypeStrings, 3))
        component.type = static_cast<RigidbodyComponent::BodyType>(bodyType);

      if (component.type == RigidbodyComponent::BodyType::Dynamic) {
        OxUI::property("Mass", &component.mass, 0.01f, 10000.0f);
        OxUI::property("Linear Drag", &component.linear_drag);
        OxUI::property("Angular Drag", &component.angular_drag);
        OxUI::property("Gravity Scale", &component.gravity_scale);
        OxUI::property("Allow Sleep", &component.allow_sleep);
        OxUI::property("Awake", &component.awake);
        OxUI::property("Continuous", &component.continuous);
        OxUI::property("Interpolation", &component.interpolation);

        component.linear_drag = glm::max(component.linear_drag, 0.0f);
        component.angular_drag = glm::max(component.angular_drag, 0.0f);
      }

      OxUI::property("Is Sensor", &component.is_sensor);
      OxUI::end_properties();
    });

  DrawComponent<BoxColliderComponent>(ICON_MDI_CHECKBOX_BLANK_OUTLINE " Box Collider",
    entity,
    [](BoxColliderComponent& component) {
      OxUI::begin_properties();
      OxUI::property_vector("Size", component.size);
      OxUI::property_vector("Offset", component.offset);
      OxUI::property("Density", &component.density);
      OxUI::property("Friction", &component.friction, 0.0f, 1.0f);
      OxUI::property("Restitution", &component.restitution, 0.0f, 1.0f);
      OxUI::end_properties();

      component.density = glm::max(component.density, 0.001f);
    });

  DrawComponent<SphereColliderComponent>(ICON_MDI_CIRCLE_OUTLINE " Sphere Collider",
    entity,
    [](SphereColliderComponent& component) {
      OxUI::begin_properties();
      OxUI::property("Radius", &component.radius);
      OxUI::property_vector("Offset", component.offset);
      OxUI::property("Density", &component.density);
      OxUI::property("Friction", &component.friction, 0.0f, 1.0f);
      OxUI::property("Restitution", &component.restitution, 0.0f, 1.0f);
      OxUI::end_properties();

      component.density = glm::max(component.density, 0.001f);
    });

  DrawComponent<CapsuleColliderComponent>(ICON_MDI_CIRCLE_OUTLINE " Capsule Collider",
    entity,
    [](CapsuleColliderComponent& component) {
      OxUI::begin_properties();
      OxUI::property("Height", &component.height);
      OxUI::property("Radius", &component.radius);
      OxUI::property_vector("Offset", component.offset);
      OxUI::property("Density", &component.density);
      OxUI::property("Friction", &component.friction, 0.0f, 1.0f);
      OxUI::property("Restitution", &component.restitution, 0.0f, 1.0f);
      OxUI::end_properties();

      component.density = glm::max(component.density, 0.001f);
    });

  DrawComponent<TaperedCapsuleColliderComponent>(ICON_MDI_CIRCLE_OUTLINE " Tapered Capsule Collider",
    entity,
    [](TaperedCapsuleColliderComponent& component) {
      OxUI::begin_properties();
      OxUI::property("Height", &component.height);
      OxUI::property("Top Radius", &component.top_radius);
      OxUI::property("Bottom Radius", &component.bottom_radius);
      OxUI::property_vector("Offset", component.offset);
      OxUI::property("Density", &component.density);
      OxUI::property("Friction", &component.friction, 0.0f, 1.0f);
      OxUI::property("Restitution", &component.restitution, 0.0f, 1.0f);
      OxUI::end_properties();

      component.density = glm::max(component.density, 0.001f);
    });

  DrawComponent<CylinderColliderComponent>(ICON_MDI_CIRCLE_OUTLINE " Cylinder Collider",
    entity,
    [](CylinderColliderComponent& component) {
      OxUI::begin_properties();
      OxUI::property("Height", &component.height);
      OxUI::property("Radius", &component.radius);
      OxUI::property_vector("Offset", component.offset);
      OxUI::property("Density", &component.density);
      OxUI::property("Friction", &component.friction, 0.0f, 1.0f);
      OxUI::property("Restitution", &component.restitution, 0.0f, 1.0f);
      OxUI::end_properties();

      component.density = glm::max(component.density, 0.001f);
    });

  DrawComponent<CharacterControllerComponent>(ICON_MDI_CIRCLE_OUTLINE " Character Controller",
    entity,
    [](CharacterControllerComponent& component) {
      OxUI::begin_properties();
      OxUI::property("CharacterHeightStanding", &component.character_height_standing);
      OxUI::property("CharacterRadiusStanding", &component.character_radius_standing);
      OxUI::property("CharacterHeightCrouching", &component.character_height_crouching);
      OxUI::property("CharacterRadiusCrouching", &component.character_radius_crouching);

      // Movement
      OxUI::property("ControlMovementDuringJump", &component.control_movement_during_jump);
      OxUI::property("JumpForce", &component.jump_force);

      OxUI::property("Friction", &component.friction, 0.0f, 1.0f);
      OxUI::property("CollisionTolerance", &component.collision_tolerance);
      OxUI::end_properties();
    });

  DrawComponent<CameraComponent>(ICON_MDI_CAMERA "Camera Component",
    entity,
    [](const CameraComponent& component) {
      OxUI::begin_properties();
      static float fov = component.system->Fov;
      if (OxUI::property("FOV", &fov)) {
        component.system->set_fov(fov);
      }
      static float nearClip = component.system->near_clip;
      if (OxUI::property("Near Clip", &nearClip)) {
        component.system->set_near(nearClip);
      }
      static float farClip = component.system->far_clip;
      if (OxUI::property("Far Clip", &farClip)) {
        component.system->set_far(farClip);
      }
      OxUI::end_properties();
    });

  DrawComponent<LuaScriptComponent>(ICON_MDI_CAMERA "Lua Script Component",
    entity,
    [](LuaScriptComponent& component) {
      const std::string name = component.lua_system
                                 ? FileSystem::get_file_name(component.lua_system->get_path())
                                 : "Drop a lua script file";
      auto load_script = [](const std::string& path, LuaScriptComponent& comp) {
        if (path.empty())
          return;
        const auto ext = FileSystem::get_file_extension(path);
        if (ext == "lua") {
          comp.lua_system = create_ref<LuaSystem>(path);
        }
      };
      const float x = ImGui::GetContentRegionAvail().x;
      const float y = ImGui::GetFrameHeight();
      if (ImGui::Button(name.c_str(), {x, y})) {
        const std::string filePath = FileDialogs::open_file({{"Lua file", "lua"}});
        load_script(filePath, component);
      }
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
          const auto path = OxUI::get_path_from_imgui_payload(payload).string();
          load_script(path, component);
        }
        ImGui::EndDragDropTarget();
      }
      if (ImGui::Button("Reload", {x, y})) {
        if (component.lua_system)
          component.lua_system->reload();
      }
    });


  DrawComponent<ParticleSystemComponent>(ICON_MDI_LAMP "Particle System Component",
    entity,
    [](const ParticleSystemComponent& component) {
      auto& props = component.system->get_properties();

      ImGui::Text("Active Particles Count: %u",
        component.system->get_active_particle_count());
      ImGui::BeginDisabled(props.looping);
      if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_PLAY)))
        component.system->play();
      ImGui::SameLine();
      if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_STOP)))
        component.system->stop();
      ImGui::EndDisabled();

      ImGui::Separator();

      OxUI::begin_properties();
      OxUI::property("Duration", &props.duration);
      if (OxUI::property("Looping", &props.looping)) {
        if (props.looping)
          component.system->play();
      }
      OxUI::property("Start Delay", &props.start_delay);
      OxUI::property("Start Lifetime", &props.start_lifetime);
      OxUI::property_vector("Start Velocity", props.start_velocity);
      OxUI::property_vector("Start Color", props.start_color, true);
      OxUI::property_vector("Start Size", props.start_size);
      OxUI::property_vector("Start Rotation", props.start_rotation);
      OxUI::property("Gravity Modifier", &props.gravity_modifier);
      OxUI::property("Simulation Speed", &props.simulation_speed);
      OxUI::property("Play On Awake", &props.play_on_awake);
      OxUI::property("Max Particles", &props.max_particles);
      OxUI::end_properties();

      ImGui::Separator();

      OxUI::begin_properties();
      OxUI::property("Rate Over Time", &props.rate_over_time);
      OxUI::property("Rate Over Distance", &props.rate_over_distance);
      OxUI::property("Burst Count", &props.burst_count);
      OxUI::property("Burst Time", &props.burst_time);
      OxUI::property_vector("Position Start", props.position_start);
      OxUI::property_vector("Position End", props.position_end);
      //OxUI::Property("Texture", props.Texture); //TODO:
      OxUI::end_properties();

      DrawParticleOverLifetimeModule("Velocity Over Lifetime", props.velocity_over_lifetime);
      DrawParticleOverLifetimeModule("Force Over Lifetime", props.force_over_lifetime);
      DrawParticleOverLifetimeModule("Color Over Lifetime", props.color_over_lifetime, true);
      DrawParticleBySpeedModule("Color By Speed", props.color_by_speed, true);
      DrawParticleOverLifetimeModule("Size Over Lifetime", props.size_over_lifetime);
      DrawParticleBySpeedModule("Size By Speed", props.size_by_speed);
      DrawParticleOverLifetimeModule("Rotation Over Lifetime", props.rotation_over_lifetime, false, true);
      DrawParticleBySpeedModule("Rotation By Speed", props.rotation_by_speed, false, true);
    });

  if (entity.has_component<CustomComponent>()) {
    const auto n = entity.get_component<CustomComponent>().name;
    const auto* n2 = (const char8_t*)n.c_str();
    DrawComponent<CustomComponent>(n2,
      entity,
      [](CustomComponent& component) {
        OxUI::begin_properties();
        OxUI::property("Component Name", &component.name, ImGuiInputTextFlags_EnterReturnsTrue);
        OxUI::end_properties();

        ImGui::Text("Fields");
        ImGui::BeginTable("FieldTable",
          3,
          ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchSame |
          ImGuiTableFlags_BordersInner | ImGuiTableFlags_BordersOuterH);
        ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 100.0f);
        ImGui::TableSetupColumn("Type", ImGuiTableColumnFlags_WidthStretch, 70.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 100.0f);
        ImGui::TableHeadersRow();
        for (int i = 0; i < (int)component.fields.size(); i++) {
          auto inputFlags = ImGuiInputTextFlags_EnterReturnsTrue |
                            ImGuiInputTextFlags_CallbackHistory | ImGuiInputTextFlags_EscapeClearsAll;
          ImGui::TableNextRow();
          ImGui::TableNextColumn();

          auto& field = component.fields[i];
          auto id = fmt::format("{0}_{1}", field.name, i);

          ImGui::PushID(id.c_str());
          ImGui::SetCursorPosY(ImGui::GetCursorPosY() + ImGui::GetStyle().FramePadding.y * 0.5f);

          ImGui::PushID("FieldName");
          ImGui::SetNextItemWidth(-1);
          ImGui::InputText("##", &field.name, inputFlags);
          ImGui::PopID();

          ImGui::TableNextColumn();
          const char* fieldTypes[] = {"INT", "FLOAT", "STRING", "BOOL"};
          int type = field.type;
          ImGui::SetNextItemWidth(-1);
          if (ImGui::Combo("##", &type, fieldTypes, 4))
            field.type = (CustomComponent::FieldType)type;

          ImGui::PushID("FieldValue");
          ImGui::TableNextColumn();
          ImGui::SetNextItemWidth(-1);
          if (field.type == CustomComponent::FieldType::INT
              || field.type == CustomComponent::FieldType::BOOL
              || field.type == CustomComponent::FieldType::FLOAT) {
            inputFlags |= ImGuiInputTextFlags_CharsDecimal;
          }
          ImGui::InputText("##", &field.value, inputFlags);
          ImGui::PopID();

          ImGui::PopID();
        }
        ImGui::EndTable();

        const float x = ImGui::GetContentRegionAvail().x;
        const float y = ImGui::GetFrameHeight();
        if (ImGui::Button("+ Add Field", {x, y})) {
          component.fields.emplace_back();
        }
      });
  }
}

void InspectorPanel::pp_probe_property(const bool value, const PostProcessProbe& component) const {
  if (value) {
    m_SelectedEntity.get_scene()->get_renderer()->dispatcher.trigger(ProbeChangeEvent{component});
  }
}
}
