#include "InspectorPanel.h"

#include <icons/IconsMaterialDesignIcons.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <misc/cpp/imgui_stdlib.h>

#include <Assets/AssetManager.h>

#include "Utils/ColorUtils.h"

#include "EditorLayer.h"
#include "Assets/MaterialSerializer.h"
#include "Scene/Entity.h"
#include "UI/OxUI.h"
#include "Utils/StringUtils.h"
#include "Utils/UIUtils.h"

#include "Render/SceneRendererEvents.h"

#include "Scene/SceneRenderer.h"

namespace Ox {
static bool s_rename_entity = false;

InspectorPanel::InspectorPanel() : EditorPanel("Inspector", ICON_MDI_INFORMATION, true), m_Scene(nullptr) { }

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
static void draw_component(const char* name,
                           Entity entity,
                           UIFunction ui_function,
                           const bool removable = true) {
  if (entity.has_component<T>()) {
    static constexpr ImGuiTreeNodeFlags TREE_FLAGS = ImGuiTreeNodeFlags_DefaultOpen
                                                     | ImGuiTreeNodeFlags_SpanAvailWidth |
                                                     ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_Framed |
                                                     ImGuiTreeNodeFlags_FramePadding;

    auto& component = entity.get_component<T>();

    const float line_height = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;

    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + line_height * 0.25f);

    const size_t id = entt::type_id<T>().hash();
    std::string name_str = StringUtils::from_char8_t(EditorTheme::component_icon_map[typeid(T).hash_code()]);
    name_str = name_str.append(name);
    const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(id), TREE_FLAGS, "%s", name_str.c_str());

    bool remove_component = false;
    if (removable) {
      ImGui::PushID((int)id);

      const float frame_height = ImGui::GetFrameHeight();
      ImGui::SameLine(ImGui::GetContentRegionMax().x - frame_height * 1.2f);
      if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_SETTINGS), ImVec2{frame_height * 1.2f, frame_height}))
        ImGui::OpenPopup("ComponentSettings");

      if (ImGui::BeginPopup("ComponentSettings")) {
        if (ImGui::MenuItem("Remove Component"))
          remove_component = true;

        ImGui::EndPopup();
      }

      ImGui::PopID();
    }

    if (open) {
      ui_function(component);
      ImGui::TreePop();
    }

    if (remove_component)
      entity.remove_component<T>();
  }
}

bool InspectorPanel::draw_material_properties(Shared<Material>& material, const bool save_to_current_path) {
  bool load_asset = false;

  if (ImGui::Button("Save")) {
    std::string path;
    if (save_to_current_path)
      path = FileDialogs::save_file({{"Material file", "oxmat"}}, "NewMaterial");
    else
      path = material->path;
    MaterialSerializer(*material).serialize(path);
  }
  ImGui::SameLine();
  if (ImGui::Button("Load")) {
    const auto& path = FileDialogs::open_file({{"Material file", "oxmat"}});
    if (!path.empty()) {
      MaterialSerializer(material).deserialize(path);
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Reset")) {
    material = create_shared<Material>();
    material->create();
  }
  ImGui::SameLine();
  const float x = ImGui::GetContentRegionAvail().x;
  const float y = ImGui::GetFrameHeight();
  ImGui::Button("Drop a material file", {x, y});
  if (ImGui::BeginDragDropTarget()) {
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
      const std::filesystem::path path = OxUI::get_path_from_imgui_payload(payload);
      const std::string ext = path.extension().string();
      if (ext == ".oxmat") {
        MaterialSerializer(material).deserialize(path.string());
        load_asset = true;
      }
    }
    ImGui::EndDragDropTarget();
  }

  OxUI::begin_properties();
  OxUI::text("Alpha mode: ", material->alpha_mode_to_string());
  OxUI::property("UV Scale", &material->parameters.uv_scale);

  if (OxUI::property("Albedo", material->get_albedo_texture()))
    material->set_albedo_texture(material->get_albedo_texture());
  OxUI::property_vector("Color", material->parameters.color, true, true);

  OxUI::property("Reflectance", &material->parameters.reflectance);
  if (OxUI::property("Normal", material->get_normal_texture()))
    material->set_normal_texture(material->get_normal_texture());

  if (OxUI::property("PhysicalMap", material->get_physical_texture()))
    material->set_physical_texture(material->get_physical_texture());
  OxUI::property("Roughness", &material->parameters.roughness);
  OxUI::property("Metallic", &material->parameters.metallic);

  if (OxUI::property("AO", material->get_ao_texture()))
    material->set_ao_texture(material->get_ao_texture());

  if (OxUI::property("Emissive", material->get_emissive_texture()))
    material->set_emissive_texture(material->get_emissive_texture());
  OxUI::property_vector("Emissive Color", material->parameters.emissive, true, true);

  OxUI::end_properties();

  return load_asset;
}

