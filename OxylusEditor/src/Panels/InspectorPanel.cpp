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
#include "UI/IGUI.h"
#include "Utils/StringUtils.h"
#include "Utils/UIUtils.h"
#include "Assets/MaterialSerializer.h"
#include "Render/Vulkan/VulkanRenderer.h"

namespace Oxylus {
static bool s_RenameEntity = false;

InspectorPanel::InspectorPanel() : EditorPanel("Inspector", ICON_MDI_INFORMATION, true) { }

void InspectorPanel::on_imgui_render() {
  m_SelectedEntity = EditorLayer::get()->get_selected_entity();
  m_Scene = EditorLayer::get()->GetSelectedScene();

  ImGui::Begin(fmt::format("{} {}", StringUtils::from_char8_t(ICON_MDI_INFORMATION), "Inspector").c_str());
  if (m_SelectedEntity) {
    DrawComponents(m_SelectedEntity);
  }
  ImGui::End();
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

bool InspectorPanel::DrawMaterialProperties(Ref<Material>& material, bool saveToCurrentPath) {
  bool loadAsset = false;

  if (ImGui::Button("Save")) {
    std::string path;
    if (saveToCurrentPath)
      path = FileDialogs::SaveFile({{"Material file", "oxmat"}}, "NewMaterial");
    else
      path = material->path;
    MaterialSerializer(*material).Serialize(path);
  }
  ImGui::SameLine();
  if (ImGui::Button("Load")) {
    const auto& path = FileDialogs::OpenFile({{"Material file", "oxmat"}});
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
      std::filesystem::path path = IGUI::get_path_from_im_gui_payload(payload);
      const std::string ext = path.extension().string();
      if (ext == ".oxmat") {
        MaterialSerializer(material).Deserialize(path.string());
        loadAsset = true;
      }
    }
    ImGui::EndDragDropTarget();
  }

  IGUI::begin_properties();
  IGUI::text("Alpha mode: ", material->alpha_mode_to_string());
  IGUI::property("UV Scale", material->parameters.uv_scale);
  IGUI::property("Use Albedo", (bool&)material->parameters.use_albedo);
  IGUI::property("Albedo", material->albedo_texture);
  IGUI::PropertyVector("Color", material->parameters.color, true, true);

  IGUI::property("Specular", material->parameters.specular);
  IGUI::PropertyVector("Emmisive", material->parameters.emmisive, true, true);

  IGUI::property("Use Normal", (bool&)material->parameters.use_normal);
  IGUI::property("Normal", material->normal_texture);

  IGUI::property("Use PhysicalMap(Roughness/Metallic)", (bool&)material->parameters.use_physical_map);
  IGUI::property("PhysicalMap", material->metallic_roughness_texture);

  IGUI::property("Roughness", material->parameters.roughness);

  IGUI::property("Metallic", material->parameters.metallic);

  IGUI::property("Use AO", (bool&)material->parameters.use_ao);
  IGUI::property("AO", material->ao_texture);

  IGUI::end_properties();

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
    IGUI::begin_properties();
    IGUI::property("Enabled", propertyModule.enabled);

    if (rotation) {
      T degrees = glm::degrees(propertyModule.start);
      if (IGUI::PropertyVector("Start", degrees))
        propertyModule.start = glm::radians(degrees);

      degrees = glm::degrees(propertyModule.end);
      if (IGUI::PropertyVector("End", degrees))
        propertyModule.end = glm::radians(degrees);
    }
    else {
      IGUI::PropertyVector("Start", propertyModule.start, color);
      IGUI::PropertyVector("End", propertyModule.end, color);
    }

    IGUI::end_properties();

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
    IGUI::begin_properties();
    IGUI::property("Enabled", propertyModule.enabled);

    if (rotation) {
      T degrees = glm::degrees(propertyModule.start);
      if (IGUI::PropertyVector("Start", degrees))
        propertyModule.start = glm::radians(degrees);

      degrees = glm::degrees(propertyModule.end);
      if (IGUI::PropertyVector("End", degrees))
        propertyModule.end = glm::radians(degrees);
    }
    else {
      IGUI::PropertyVector("Start", propertyModule.start, color);
      IGUI::PropertyVector("End", propertyModule.end, color);
    }

    IGUI::property("Min Speed", propertyModule.min_speed);
    IGUI::property("Max Speed", propertyModule.max_speed);
    IGUI::end_properties();
    ImGui::TreePop();
  }
}

