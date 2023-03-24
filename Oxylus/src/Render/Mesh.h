#pragma once

#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include <string>
#include <vector>
#include <glm/detail/type_quat.hpp>

#include "Render/Vulkan/VulkanBuffer.h"
#include "Assets/Material.h"

#define TINYGLTF_NO_STB_IMAGE_WRITE 
#include "tinygltf/tiny_gltf.h"
#include "Utils/Log.h"
#include "Vulkan/VulkanImage.h"

namespace Oxylus {
  enum class VertexComponent {
    POSITION2D,
    POSITION,
    NORMAL,
    COLOR,
    UV,
    TANGENT,
    BITANGENT,
    JOINT0,
    WEIGHT0,
  };

  class Mesh {
  public:
    enum FileLoadingFlags {
      None = 0,
      PreMultiplyVertexColors,
      FlipY,
      DontLoadImages,
      DontCreateMaterials,
    };

    struct Primitive {
      uint32_t firstIndex;
      uint32_t indexCount;
      uint32_t firstVertex;
      uint32_t vertexCount;

      struct Dimensions {
        glm::vec3 min = glm::vec3(FLT_MAX);
        glm::vec3 max = glm::vec3(-FLT_MAX);
        glm::vec3 size;
        glm::vec3 center;
        float radius;
      } dimensions;

      int32_t materialIndex = 0;

      void SetDimensions(glm::vec3 min, glm::vec3 max);
      //Primitive(uint32_t firstIndex, uint32_t indexCount, Material& material) : firstIndex(firstIndex), indexCount(indexCount), material(material) {}
      Primitive(uint32_t firstIndex, uint32_t indexCount) : firstIndex(firstIndex), indexCount(indexCount) { }
    };

    struct Node {
      Node* Parent;
      uint32_t Index;
      uint32_t MeshIndex;
      std::vector<Node*> Children;
      std::vector<Primitive*> Primitives;
      glm::mat4 Matrix;
      std::string Name;
      int32_t SkinIndex = -1;
      glm::vec3 Translation{};
      glm::vec3 Scale{1.0f};
      glm::quat Rotation{};
      bool ContainsMesh = false;
      glm::mat4 LocalMatrix() const;
      glm::mat4 GetMatrix() const;
    };

    struct Vertex {
      glm::vec3 pos;
      glm::vec3 normal;
      glm::vec2 uv;
      glm::vec4 color;
      glm::vec4 joint0;
      glm::vec4 weight0;
      glm::vec4 tangent;
    };

    std::vector<Ref<VulkanImage>> m_Textures;
    std::vector<Node*> Nodes;
    std::vector<Node*> LinearNodes;
    VulkanBuffer VerticiesBuffer;
    VulkanBuffer IndiciesBuffer;
    uint32_t IndexCount = 0;
    std::string Name;
    std::string Path;
    uint32_t FileLoadingFlags = 0;
    bool ShouldUpdate = false;

    Mesh() = default;
    Mesh(std::string_view path, uint32_t fileLoadingFlags = None);
    ~Mesh();

    void LoadFromFile(const std::string& path, uint32_t fileLoadingFlags = None, float scale = 1);
    void SetScale(const glm::vec3& scale);
    void Draw(const vk::CommandBuffer& cmdBuffer) const;
    void UpdateMaterials();
    size_t GetNodeCount() const { return Nodes.size(); }
    const Ref<Material>& GetMaterial(uint32_t index) const;
    std::vector<Ref<Material>> GetMaterialsAsRef() const;
    void Destroy();

    operator bool() const {
      return !Nodes.empty();
    }

  private:
    std::vector<Ref<Material>> m_Materials;
    std::vector<uint32_t> m_IndexBuffer;
    std::vector<Vertex> m_VertexBuffer;
    uint32_t VertexCount = 0;
    std::vector<glm::vec4> m_DiffuseColors;
    glm::vec3 m_Scale{1.0f};
    glm::vec3 center{0.0f};
    glm::vec2 uvscale{1.0f};
    void LoadTextures(const tinygltf::Model& model);
    void LoadMaterials(tinygltf::Model& model);
    void LoadNode(Node* parent,
                  const tinygltf::Node& node,
                  uint32_t nodeIndex,
                  tinygltf::Model& model,
                  std::vector<uint32_t>& indexBuffer,
                  std::vector<Vertex>& vertexBuffer,
                  float globalscale);
    void LoadFailFallback();
  };

  struct VertexLayout {
    std::vector<VertexComponent> components;
    VertexLayout() = default;

    VertexLayout(std::vector<VertexComponent>&& components,
                 uint32_t binding = 0) : components(std::move(components)) { }

    uint32_t ComponentIndex(const VertexComponent component) const {
      for (size_t i = 0; i < components.size(); ++i) {
        if (components[i] == component) {
          return (uint32_t)i;
        }
      }
      return static_cast<uint32_t>(-1);
    }

    static vk::Format ComponentFormat(const VertexComponent component) {
      switch (component) {
        case VertexComponent::UV: return vk::Format::eR32G32Sfloat;
        case VertexComponent::POSITION2D: return vk::Format::eR32G32Sfloat;
        default: return vk::Format::eR32G32B32Sfloat;
      }
    }

    static uint32_t ComponentSize(VertexComponent component) {
      switch (component) {
        case VertexComponent::UV: return 2 * sizeof(float);
        case VertexComponent::POSITION2D: return 2 * sizeof(float);
        default: return 3 * sizeof(float);
      }
    }

    uint32_t stride() const {
      return sizeof(Mesh::Vertex);
    }

    uint32_t offset(uint32_t index) const {
      uint32_t res = 0;
      assert(index < components.size());
      for (uint32_t i = 0; i < index; ++i) {
        res += ComponentSize(components[i]);
      }
      return res;
    }
  };
}