template <typename T>
static void draw_particle_over_lifetime_module(const std::string_view module_name,
                                               OverLifetimeModule<T>& property_module,
                                               bool color = false,
                                               bool rotation = false) {
  static constexpr ImGuiTreeNodeFlags TREE_FLAGS = ImGuiTreeNodeFlags_DefaultOpen
                                                   | ImGuiTreeNodeFlags_SpanAvailWidth |
                                                   ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_Framed |
                                                   ImGuiTreeNodeFlags_FramePadding;

  if (ImGui::TreeNodeEx(module_name.data(), TREE_FLAGS, "%s", module_name.data())) {
    OxUI::begin_properties();
    OxUI::property("Enabled", &property_module.enabled);

    if (rotation) {
      T degrees = glm::degrees(property_module.start);
      if (OxUI::property_vector("Start", degrees))
        property_module.start = glm::radians(degrees);

      degrees = glm::degrees(property_module.end);
      if (OxUI::property_vector("End", degrees))
        property_module.end = glm::radians(degrees);
    }
    else {
      OxUI::property_vector("Start", property_module.start, color);
      OxUI::property_vector("End", property_module.end, color);
    }

    OxUI::end_properties();

    ImGui::TreePop();
  }
}

template <typename T>
static void draw_particle_by_speed_module(const std::string_view module_name,
                                          BySpeedModule<T>& property_module,
                                          bool color = false,
                                          bool rotation = false) {
  static constexpr ImGuiTreeNodeFlags TREE_FLAGS = ImGuiTreeNodeFlags_DefaultOpen
                                                   | ImGuiTreeNodeFlags_SpanAvailWidth |
                                                   ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_Framed |
                                                   ImGuiTreeNodeFlags_FramePadding;

  if (ImGui::TreeNodeEx(module_name.data(), TREE_FLAGS, "%s", module_name.data())) {
    OxUI::begin_properties();
    OxUI::property("Enabled", &property_module.enabled);

    if (rotation) {
      T degrees = glm::degrees(property_module.start);
      if (OxUI::property_vector("Start", degrees))
        property_module.start = glm::radians(degrees);

      degrees = glm::degrees(property_module.end);
      if (OxUI::property_vector("End", degrees))
        property_module.end = glm::radians(degrees);
    }
    else {
      OxUI::property_vector("Start", property_module.start, color);
      OxUI::property_vector("End", property_module.end, color);
    }

    OxUI::property("Min Speed", &property_module.min_speed);
    OxUI::property("Max Speed", &property_module.max_speed);
    OxUI::end_properties();
    ImGui::TreePop();
  }
}

