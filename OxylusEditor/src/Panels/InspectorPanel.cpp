#include "InspectorPanel.h"

#include <icons/IconsMaterialDesignIcons.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <fmt/format.h>

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

  void InspectorPanel::OnImGuiRender() {
    m_SelectedEntity = EditorLayer::Get()->GetSelectedEntity();
    m_Scene = EditorLayer::Get()->GetSelectedScene();

    ImGui::Begin(fmt::format("{} {}", StringUtils::FromChar8T(ICON_MDI_INFORMATION), "Inspector").c_str());
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
    if (entity.HasComponent<T>()) {
      static constexpr ImGuiTreeNodeFlags treeFlags = ImGuiTreeNodeFlags_DefaultOpen
                                                      | ImGuiTreeNodeFlags_SpanAvailWidth |
                                                      ImGuiTreeNodeFlags_AllowItemOverlap | ImGuiTreeNodeFlags_Framed |
                                                      ImGuiTreeNodeFlags_FramePadding;

      auto& component = entity.GetComponent<T>();

      const float lineHeight = GImGui->Font->FontSize + GImGui->Style.FramePadding.y * 2.0f;

      ImGui::SetCursorPosY(ImGui::GetCursorPosY() + lineHeight * 0.25f);

      const size_t id = entt::type_id<T>().hash();
      const bool open = ImGui::TreeNodeEx(reinterpret_cast<void*>(id), treeFlags, "%s", name);

      bool removeComponent = false;
      if (removable) {
        ImGui::PushID(static_cast<int>(id));

        const float frameHeight = ImGui::GetFrameHeight();
        ImGui::SameLine(ImGui::GetContentRegionMax().x - frameHeight * 1.2f);
        if (ImGui::Button(StringUtils::FromChar8T(ICON_MDI_SETTINGS), ImVec2{frameHeight * 1.2f, frameHeight}))
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
        entity.RemoveComponent<T>();
    }
  }

  bool InspectorPanel::DrawMaterialProperties(Ref<Material>& material, bool saveToCurrentPath) {
    bool loadAsset = false;

    if (ImGui::Button("Save")) {
      std::string path = {};
      if (saveToCurrentPath)
        path = FileDialogs::SaveFile({{"Material file", "oxmat"}}, "NewMaterial");
      else
        path = material->Path;
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
      material = CreateRef<Material>();
      material->Create();
    }
    ImGui::SameLine();
    const float x = ImGui::GetContentRegionAvail().x;
    const float y = ImGui::GetFrameHeight();
    ImGui::Button("Drop a material file", {x, y});
    if (ImGui::BeginDragDropTarget()) {
      if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
        std::filesystem::path path = IGUI::GetPathFromImGuiPayload(payload);
        const std::string ext = path.extension().string();
        path = AssetManager::GetAssetFileSystemPath(path);
        if (ext == ".oxmat") {
          MaterialSerializer(material).Deserialize(path.string());
          loadAsset = true;
        }
      }
      ImGui::EndDragDropTarget();
    }

    IGUI::BeginProperties();
    IGUI::Property("UV Scale", material->Parameters.UVScale);
    IGUI::Property("Use Albedo", (bool&)material->Parameters.UseAlbedo);
    if (IGUI::Property("Albedo", material->AlbedoTexture)) {
      material->Update();
    }
    IGUI::PropertyVector("Color", material->Parameters.Color, true, true);

    IGUI::Property("Specular", material->Parameters.Specular);
    IGUI::PropertyVector("Emmisive", material->Parameters.Emmisive, true, false);

    IGUI::Property("Use Normal", (bool&)material->Parameters.UseNormal);
    if (IGUI::Property("Normal", material->NormalTexture)) {
      material->Update();
    }

    IGUI::Property("Use Roughness", (bool&)material->Parameters.UseRoughness);
    if (IGUI::Property("Roughness", material->RoughnessTexture)) {
      material->Update();
    }
    IGUI::Property("Roughness", material->Parameters.Roughness);

    IGUI::Property("Use Metallic", (bool&)material->Parameters.UseMetallic);
    if (IGUI::Property("Metallic", material->MetallicTexture)) {
      material->Update();
    }
    IGUI::Property("Metallic", material->Parameters.Metallic);

    IGUI::Property("Use AO", (bool&)material->Parameters.UseAO);
    if (IGUI::Property("AO", material->AOTexture)) {
      material->Update();
    }

    IGUI::EndProperties();

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
      IGUI::BeginProperties();
      IGUI::Property("Enabled", propertyModule.Enabled);

      if (rotation) {
        T degrees = glm::degrees(propertyModule.Start);
        if (IGUI::PropertyVector("Start", degrees))
          propertyModule.Start = glm::radians(degrees);

        degrees = glm::degrees(propertyModule.End);
        if (IGUI::PropertyVector("End", degrees))
          propertyModule.End = glm::radians(degrees);
      }
      else {
        IGUI::PropertyVector("Start", propertyModule.Start, color);
        IGUI::PropertyVector("End", propertyModule.End, color);
      }

      IGUI::EndProperties();

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
      IGUI::BeginProperties();
      IGUI::Property("Enabled", propertyModule.Enabled);

      if (rotation) {
        T degrees = glm::degrees(propertyModule.Start);
        if (IGUI::PropertyVector("Start", degrees))
          propertyModule.Start = glm::radians(degrees);

        degrees = glm::degrees(propertyModule.End);
        if (IGUI::PropertyVector("End", degrees))
          propertyModule.End = glm::radians(degrees);
      }
      else {
        IGUI::PropertyVector("Start", propertyModule.Start, color);
        IGUI::PropertyVector("End", propertyModule.End, color);
      }

      IGUI::Property("Min Speed", propertyModule.MinSpeed);
      IGUI::Property("Max Speed", propertyModule.MaxSpeed);
      IGUI::EndProperties();
      ImGui::TreePop();
    }
  }

  template <typename Component>
  void InspectorPanel::DrawAddComponent(Entity entity, const char* name) const {
    if (ImGui::MenuItem(name)) {
      if (!entity.HasComponent<Component>())
        entity.AddComponentI<Component>();
      else
        OX_CORE_ERROR("Entity already has the {}!", typeid(Component).name());
      ImGui::CloseCurrentPopup();
    }
  }

  void InspectorPanel::DrawComponents(Entity entity) const {
    if (entity.HasComponent<TagComponent>()) {
      auto& tag = entity.GetComponent<TagComponent>().Tag;
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
      DrawAddComponent<RigidBodyComponent>(m_SelectedEntity, "Rigidbody");
      DrawAddComponent<BoxColliderComponent>(m_SelectedEntity, "Box Collider");
      DrawAddComponent<AudioSourceComponent>(m_SelectedEntity, "Audio Source");
      DrawAddComponent<LightComponent>(m_SelectedEntity, "Light");
      DrawAddComponent<ParticleSystemComponent>(m_SelectedEntity, "Particle System");
      DrawAddComponent<CameraComponent>(m_SelectedEntity, "Camera");
      DrawAddComponent<PostProcessProbe>(m_SelectedEntity, "PostProcess Probe");

      ImGui::EndPopup();
    }
    ImGui::PopItemWidth();

    DrawComponent<TransformComponent>(ICON_MDI_VECTOR_LINE " Transform Component",
      entity,
      [](TransformComponent& component) {
        IGUI::BeginProperties();
        IGUI::DrawVec3Control("Translation", component.Translation);
        Vec3 rotation = glm::degrees(component.Rotation);
        IGUI::DrawVec3Control("Rotation", rotation);
        component.Rotation = glm::radians(rotation);
        IGUI::DrawVec3Control("Scale", component.Scale, nullptr, 1.0f);
        IGUI::EndProperties();
      });

    DrawComponent<MeshRendererComponent>(ICON_MDI_VECTOR_SQUARE " Mesh Renderer Component",
      entity,
      [](MeshRendererComponent& component) {
        const float x = ImGui::GetContentRegionAvail().x;
        const float y = ImGui::GetFrameHeight();
        if (ImGui::Button(StringUtils::FromChar8T(ICON_MDI_FILE_UPLOAD), {x, y})) {
          const auto& path = FileDialogs::OpenFile({{"Mesh File", "gltf"}});
          if (!path.empty()) {
            component.MeshGeometry = AssetManager::GetMeshAsset(path).Data;
          }
        }
        if (!component.MeshGeometry)
          return;
        const char* fileName = component.MeshGeometry->Name.empty()
                                 ? "Empty"
                                 : component.MeshGeometry->Name.c_str();
        ImGui::Text("Loaded Mesh: %s", fileName);
      });

    DrawComponent<MaterialComponent>(ICON_MDI_SPRAY " Material Component",
      entity,
      [](MaterialComponent& component) {
        if (component.Materials.empty())
          return;

        constexpr ImGuiTreeNodeFlags flags =
          ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth |
          ImGuiTreeNodeFlags_FramePadding;

        for (auto& material : component.Materials) {
          if (ImGui::TreeNodeEx(material->Name.c_str(),
            flags,
            "%s %s",
            StringUtils::FromChar8T(ICON_MDI_CIRCLE),
            material->Name.c_str())) {
            if (DrawMaterialProperties(material)) {
              component.UsingMaterialAsset = true;
            }
            ImGui::TreePop();
          }
        }
      });

    DrawComponent<SkyLightComponent>(ICON_MDI_WEATHER_SUNNY " Sky Light Component",
      entity,
      [this](SkyLightComponent& component) {
        const char* filepath = component.Cubemap
                                 ? component.Cubemap->GetDesc().Path.c_str()
                                 : "Drop a cubemap file";
        const float x = ImGui::GetContentRegionAvail().x;
        const float y = ImGui::GetFrameHeight();
        if(ImGui::Button(filepath, {x, y}))
          const auto& path = FileDialogs::OpenFile({{"CubeMap File", ".ktx, .ktx2"}});
        if (ImGui::BeginDragDropTarget()) {
          if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
            const std::filesystem::path path = AssetManager::GetAssetFileSystemPath(IGUI::GetPathFromImGuiPayload(payload));
            const std::string ext = path.extension().string();
            if (ext == ".ktx" || ext == ".ktx2" || ext == ".hdr") {
              VulkanImageDescription CubeMapDesc;
              CubeMapDesc.Path = path.string();
              CubeMapDesc.Type = ImageType::TYPE_CUBE;
              component.Cubemap = AssetManager::GetImageAsset(CubeMapDesc).Data;
              m_Scene->GetRenderer().Dispatcher.trigger(SceneRenderer::SkyboxLoadEvent{component.Cubemap});
            }
          }
          ImGui::EndDragDropTarget();
        }
        ImGui::Spacing();
        IGUI::BeginProperties();
        IGUI::Property("Cubemap Lod Bias", component.CubemapLodBias);
        IGUI::Property("Flip Image", component.FlipImage);
        IGUI::EndProperties();
      });

    DrawComponent<PostProcessProbe>(ICON_MDI_SPRAY " PostProcess Probe Component",
      entity,
      [this](PostProcessProbe& component) {
        ImGui::Text("Vignette");
        IGUI::BeginProperties();
        PP_ProbeProperty(IGUI::Property("Enable", component.VignetteEnabled));
        PP_ProbeProperty(IGUI::Property("Intensity", component.VignetteIntensity));
        IGUI::EndProperties();
        ImGui::Separator();

        ImGui::Text("FilmGrain");
        IGUI::BeginProperties();
        PP_ProbeProperty(IGUI::Property("Enable", component.FilmGrainEnabled));
        PP_ProbeProperty(IGUI::Property("Intensity", component.FilmGrainIntensity));
        IGUI::EndProperties();
        ImGui::Separator();

        ImGui::Text("ChromaticAberration");
        IGUI::BeginProperties();
        PP_ProbeProperty(IGUI::Property("Enable", component.ChromaticAberrationEnabled));
        PP_ProbeProperty(IGUI::Property("Intensity", component.ChromaticAberrationIntensity));
        IGUI::EndProperties();
        ImGui::Separator();

        ImGui::Text("Sharpen");
        IGUI::BeginProperties();
        PP_ProbeProperty(IGUI::Property("Enable", component.SharpenEnabled));
        PP_ProbeProperty(IGUI::Property("Intensity", component.SharpenIntensity));
        IGUI::EndProperties();
        ImGui::Separator();
      });

    DrawComponent<AudioSourceComponent>(ICON_MDI_VOLUME_MEDIUM " Audio Source Component",
      entity,
      [&entity](AudioSourceComponent& component) {
        auto& config = component.Config;

        const char* filepath = component.Source
                                 ? component.Source->GetPath()
                                 : "Drop an audio file";
        const float x = ImGui::GetContentRegionAvail().x;
        const float y = ImGui::GetFrameHeight();
        ImGui::Button(filepath, {x, y});
        if (ImGui::BeginDragDropTarget()) {
          if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
            const std::filesystem::path path = IGUI::GetPathFromImGuiPayload(payload);
            const std::string ext = path.extension().string();
            if (ext == ".mp3" || ext == ".wav")
              component.Source = CreateRef<AudioSource>(AssetManager::GetAssetFileSystemPath(path).string().c_str());
          }
          ImGui::EndDragDropTarget();
        }
        ImGui::Spacing();

        IGUI::BeginProperties();
        IGUI::Property("Volume Multiplier", config.VolumeMultiplier);
        IGUI::Property("Pitch Multiplier", config.PitchMultiplier);
        IGUI::Property("Play On Awake", config.PlayOnAwake);
        IGUI::Property("Looping", config.Looping);
        IGUI::EndProperties();

        ImGui::Spacing();
        if (ImGui::Button(StringUtils::FromChar8T(ICON_MDI_PLAY "Play ")) &&
            component.Source)
          component.Source->Play();
        ImGui::SameLine();
        if (ImGui::Button(StringUtils::FromChar8T(ICON_MDI_PAUSE "Pause ")) &&
            component.Source)
          component.Source->Pause();
        ImGui::SameLine();
        if (ImGui::Button(StringUtils::FromChar8T(ICON_MDI_STOP "Stop ")) &&
            component.Source)
          component.Source->Stop();
        ImGui::Spacing();

        IGUI::BeginProperties();
        IGUI::Property("Spatialization", config.Spatialization);

        if (config.Spatialization) {
          ImGui::Indent();
          const char* attenuationTypeStrings[] = {
            "None", "Inverse", "Linear",
            "Exponential"
          };
          int attenuationType = static_cast<int>(config.AttenuationModel);
          if (IGUI::Property("Attenuation Model",
            attenuationType,
            attenuationTypeStrings,
            4))
            config.AttenuationModel = static_cast<AttenuationModelType>(attenuationType);
          IGUI::Property("Roll Off", config.RollOff);
          IGUI::Property("Min Gain", config.MinGain);
          IGUI::Property("Max Gain", config.MaxGain);
          IGUI::Property("Min Distance", config.MinDistance);
          IGUI::Property("Max Distance", config.MaxDistance);
          float degrees = glm::degrees(config.ConeInnerAngle);
          if (IGUI::Property("Cone Inner Angle", degrees))
            config.ConeInnerAngle = glm::radians(degrees);
          degrees = glm::degrees(config.ConeOuterAngle);
          if (IGUI::Property("Cone Outer Angle", degrees))
            config.ConeOuterAngle = glm::radians(degrees);
          IGUI::Property("Cone Outer Gain", config.ConeOuterGain);
          IGUI::Property("Doppler Factor", config.DopplerFactor);
          ImGui::Unindent();
        }
        IGUI::EndProperties();

        if (component.Source) {
          const glm::mat4 inverted = glm::inverse(entity.GetWorldTransform());
          const Vec3 forward = normalize(Vec3(inverted[2]));
          component.Source->SetConfig(config);
          component.Source->SetPosition(entity.GetTransform().Translation);
          component.Source->SetDirection(-forward);
        }
      });

    DrawComponent<AudioListenerComponent>(ICON_MDI_CIRCLE_SLICE_8 " Audio Listener Component",
      entity,
      [](AudioListenerComponent& component) {
        auto& config = component.Config;
        IGUI::BeginProperties();
        IGUI::Property("Active", component.Active);
        float degrees = glm::degrees(config.ConeInnerAngle);
        if (IGUI::Property("Cone Inner Angle", degrees))
          config.ConeInnerAngle = glm::radians(degrees);
        degrees = glm::degrees(config.ConeOuterAngle);
        if (IGUI::Property("Cone Outer Angle", degrees))
          config.ConeOuterAngle = glm::radians(degrees);
        IGUI::Property("Cone Outer Gain", config.ConeOuterGain);
        IGUI::EndProperties();
      });

    DrawComponent<LightComponent>(ICON_MDI_LAMP " Light Component",
      entity,
      [](LightComponent& component) {
        IGUI::BeginProperties();
        const char* lightTypeStrings[] = {"Directional", "Point", "Spot"};
        int lightType = static_cast<int>(component.Type);
        if (IGUI::Property("Light Type", lightType, lightTypeStrings, 3))
          component.Type = static_cast<LightComponent::LightType>(lightType);

        if (IGUI::Property("Use color temperature mode",
              component.UseColorTemperatureMode) && component.
            UseColorTemperatureMode) {
          ColorUtils::TempratureToColor(component.Temperature, component.Color);
        }

        if (component.UseColorTemperatureMode) {
          if (IGUI::Property<uint32_t>("Temperature (K)",
            component.Temperature,
            1000,
            40000))
            ColorUtils::TempratureToColor(component.Temperature, component.Color);
        }
        else {
          IGUI::PropertyVector("Color", component.Color, true);
        }

        if (IGUI::Property("Intensity", component.Intensity) &&
            component.Intensity < 0.0f) {
          component.Intensity = 0.0f;
        }

        ImGui::Spacing();

        if (component.Type == LightComponent::LightType::Point) {
          IGUI::Property("Range", component.Range);
        }
        else if (component.Type == LightComponent::LightType::Spot) {
          IGUI::Property("Range", component.Range);
          float degrees = glm::degrees(component.OuterCutOffAngle);
          if (IGUI::Property("Outer Cut-Off Angle", degrees, 1.0f, 90.0f))
            component.OuterCutOffAngle = glm::radians(degrees);
          degrees = glm::degrees(component.CutOffAngle);
          if (IGUI::Property("Cut-Off Angle", degrees, 1.0f, 90.0f))
            component.CutOffAngle = glm::radians(degrees);

          if (component.Range < 0.1f)
            component.Range = 0.1f;
          if (component.OuterCutOffAngle < component.CutOffAngle)
            component.CutOffAngle = component.OuterCutOffAngle;
          if (component.CutOffAngle > component.OuterCutOffAngle)
            component.OuterCutOffAngle = component.CutOffAngle;
        }
        else {
          const char* shadowQualityTypeStrings[] = {"Hard", "Soft", "Ultra Soft"};
          int shadowQualityType = static_cast<int>(component.ShadowQuality);

          if (IGUI::Property("Shadow Quality Type",
            shadowQualityType,
            shadowQualityTypeStrings,
            3))
            component.ShadowQuality = static_cast<LightComponent::ShadowQualityType>(shadowQualityType);
        }

        IGUI::EndProperties();
      });

    DrawComponent<RigidBodyComponent>(ICON_MDI_SOCCER " Rigidbody Component",
      entity,
      [this](RigidBodyComponent& component) {
        const auto bodyInterface = m_Scene->GetBodyInterface();
        IGUI::BeginProperties();
        // TODO(hatrickek): Inspector panel for rigidbody component.

        // Motion type
        const char* motionTypes[3] = {"Static", "Kinematic", "Dynamic"};
        const uint8_t currentType = (uint8_t)bodyInterface->GetMotionType(component.BodyID);
        int selectedType = currentType;
        if (IGUI::Property("Motion Type", selectedType, motionTypes, 3)) {
          bodyInterface->SetMotionType(component.BodyID, (JPH::EMotionType)(uint8_t)selectedType, JPH::EActivation::Activate);
        }

        // Friction
        const float currentFriction = bodyInterface->GetFriction(component.BodyID);
        float selectedFriction = currentFriction;
        if (IGUI::Property("Friction", selectedFriction, 0.0f, 10.0f)) {
          bodyInterface->SetFriction(component.BodyID, selectedFriction);
        }

        ImGui::Separator();

        // Shape
        auto shapeEnumToString = [](const JPH::EShapeSubType type) {
          switch (type) {
            case JPH::EShapeSubType::Sphere: return "Sphere";
            case JPH::EShapeSubType::Box: return "Box";
            case JPH::EShapeSubType::Triangle: return "Triangle";
            case JPH::EShapeSubType::Capsule: return "Capsule";
            case JPH::EShapeSubType::TaperedCapsule: return "TaperedCapsule";
            case JPH::EShapeSubType::Cylinder: return "Cylinder";
            case JPH::EShapeSubType::ConvexHull: return "ConvexHull";
            case JPH::EShapeSubType::Mesh: return "Mesh";
            default: return "Empty";
          }
        };
        const auto currentShape = bodyInterface->GetShape(component.BodyID)->GetSubType();
        IGUI::Text("Shape", shapeEnumToString(currentShape));

        IGUI::EndProperties();
      });

    DrawComponent<BoxColliderComponent>(ICON_MDI_CHECKBOX_BLANK_OUTLINE "Box Collider Component",
      entity,
      [](BoxColliderComponent& component) { });

    DrawComponent<CameraComponent>(ICON_MDI_CAMERA "Camera Component",
      entity,
      [](const CameraComponent& component) {
        IGUI::BeginProperties();
        static float fov = component.System->Fov;
        if (IGUI::Property("FOV", fov)) {
          component.System->SetFov(fov);
        }
        static float nearClip = component.System->NearClip;
        if (IGUI::Property("Near Clip", nearClip)) {
          component.System->SetNear(nearClip);
        }
        static float farClip = component.System->FarClip;
        if (IGUI::Property("Far Clip", farClip)) {
          component.System->SetFar(farClip);
        }
        IGUI::EndProperties();
      });

    DrawComponent<ParticleSystemComponent>(ICON_MDI_LAMP "Particle System Component",
      entity,
      [](const ParticleSystemComponent& component) {
        auto& props = component.System->GetProperties();

        ImGui::Text("Active Particles Count: %u",
          component.System->GetActiveParticleCount());
        ImGui::BeginDisabled(props.Looping);
        if (ImGui::Button(StringUtils::FromChar8T(ICON_MDI_PLAY)))
          component.System->Play();
        ImGui::SameLine();
        if (ImGui::Button(StringUtils::FromChar8T(ICON_MDI_STOP)))
          component.System->Stop();
        ImGui::EndDisabled();

        ImGui::Separator();

        IGUI::BeginProperties();
        IGUI::Property("Duration", props.Duration);
        if (IGUI::Property("Looping", props.Looping)) {
          if (props.Looping)
            component.System->Play();
        }
        IGUI::Property("Start Delay", props.StartDelay);
        IGUI::Property("Start Lifetime", props.StartLifetime);
        IGUI::PropertyVector("Start Velocity", props.StartVelocity);
        IGUI::PropertyVector("Start Color", props.StartColor, true);
        IGUI::PropertyVector("Start Size", props.StartSize);
        IGUI::PropertyVector("Start Rotation", props.StartRotation);
        IGUI::Property("Gravity Modifier", props.GravityModifier);
        IGUI::Property("Simulation Speed", props.SimulationSpeed);
        IGUI::Property("Play On Awake", props.PlayOnAwake);
        IGUI::Property("Max Particles", props.MaxParticles);
        IGUI::EndProperties();

        ImGui::Separator();

        IGUI::BeginProperties();
        IGUI::Property("Rate Over Time", props.RateOverTime);
        IGUI::Property("Rate Over Distance", props.RateOverDistance);
        IGUI::Property("Burst Count", props.BurstCount);
        IGUI::Property("Burst Time", props.BurstTime);
        IGUI::PropertyVector("Position Start", props.PositionStart);
        IGUI::PropertyVector("Position End", props.PositionEnd);
        IGUI::Property("Texture", props.Texture);
        IGUI::EndProperties();

        DrawParticleOverLifetimeModule("Velocity Over Lifetime", props.VelocityOverLifetime);
        DrawParticleOverLifetimeModule("Force Over Lifetime", props.ForceOverLifetime);
        DrawParticleOverLifetimeModule("Color Over Lifetime", props.ColorOverLifetime, true);
        DrawParticleBySpeedModule("Color By Speed", props.ColorBySpeed, true);
        DrawParticleOverLifetimeModule("Size Over Lifetime", props.SizeOverLifetime);
        DrawParticleBySpeedModule("Size By Speed", props.SizeBySpeed);
        DrawParticleOverLifetimeModule("Rotation Over Lifetime", props.RotationOverLifetime, false, true);
        DrawParticleBySpeedModule("Rotation By Speed", props.RotationBySpeed, false, true);
      });
  }

  void InspectorPanel::PP_ProbeProperty(const bool value) const {
    if (value) {
      m_SelectedEntity.GetScene()->GetRenderer().Dispatcher.trigger(SceneRenderer::ProbeChangeEvent{});
    }
  }
}