template <typename Component>
void InspectorPanel::DrawAddComponent(Entity entity, const char* name) const {
  if (ImGui::MenuItem(name)) {
    if (!entity.has_component<Component>())
      entity.add_component_internal<Component>();
    else
      OX_CORE_ERROR("Entity already has the {}!", typeid(Component).name());
    ImGui::CloseCurrentPopup();
  }
}

void InspectorPanel::DrawComponents(Entity entity) const {
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
    DrawAddComponent<MeshRendererComponent>(m_SelectedEntity, "Mesh Renderer");
    DrawAddComponent<MaterialComponent>(m_SelectedEntity, "Material");
    DrawAddComponent<AudioSourceComponent>(m_SelectedEntity, "Audio Source");
    DrawAddComponent<LightComponent>(m_SelectedEntity, "Light");
    DrawAddComponent<ParticleSystemComponent>(m_SelectedEntity, "Particle System");
    DrawAddComponent<CameraComponent>(m_SelectedEntity, "Camera");
    DrawAddComponent<PostProcessProbe>(m_SelectedEntity, "PostProcess Probe");
    DrawAddComponent<RigidbodyComponent>(m_SelectedEntity, "Rigidbody");
    DrawAddComponent<BoxColliderComponent>(entity, "Box Collider");
    DrawAddComponent<SphereColliderComponent>(entity, "Sphere Collider");
    DrawAddComponent<CapsuleColliderComponent>(entity, "Capsule Collider");
    DrawAddComponent<TaperedCapsuleColliderComponent>(entity, "Tapered Capsule Collider");
    DrawAddComponent<CylinderColliderComponent>(entity, "Cylinder Collider");
    DrawAddComponent<CharacterControllerComponent>(entity, "Character Controller");
    DrawAddComponent<CustomComponent>(entity, "Custom Component");

    ImGui::EndPopup();
  }
  ImGui::PopItemWidth();

  DrawComponent<TransformComponent>(ICON_MDI_VECTOR_LINE " Transform Component",
    entity,
    [](TransformComponent& component) {
      IGUI::begin_properties();
      IGUI::draw_vec3_control("Translation", component.translation);
      Vec3 rotation = glm::degrees(component.rotation);
      IGUI::draw_vec3_control("Rotation", rotation);
      component.rotation = glm::radians(rotation);
      IGUI::draw_vec3_control("Scale", component.scale, nullptr, 1.0f);
      IGUI::end_properties();
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

      name_filter.Draw("##material_filter", ImGui::GetContentRegionAvail().x - (IGUI::get_icon_button_size(ICON_MDI_PLUS, "").x + 2.0f * ImGui::GetStyle().FramePadding.x));

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
            if (DrawMaterialProperties(material)) {
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
      const char* name = component.cubemap
                           ? component.cubemap->get_path().c_str()
                           : "Drop a hdr file";
      const float x = ImGui::GetContentRegionAvail().x;
      const float y = ImGui::GetFrameHeight();
      auto loadCubeMap = [](const Ref<Scene>& scene, const std::string& path, SkyLightComponent& comp) {
        if (path.empty())
          return;
        const auto ext = std::filesystem::path(path).extension().string();
        if (ext == ".hdr") {
          comp.cubemap = AssetManager::get_texture_asset({.Path = path, .Format = vuk::Format::eR8G8B8A8Srgb});
          scene->get_renderer()->dispatcher.trigger(SceneRenderer::SkyboxLoadEvent{comp.cubemap});
        }
      };
      if (ImGui::Button(name, {x, y})) {
        const std::string filePath = FileDialogs::OpenFile({{"HDR File", "hdr"}});
        loadCubeMap(m_Scene, filePath, component);
      }
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
          const auto path = IGUI::get_path_from_im_gui_payload(payload).string();
          loadCubeMap(m_Scene, path, component);
        }
        ImGui::EndDragDropTarget();
      }
      ImGui::Spacing();
      IGUI::begin_properties();
      IGUI::property("Lod Bias", component.lod_bias);
      IGUI::end_properties();
    });

  DrawComponent<PostProcessProbe>(ICON_MDI_SPRAY " PostProcess Probe Component",
    entity,
    [this](PostProcessProbe& component) {
      ImGui::Text("Vignette");
      IGUI::begin_properties();
      PP_ProbeProperty(IGUI::property("Enable", component.vignette_enabled), component);
      PP_ProbeProperty(IGUI::property("Intensity", component.vignette_intensity), component);
      IGUI::end_properties();
      ImGui::Separator();

      ImGui::Text("FilmGrain");
      IGUI::begin_properties();
      PP_ProbeProperty(IGUI::property("Enable", component.film_grain_enabled), component);
      PP_ProbeProperty(IGUI::property("Intensity", component.film_grain_intensity), component);
      IGUI::end_properties();
      ImGui::Separator();

      ImGui::Text("ChromaticAberration");
      IGUI::begin_properties();
      PP_ProbeProperty(IGUI::property("Enable", component.chromatic_aberration_enabled), component);
      PP_ProbeProperty(IGUI::property("Intensity", component.chromatic_aberration_intensity), component);
      IGUI::end_properties();
      ImGui::Separator();

      ImGui::Text("Sharpen");
      IGUI::begin_properties();
      PP_ProbeProperty(IGUI::property("Enable", component.sharpen_enabled), component);
      PP_ProbeProperty(IGUI::property("Intensity", component.sharpen_intensity), component);
      IGUI::end_properties();
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
          const std::filesystem::path path = IGUI::get_path_from_im_gui_payload(payload);
          const std::string ext = path.extension().string();
          if (ext == ".mp3" || ext == ".wav")
            component.source = create_ref<AudioSource>(AssetManager::get_asset_file_system_path(path).string().c_str());
        }
        ImGui::EndDragDropTarget();
      }
      ImGui::Spacing();

      IGUI::begin_properties();
      IGUI::property("Volume Multiplier", config.VolumeMultiplier);
      IGUI::property("Pitch Multiplier", config.PitchMultiplier);
      IGUI::property("Play On Awake", config.PlayOnAwake);
      IGUI::property("Looping", config.Looping);
      IGUI::end_properties();

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

      IGUI::begin_properties();
      IGUI::property("Spatialization", config.Spatialization);

      if (config.Spatialization) {
        ImGui::Indent();
        const char* attenuationTypeStrings[] = {
          "None",
          "Inverse",
          "Linear",
          "Exponential"
        };
        int attenuationType = static_cast<int>(config.AttenuationModel);
        if (IGUI::property("Attenuation Model",
          attenuationType,
          attenuationTypeStrings,
          4))
          config.AttenuationModel = static_cast<AttenuationModelType>(attenuationType);
        IGUI::property("Roll Off", config.RollOff);
        IGUI::property("Min Gain", config.MinGain);
        IGUI::property("Max Gain", config.MaxGain);
        IGUI::property("Min Distance", config.MinDistance);
        IGUI::property("Max Distance", config.MaxDistance);
        float degrees = glm::degrees(config.ConeInnerAngle);
        if (IGUI::property("Cone Inner Angle", degrees))
          config.ConeInnerAngle = glm::radians(degrees);
        degrees = glm::degrees(config.ConeOuterAngle);
        if (IGUI::property("Cone Outer Angle", degrees))
          config.ConeOuterAngle = glm::radians(degrees);
        IGUI::property("Cone Outer Gain", config.ConeOuterGain);
        IGUI::property("Doppler Factor", config.DopplerFactor);
        ImGui::Unindent();
      }
      IGUI::end_properties();

      if (component.source) {
        const glm::mat4 inverted = glm::inverse(entity.get_world_transform());
        const Vec3 forward = normalize(Vec3(inverted[2]));
        component.source->SetConfig(config);
        component.source->SetPosition(entity.get_transform().translation);
        component.source->SetDirection(-forward);
      }
    });

  DrawComponent<AudioListenerComponent>(ICON_MDI_CIRCLE_SLICE_8 " Audio Listener Component",
    entity,
    [](AudioListenerComponent& component) {
      auto& config = component.config;
      IGUI::begin_properties();
      IGUI::property("Active", component.active);
      float degrees = glm::degrees(config.ConeInnerAngle);
      if (IGUI::property("Cone Inner Angle", degrees))
        config.ConeInnerAngle = glm::radians(degrees);
      degrees = glm::degrees(config.ConeOuterAngle);
      if (IGUI::property("Cone Outer Angle", degrees))
        config.ConeOuterAngle = glm::radians(degrees);
      IGUI::property("Cone Outer Gain", config.ConeOuterGain);
      IGUI::end_properties();
    });

  DrawComponent<LightComponent>(ICON_MDI_LAMP " Light Component",
    entity,
    [](LightComponent& component) {
      IGUI::begin_properties();
      const char* lightTypeStrings[] = {"Directional", "Point", "Spot"};
      int lightType = static_cast<int>(component.type);
      if (IGUI::property("Light Type", lightType, lightTypeStrings, 3))
        component.type = static_cast<LightComponent::LightType>(lightType);

      if (IGUI::property("Use color temperature mode",
            component.use_color_temperature_mode) && component.
          use_color_temperature_mode) {
        ColorUtils::TempratureToColor(component.temperature, component.color);
      }

      if (component.use_color_temperature_mode) {
        if (IGUI::property<uint32_t>("Temperature (K)",
          component.temperature,
          1000,
          40000))
          ColorUtils::TempratureToColor(component.temperature, component.color);
      }
      else {
        IGUI::PropertyVector("Color", component.color, true);
      }

      if (IGUI::property("Intensity", component.intensity) &&
          component.intensity < 0.0f) {
        component.intensity = 0.0f;
      }

      ImGui::Spacing();

      if (component.type == LightComponent::LightType::Point) {
        IGUI::property("Range", component.range);
      }
      else if (component.type == LightComponent::LightType::Spot) {
        IGUI::property("Range", component.range);
        float degrees = glm::degrees(component.outer_cut_off_angle);
        if (IGUI::property("Outer Cut-Off Angle", degrees, 1.0f, 90.0f))
          component.outer_cut_off_angle = glm::radians(degrees);
        degrees = glm::degrees(component.cut_off_angle);
        if (IGUI::property("Cut-Off Angle", degrees, 1.0f, 90.0f))
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

        if (IGUI::property("Shadow Quality Type", shadowQualityType, shadowQualityTypeStrings, 3))
          component.shadow_quality = static_cast<LightComponent::ShadowQualityType>(shadowQualityType);
      }

      IGUI::end_properties();
    });

  DrawComponent<RigidbodyComponent>(ICON_MDI_SOCCER " Rigidbody Component",
    entity,
    [this](RigidbodyComponent& component) {
      IGUI::begin_properties();

      const char* bodyTypeStrings[] = {"Static", "Kinematic", "Dynamic"};
      int bodyType = static_cast<int>(component.type);
      if (IGUI::property("Body Type", bodyType, bodyTypeStrings, 3))
        component.type = static_cast<RigidbodyComponent::BodyType>(bodyType);

      if (component.type == RigidbodyComponent::BodyType::Dynamic) {
        IGUI::property("Mass", component.mass, 0.01f, 10000.0f);
        IGUI::property("Linear Drag", component.linear_drag);
        IGUI::property("Angular Drag", component.angular_drag);
        IGUI::property("Gravity Scale", component.gravity_scale);
        IGUI::property("Allow Sleep", component.allow_sleep);
        IGUI::property("Awake", component.awake);
        IGUI::property("Continuous", component.continuous);
        IGUI::property("Interpolation", component.interpolation);

        component.linear_drag = glm::max(component.linear_drag, 0.0f);
        component.angular_drag = glm::max(component.angular_drag, 0.0f);
      }

      IGUI::property("Is Sensor", component.is_sensor);
      IGUI::end_properties();
    });

  DrawComponent<BoxColliderComponent>(ICON_MDI_CHECKBOX_BLANK_OUTLINE " Box Collider",
    entity,
    [](BoxColliderComponent& component) {
      IGUI::begin_properties();
      IGUI::PropertyVector("Size", component.size);
      IGUI::PropertyVector("Offset", component.offset);
      IGUI::property("Density", component.density);
      IGUI::property("Friction", component.friction, 0.0f, 1.0f);
      IGUI::property("Restitution", component.restitution, 0.0f, 1.0f);
      IGUI::end_properties();

      component.density = glm::max(component.density, 0.001f);
    });

  DrawComponent<SphereColliderComponent>(ICON_MDI_CIRCLE_OUTLINE " Sphere Collider",
    entity,
    [](SphereColliderComponent& component) {
      IGUI::begin_properties();
      IGUI::property("Radius", component.radius);
      IGUI::PropertyVector("Offset", component.offset);
      IGUI::property("Density", component.density);
      IGUI::property("Friction", component.friction, 0.0f, 1.0f);
      IGUI::property("Restitution", component.restitution, 0.0f, 1.0f);
      IGUI::end_properties();

      component.density = glm::max(component.density, 0.001f);
    });

  DrawComponent<CapsuleColliderComponent>(ICON_MDI_CIRCLE_OUTLINE " Capsule Collider",
    entity,
    [](CapsuleColliderComponent& component) {
      IGUI::begin_properties();
      IGUI::property("Height", component.height);
      IGUI::property("Radius", component.radius);
      IGUI::PropertyVector("Offset", component.offset);
      IGUI::property("Density", component.density);
      IGUI::property("Friction", component.friction, 0.0f, 1.0f);
      IGUI::property("Restitution", component.restitution, 0.0f, 1.0f);
      IGUI::end_properties();

      component.density = glm::max(component.density, 0.001f);
    });

  DrawComponent<TaperedCapsuleColliderComponent>(ICON_MDI_CIRCLE_OUTLINE " Tapered Capsule Collider",
    entity,
    [](TaperedCapsuleColliderComponent& component) {
      IGUI::begin_properties();
      IGUI::property("Height", component.height);
      IGUI::property("Top Radius", component.top_radius);
      IGUI::property("Bottom Radius", component.bottom_radius);
      IGUI::PropertyVector("Offset", component.offset);
      IGUI::property("Density", component.density);
      IGUI::property("Friction", component.friction, 0.0f, 1.0f);
      IGUI::property("Restitution", component.restitution, 0.0f, 1.0f);
      IGUI::end_properties();

      component.density = glm::max(component.density, 0.001f);
    });

  DrawComponent<CylinderColliderComponent>(ICON_MDI_CIRCLE_OUTLINE " Cylinder Collider",
    entity,
    [](CylinderColliderComponent& component) {
      IGUI::begin_properties();
      IGUI::property("Height", component.height);
      IGUI::property("Radius", component.radius);
      IGUI::PropertyVector("Offset", component.offset);
      IGUI::property("Density", component.density);
      IGUI::property("Friction", component.friction, 0.0f, 1.0f);
      IGUI::property("Restitution", component.restitution, 0.0f, 1.0f);
      IGUI::end_properties();

      component.density = glm::max(component.density, 0.001f);
    });

  DrawComponent<CharacterControllerComponent>(ICON_MDI_CIRCLE_OUTLINE " Character Controller",
    entity,
    [](CharacterControllerComponent& component) {
      IGUI::begin_properties();
      IGUI::property("CharacterHeightStanding", component.character_height_standing);
      IGUI::property("CharacterRadiusStanding", component.character_radius_standing);
      IGUI::property("CharacterHeightCrouching", component.character_height_crouching);
      IGUI::property("CharacterRadiusCrouching", component.character_radius_crouching);

      // Movement
      IGUI::property("ControlMovementDuringJump", component.control_movement_during_jump);
      IGUI::property("JumpForce", component.jump_force);

      IGUI::property("Friction", component.friction, 0.0f, 1.0f);
      IGUI::property("CollisionTolerance", component.collision_tolerance);
      IGUI::end_properties();
    });

  DrawComponent<CameraComponent>(ICON_MDI_CAMERA "Camera Component",
    entity,
    [](const CameraComponent& component) {
      IGUI::begin_properties();
      static float fov = component.system->Fov;
      if (IGUI::property("FOV", fov)) {
        component.system->SetFov(fov);
      }
      static float nearClip = component.system->NearClip;
      if (IGUI::property("Near Clip", nearClip)) {
        component.system->SetNear(nearClip);
      }
      static float farClip = component.system->FarClip;
      if (IGUI::property("Far Clip", farClip)) {
        component.system->SetFar(farClip);
      }
      IGUI::end_properties();
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

      IGUI::begin_properties();
      IGUI::property("Duration", props.duration);
      if (IGUI::property("Looping", props.looping)) {
        if (props.looping)
          component.system->play();
      }
      IGUI::property("Start Delay", props.start_delay);
      IGUI::property("Start Lifetime", props.start_lifetime);
      IGUI::PropertyVector("Start Velocity", props.start_velocity);
      IGUI::PropertyVector("Start Color", props.start_color, true);
      IGUI::PropertyVector("Start Size", props.start_size);
      IGUI::PropertyVector("Start Rotation", props.start_rotation);
      IGUI::property("Gravity Modifier", props.gravity_modifier);
      IGUI::property("Simulation Speed", props.simulation_speed);
      IGUI::property("Play On Awake", props.play_on_awake);
      IGUI::property("Max Particles", props.max_particles);
      IGUI::end_properties();

      ImGui::Separator();

      IGUI::begin_properties();
      IGUI::property("Rate Over Time", props.rate_over_time);
      IGUI::property("Rate Over Distance", props.rate_over_distance);
      IGUI::property("Burst Count", props.burst_count);
      IGUI::property("Burst Time", props.burst_time);
      IGUI::PropertyVector("Position Start", props.position_start);
      IGUI::PropertyVector("Position End", props.position_end);
      //IGUI::Property("Texture", props.Texture); //TODO:
      IGUI::end_properties();

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
        IGUI::begin_properties();
        IGUI::property("Component Name", &component.name, ImGuiInputTextFlags_EnterReturnsTrue);
        IGUI::end_properties();

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

void InspectorPanel::PP_ProbeProperty(const bool value, const PostProcessProbe& component) const {
  if (value) {
    m_SelectedEntity.get_scene()->get_renderer()->dispatcher.trigger(SceneRenderer::ProbeChangeEvent{component});
  }
}
}