template <typename Component>
void InspectorPanel::draw_add_component(Entity entity, const char* name) {
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
    if (s_rename_entity)
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
    draw_add_component<MeshComponent>(m_SelectedEntity, "Mesh Renderer");
    draw_add_component<MaterialComponent>(m_SelectedEntity, "Material");
    draw_add_component<AudioSourceComponent>(m_SelectedEntity, "Audio Source");
    draw_add_component<AudioListenerComponent>(m_SelectedEntity, "Audio Listener");
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
    draw_add_component<LuaScriptComponent>(entity, "Lua Script Component");

    ImGui::EndPopup();
  }
  ImGui::PopItemWidth();

  draw_component<TransformComponent>(
    " Transform Component",
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

  draw_component<MeshComponent>(
    " Mesh Renderer Component",
    entity,
    [](const MeshComponent& component) {
      if (!component.mesh_base)
        return;
      const char* file_name = component.mesh_base->name.empty()
                                ? "Empty"
                                : component.mesh_base->name.c_str();
      ImGui::Text("Loaded Mesh: %s", file_name);
      ImGui::Text("Material Count: %d", (uint32_t)component.mesh_base->get_materials_as_ref().size());
    });

  draw_component<MaterialComponent>(
    " Material Component",
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

      for (auto i = 0; i < (uint32_t)component.materials.size(); i++) {
        auto& material = component.materials[i];
        if (name_filter.PassFilter(material->name.c_str())) {
          ImGui::PushID(i);
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
          ImGui::PopID();
        }
      }
    });

  draw_component<AnimationComponent>(
    " Animation Component",
    entity,
    [](AnimationComponent& component) {
      constexpr ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth |
                                           ImGuiTreeNodeFlags_FramePadding;

      const float filter_cursor_pos_x = ImGui::GetCursorPosX();
      ImGuiTextFilter name_filter;

      name_filter.Draw("##animation_filter", ImGui::GetContentRegionAvail().x - (OxUI::get_icon_button_size(ICON_MDI_PLUS, "").x + 2.0f * ImGui::GetStyle().FramePadding.x));

      if (!name_filter.IsActive()) {
        ImGui::SameLine();
        ImGui::SetCursorPosX(filter_cursor_pos_x + ImGui::GetFontSize() * 0.5f);
        ImGui::TextUnformatted(StringUtils::from_char8_t(ICON_MDI_MAGNIFY " Search..."));
      }

      for (uint32_t index = 0; index < (uint32_t)component.animations.size(); index++) {
        const auto& animation = component.animations[index];
        if (name_filter.PassFilter(animation->name.c_str())) {
          if (ImGui::TreeNodeEx(animation->name.c_str(), flags, "%s %s", StringUtils::from_char8_t(ICON_MDI_CIRCLE), animation->name.c_str())) {
            if (ImGui::Button("Play", {ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight()})) {
              component.current_animation_index = index;
            }
            ImGui::TreePop();
          }
        }
      }
    });

  draw_component<SkyLightComponent>(
    " Sky Light Component",
    entity,
    [this](SkyLightComponent& component) {
      const std::string name = component.cubemap
                                 ? FileSystem::get_file_name(component.cubemap->get_path())
                                 : "Drop a hdr file";
      auto load_cube_map = [](Scene* scene, const std::string& path, SkyLightComponent& comp) {
        if (path.empty())
          return;
        const auto ext = FileSystem::get_file_extension(path);
        if (ext == "hdr") {
          comp.cubemap = AssetManager::get_texture_asset({.path = path, .format = vuk::Format::eR8G8B8A8Srgb, .generate_mips = false});
          scene->get_renderer()->dispatcher.trigger(SkyboxLoadEvent{comp.cubemap});
        }
      };
      const float x = ImGui::GetContentRegionAvail().x;
      const float y = ImGui::GetFrameHeight();
      if (ImGui::Button(name.c_str(), {x - 60.f, y})) {
        const std::string file_path = FileDialogs::open_file({{"HDR File", "hdr"}});
        load_cube_map(m_Scene, file_path, component);
      }
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
          const auto path = OxUI::get_path_from_imgui_payload(payload).string();
          load_cube_map(m_Scene, path, component);
        }
        ImGui::EndDragDropTarget();
      }
      ImGui::SameLine(0, 2.f);
      if (ImGui::Button("x", {50.f, 0.f})) {
        component.cubemap = {};
        m_Scene->get_renderer()->dispatcher.trigger(SkyboxLoadEvent{component.cubemap});
      }
      ImGui::Spacing();
    });

  draw_component<PostProcessProbe>(
    " PostProcess Probe Component",
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

  draw_component<AudioSourceComponent>(
    " Audio Source Component",
    entity,
    [&entity](AudioSourceComponent& component) {
      auto& config = component.config;
      const char* filepath = component.source
                               ? component.source->get_path()
                               : "Drop an audio file";

      auto load_file = [](const std::filesystem::path& path, AudioSourceComponent& comp) {
        if (const std::string ext = path.extension().string(); ext == ".mp3" || ext == ".wav" || ext == ".flac")
          comp.source = create_shared<AudioSource>(AssetManager::get_asset_file_system_path(path).string().c_str());
      };

      const float x = ImGui::GetContentRegionAvail().x;
      const float y = ImGui::GetFrameHeight();
      if (ImGui::Button(filepath, {x, y})) {
        const std::string file_path = FileDialogs::open_file({{"Audio file", "mp3, wav, flac"}});
        load_file(file_path, component);
      }
      if (ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
          const std::filesystem::path path = OxUI::get_path_from_imgui_payload(payload);
          load_file(path, component);
        }
        ImGui::EndDragDropTarget();
      }
      ImGui::Spacing();

      OxUI::begin_properties();
      OxUI::property("Volume Multiplier", &config.volume_multiplier);
      OxUI::property("Pitch Multiplier", &config.pitch_multiplier);
      OxUI::property("Play On Awake", &config.play_on_awake);
      OxUI::property("Looping", &config.looping);
      OxUI::end_properties();

      ImGui::Spacing();
      if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_PLAY "Play ")) &&
          component.source)
        component.source->play();
      ImGui::SameLine();
      if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_PAUSE "Pause ")) &&
          component.source)
        component.source->pause();
      ImGui::SameLine();
      if (ImGui::Button(StringUtils::from_char8_t(ICON_MDI_STOP "Stop ")) &&
          component.source)
        component.source->stop();
      ImGui::Spacing();

      OxUI::begin_properties();
      OxUI::property("Spatialization", &config.spatialization);

      if (config.spatialization) {
        ImGui::Indent();
        const char* attenuation_type_strings[] = {
          "None",
          "Inverse",
          "Linear",
          "Exponential"
        };
        int attenuation_type = static_cast<int>(config.attenuation_model);
        if (OxUI::property("Attenuation Model", &attenuation_type, attenuation_type_strings, 4))
          config.attenuation_model = static_cast<AttenuationModelType>(attenuation_type);
        OxUI::property("Roll Off", &config.roll_off);
        OxUI::property("Min Gain", &config.min_gain);
        OxUI::property("Max Gain", &config.max_gain);
        OxUI::property("Min Distance", &config.min_distance);
        OxUI::property("Max Distance", &config.max_distance);
        float degrees = glm::degrees(config.cone_inner_angle);
        if (OxUI::property("Cone Inner Angle", &degrees))
          config.cone_inner_angle = glm::radians(degrees);
        degrees = glm::degrees(config.cone_outer_angle);
        if (OxUI::property("Cone Outer Angle", &degrees))
          config.cone_outer_angle = glm::radians(degrees);
        OxUI::property("Cone Outer Gain", &config.cone_outer_gain);
        OxUI::property("Doppler Factor", &config.doppler_factor);
        ImGui::Unindent();
      }
      OxUI::end_properties();

      if (component.source) {
        const glm::mat4 inverted = glm::inverse(entity.get_world_transform());
        const Vec3 forward = normalize(Vec3(inverted[2]));
        component.source->set_config(config);
        component.source->set_position(entity.get_transform().position);
        component.source->set_direction(-forward);
      }
    });

  draw_component<AudioListenerComponent>(
    " Audio Listener Component",
    entity,
    [](AudioListenerComponent& component) {
      auto& config = component.config;
      OxUI::begin_properties();
      OxUI::property("Active", &component.active);
      float degrees = glm::degrees(config.cone_inner_angle);
      if (OxUI::property("Cone Inner Angle", &degrees))
        config.cone_inner_angle = glm::radians(degrees);
      degrees = glm::degrees(config.cone_outer_angle);
      if (OxUI::property("Cone Outer Angle", &degrees))
        config.cone_outer_angle = glm::radians(degrees);
      OxUI::property("Cone Outer Gain", &config.cone_outer_gain);
      OxUI::end_properties();
    });

  draw_component<LightComponent>(
    " Light Component",
    entity,
    [](LightComponent& component) {
      OxUI::begin_properties();
      const char* light_type_strings[] = {"Directional", "Point", "Spot"};
      int light_type = static_cast<int>(component.type);
      if (OxUI::property("Light Type", &light_type, light_type_strings, 3))
        component.type = static_cast<LightComponent::LightType>(light_type);

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
        const char* shadow_quality_type_strings[] = {"Hard", "Soft", "Ultra Soft"};
        int shadow_quality_type = static_cast<int>(component.shadow_quality);

        if (OxUI::property("Shadow Quality Type", &shadow_quality_type, shadow_quality_type_strings, 3))
          component.shadow_quality = static_cast<LightComponent::ShadowQualityType>(shadow_quality_type);
      }

      OxUI::end_properties();
    });

  draw_component<RigidbodyComponent>(
    " Rigidbody Component",
    entity,
    [this](RigidbodyComponent& component) {
      OxUI::begin_properties();

      const char* body_type_strings[] = {"Static", "Kinematic", "Dynamic"};
      int body_type = static_cast<int>(component.type);
      if (OxUI::property("Body Type", &body_type, body_type_strings, 3))
        component.type = static_cast<RigidbodyComponent::BodyType>(body_type);

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

  draw_component<BoxColliderComponent>(
    " Box Collider",
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

  draw_component<SphereColliderComponent>(
    " Sphere Collider",
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

  draw_component<CapsuleColliderComponent>(
    " Capsule Collider",
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

  draw_component<TaperedCapsuleColliderComponent>(
    " Tapered Capsule Collider",
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

  draw_component<CylinderColliderComponent>(
    " Cylinder Collider",
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

  draw_component<CharacterControllerComponent>(
    " Character Controller",
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

  draw_component<CameraComponent>(
    "Camera Component",
    entity,
    [](const CameraComponent& component) {
      OxUI::begin_properties();
      static float fov = component.system->get_fov();
      if (OxUI::property("FOV", &fov)) {
        component.system->set_fov(fov);
      }
      static float near_clip = component.system->get_near();
      if (OxUI::property("Near Clip", &near_clip)) {
        component.system->set_near(near_clip);
      }
      static float far_clip = component.system->get_far();
      if (OxUI::property("Far Clip", &far_clip)) {
        component.system->set_far(far_clip);
      }
      OxUI::end_properties();
    });

  draw_component<LuaScriptComponent>(
    "Lua Script Component",
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
          comp.lua_system = create_shared<LuaSystem>(path);
        }
      };
      const float x = ImGui::GetContentRegionAvail().x;
      const float y = ImGui::GetFrameHeight();
      if (ImGui::Button(name.c_str(), {x, y})) {
        const std::string file_path = FileDialogs::open_file({{"Lua file", "lua"}});
        load_script(file_path, component);
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

  draw_component<ParticleSystemComponent>(
    "Particle System Component",
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

      draw_particle_over_lifetime_module("Velocity Over Lifetime", props.velocity_over_lifetime);
      draw_particle_over_lifetime_module("Force Over Lifetime", props.force_over_lifetime);
      draw_particle_over_lifetime_module("Color Over Lifetime", props.color_over_lifetime, true);
      draw_particle_by_speed_module("Color By Speed", props.color_by_speed, true);
      draw_particle_over_lifetime_module("Size Over Lifetime", props.size_over_lifetime);
      draw_particle_by_speed_module("Size By Speed", props.size_by_speed);
      draw_particle_over_lifetime_module("Rotation Over Lifetime", props.rotation_over_lifetime, false, true);
      draw_particle_by_speed_module("Rotation By Speed", props.rotation_by_speed, false, true);
    });
}

void InspectorPanel::pp_probe_property(const bool value, const PostProcessProbe& component) const {
  if (value) {
    m_SelectedEntity.get_scene()->get_renderer()->dispatcher.trigger(ProbeChangeEvent{component});
  }
}
}
