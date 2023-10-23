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
    uint32_t first_index;
    uint32_t index_count;
    uint32_t first_vertex;
    uint32_t vertex_count;

    struct Dimensions {
      Vec3 min = glm::vec3(FLT_MAX);
      Vec3 max = glm::vec3(-FLT_MAX);
      Vec3 size;
      Vec3 center;
      float radius;
    } dimensions;

    int32_t material_index = 0;

    void set_dimensions(glm::vec3 min, glm::vec3 max);
    Primitive(uint32_t firstIndex, uint32_t indexCount) : first_index(firstIndex), index_count(indexCount) { }
  };

  struct Node {
    Node* parent;
    uint32_t index;
    uint32_t mesh_index;
    std::vector<Node*> children;
    std::vector<Primitive*> primitives;
    Mat4 matrix;
    std::string name;
    int32_t skin_index = -1;
    Vec3 translation{};
    Vec3 scale{1.0f};
    Quat rotation{};
    bool contains_mesh = false;
    Mat4 local_matrix() const;
    Mat4 get_matrix() const;
  };

  struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec2 uv;
    Vec4 tangent;
    Vec4 color;
    Vec4 joint0;
    Vec4 weight0;
  };

  std::vector<Ref<TextureAsset>> m_textures;
  std::vector<Node*> nodes;
  std::vector<Node*> linear_nodes;
  vuk::Unique<vuk::Buffer> verticies_buffer;
  vuk::Unique<vuk::Buffer> indicies_buffer;
  std::string name;
  std::string path;
  uint32_t loading_flags = 0;

  Mesh() = default;
  Mesh(std::string_view path, int file_loading_flags = None, float scale = 1);
  ~Mesh();

  void load_from_file(const std::string& file_path, int file_loading_flags = None, float scale = 1);

  void bind_vertex_buffer(vuk::CommandBuffer& commandBuffer) const;
  void bind_index_buffer(vuk::CommandBuffer& command_buffer) const;
  void draw(vuk::CommandBuffer& command_buffer) const;
  void destroy();

  /// Export a mesh file as glb file.
  static bool export_as_binary(const std::string& inPath, const std::string& outPath);

  Ref<Material> get_material(uint32_t index) const;
  std::vector<Ref<Material>> get_materials_as_ref() const;
  size_t get_node_count() const { return nodes.size(); }
  void set_scale(const glm::vec3& mesh_scale);

  operator bool() const {
    return !nodes.empty();
  }

private:
  std::vector<Ref<Material>> materials;
  std::vector<uint32_t> indices;
  std::vector<Vertex> vertices;
  Vec3 scale{1.0f};
  Vec3 center{0.0f};
  Vec2 uv_scale{1.0f};
  void load_textures(tinygltf::Model& model);
  void load_materials(tinygltf::Model& model);
  void load_node(Node* parent,
                const tinygltf::Node& node,
                uint32_t node_index,
                tinygltf::Model& model,
                std::vector<uint32_t>& index_buffer,
                std::vector<Vertex>& vertex_buffer,
                float globalscale);
};
}
