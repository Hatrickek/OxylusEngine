#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <glm/detail/type_quat.hpp>

#include "Assets/Material.h"

#define TINYGLTF_NO_STB_IMAGE_WRITE 
#include <vuk/Buffer.hpp>

#include "Core/Types.h"
#include "tinygltf/tiny_gltf.h"

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
  enum FileLoadingFlags : int {
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
      Vec3 min = glm::vec3(FLT_MAX);
      Vec3 max = glm::vec3(-FLT_MAX);
      Vec3 size;
      Vec3 center;
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
    Mat4 Matrix;
    std::string Name;
    int32_t SkinIndex = -1;
    Vec3 Translation{};
    Vec3 Scale{1.0f};
    Quat Rotation{};
    bool ContainsMesh = false;
    Mat4 LocalMatrix() const;
    Mat4 GetMatrix() const;
  };

  struct Vertex {
    Vec3 Pos;
    Vec3 Normal;
    Vec2 UV;
    Vec4 Tangent;
    Vec4 Color;
    Vec4 Joint0;
    Vec4 Weight0;
  };

  std::vector<Ref<TextureAsset>> m_Textures;
  std::vector<Node*> Nodes;
  std::vector<Node*> LinearNodes;
  vuk::Unique<vuk::Buffer> m_VerticiesBuffer;
  vuk::Unique<vuk::Buffer> m_IndiciesBuffer;
  std::string Name;
  std::string Path;
  uint32_t LoadingFlags = 0;
  bool ShouldUpdate = false;

  Mesh() = default;
  Mesh(std::string_view path, int fileLoadingFlags = None, float scale = 1);
  ~Mesh();

  void LoadFromFile(const std::string& path, int fileLoadingFlags = None, float scale = 1);

  void BindVertexBuffer(vuk::CommandBuffer& commandBuffer) const;
  void BindIndexBuffer(vuk::CommandBuffer& commandBuffer) const;
  void Draw(vuk::CommandBuffer& commandBuffer) const;
  void Destroy();

  /// Export a mesh file as glb file.
  static bool ExportAsBinary(const std::string& inPath, const std::string& outPath);

  Ref<Material> GetMaterial(uint32_t index) const;
  std::vector<Ref<Material>> GetMaterialsAsRef() const;
  size_t GetNodeCount() const { return Nodes.size(); }
  void SetScale(const glm::vec3& scale);

  operator bool() const {
    return !Nodes.empty();
  }

private:
  std::vector<Ref<Material>> m_Materials;
  std::vector<uint32_t> m_Indices;
  std::vector<Vertex> m_Vertices;
  Vec3 m_Scale{1.0f};
  Vec3 m_Center{0.0f};
  Vec2 m_UVScale{1.0f};
  void LoadTextures(const tinygltf::Model& model);
  void LoadMaterials(tinygltf::Model& model);
  void LoadNode(Node* parent,
                const tinygltf::Node& node,
                uint32_t nodeIndex,
                tinygltf::Model& model,
                std::vector<uint32_t>& indexBuffer,
                std::vector<Vertex>& vertexBuffer,
                float globalscale);
};
}
