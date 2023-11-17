#include "Mesh.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define TINYGLTF_USE_RAPIDJSON
#define TINYGLTF_NO_INCLUDE_RAPIDJSON
// includes for tinygltf
#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/rapidjson.h>
#include <rapidjson/stringbuffer.h>
#include <rapidjson/writer.h>

#include <tiny_gltf.h>

#include <glm/gtc/type_ptr.hpp>
#include <vuk/CommandBuffer.hpp>
#include <vuk/Partials.hpp>

#include "Texture.h"
#include "Assets/AssetManager.h"

#include <spdlog/stopwatch.h>
#include <spdlog/fmt/chrono.h>

#include "Core/Components.h"
#include "Core/Entity.h"

#include "Scene/Scene.h"

#include "Utils/Log.h"
#include "Utils/Profiler.h"
#include "Vulkan/VulkanContext.h"

namespace Oxylus {
Mesh::Mesh(const std::string_view path, const int file_loading_flags, const float scale) {
  load_from_file(path.data(), file_loading_flags, scale);
}

Mesh::Mesh(const std::vector<Vertex>& vertices, const std::vector<uint32_t>& indices) {
  this->vertices = vertices;
  this->indices = indices;
  this->aabb = {};

  const auto context = VulkanContext::get();
  auto compiler = vuk::Compiler{};

  auto [vBuffer, vBufferFut] = create_buffer(*context->superframe_allocator, vuk::MemoryUsage::eGPUonly, vuk::DomainFlagBits::eTransferOnGraphics, std::span(this->vertices));

  vBufferFut.wait(*context->superframe_allocator, compiler);
  vertex_buffer = std::move(vBuffer);

  auto [iBuffer, iBufferFut] = create_buffer(*context->superframe_allocator, vuk::MemoryUsage::eGPUonly, vuk::DomainFlagBits::eTransferOnGraphics, std::span(this->indices));

  iBufferFut.wait(*context->superframe_allocator, compiler);
  index_buffer = std::move(iBuffer);
}

Mesh::~Mesh() {
  destroy();
}

bool Mesh::export_as_binary(const std::string& inPath, const std::string& outPath) {
  tinygltf::TinyGLTF gltfContext;
  tinygltf::Model gltfModel;
  std::string error, warning;

  bool fileLoaded;
  if (std::filesystem::path(inPath).extension() == ".gltf")
    fileLoaded = gltfContext.LoadASCIIFromFile(&gltfModel, &error, &warning, inPath);
  else
    fileLoaded = gltfContext.LoadBinaryFromFile(&gltfModel, &error, &warning, inPath);

  if (!fileLoaded) {
    OX_CORE_ERROR("Couldnt load gltf file: {}", error);
    return false;
  }
  if (!warning.empty()) {
    OX_CORE_WARN("GLTF loader warning: {}", warning);
  }

  return gltfContext.WriteGltfSceneToFile(&gltfModel, outPath, true, true, false, true);
}

void Mesh::load_from_file(const std::string& file_path, int file_loading_flags, const float scale) {
  OX_SCOPED_ZONE;
  spdlog::stopwatch sw;

  path = file_path;
  loading_flags = file_loading_flags;

  tinygltf::Model gltf_model;
  tinygltf::TinyGLTF gltf_context;
  gltf_context.SetImageLoader(tinygltf::LoadImageData, this);

  std::string error, warning;
  bool file_loaded = false;
  if (std::filesystem::path(file_path).extension() == ".gltf") {
    file_loaded = gltf_context.LoadASCIIFromFile(&gltf_model, &error, &warning, file_path);
  }
  else {
    file_loaded = gltf_context.LoadBinaryFromFile(&gltf_model, &error, &warning, file_path);
  }

  if (!file_loaded) {
    OX_CORE_ERROR("Couldnt load gltf file: {}", error);
    return;
  }
  if (!warning.empty()) {
    OX_CORE_WARN("GLTF loader warning: {}", warning);
  }

  load_textures(gltf_model);
  load_materials(gltf_model);

  const tinygltf::Scene& scene = gltf_model.scenes[gltf_model.defaultScene > -1 ? gltf_model.defaultScene : 0];

  name = scene.name;

  for (int node_index : scene.nodes) {
    const tinygltf::Node node = gltf_model.nodes[node_index];
    load_node(nullptr, node, node_index, gltf_model, indices, vertices, scale);
  }

  if (!gltf_model.animations.empty()) {
    load_animations(gltf_model);
  }

  load_skins(gltf_model);

  for (auto node : linear_nodes) {
    // Assign skins
    if (node->skin_index > -1) {
      node->skin = skins[node->skin_index];
    }
    // Initial pose
    if (node->mesh_data) {
      node->update();
    }
  }

  auto context = VulkanContext::get();
  auto compiler = vuk::Compiler{};

  auto [vBuffer, vBufferFut] = create_buffer(*context->superframe_allocator, vuk::MemoryUsage::eGPUonly, vuk::DomainFlagBits::eTransferOnGraphics, std::span(vertices));

  vBufferFut.wait(*context->superframe_allocator, compiler);
  vertex_buffer = std::move(vBuffer);

  auto [iBuffer, iBufferFut] = create_buffer(*context->superframe_allocator, vuk::MemoryUsage::eGPUonly, vuk::DomainFlagBits::eTransferOnGraphics, std::span(indices));

  iBufferFut.wait(*context->superframe_allocator, compiler);
  index_buffer = std::move(iBuffer);

  m_textures.clear();

  OX_CORE_INFO("Mesh file loaded: ({}) {}, {} materials, {} animations", duration_cast<std::chrono::milliseconds>(sw.elapsed()), name.c_str(), gltf_model.materials.size(), animations.size());
}

void Mesh::bind_vertex_buffer(vuk::CommandBuffer& command_buffer) const {
  OX_SCOPED_ZONE;
  OX_CORE_ASSERT(vertex_buffer)
  // TODO: We should only bind the animation data if necessary etc.
  command_buffer.bind_vertex_buffer(0,
    *vertex_buffer,
    0,
    vuk::Packed{
      vuk::Format::eR32G32B32Sfloat,    // 12 postition
      vuk::Format::eR32G32B32Sfloat,    // 12 normal
      vuk::Format::eR32G32Sfloat,       // 8  uv
      vuk::Format::eR32G32B32A32Sfloat, // 16 tangent
      vuk::Format::eR32G32B32A32Sfloat, // 16 color
      vuk::Format::eR32G32B32A32Sfloat, // 16 joint
      vuk::Format::eR32G32B32A32Sfloat, // 16 weight
    });
}

void Mesh::bind_index_buffer(vuk::CommandBuffer& command_buffer) const {
  OX_SCOPED_ZONE;
  OX_CORE_ASSERT(index_buffer)
  command_buffer.bind_index_buffer(*index_buffer, vuk::IndexType::eUint32);
}

void Mesh::set_scale(const Vec3& mesh_scale) {
  scale = mesh_scale;

  //TODO:
}

void Mesh::draw_node(const Node* node, vuk::CommandBuffer& commandBuffer) const {
  if (node->mesh_data) {
    for (const auto primitive : node->mesh_data->primitives) {
      commandBuffer.draw_indexed(primitive->index_count, 1, primitive->first_index, 0, 0);
    }
  }
  for (const auto& child : node->children) {
    draw_node(child, commandBuffer);
  }
}

void Mesh::draw(vuk::CommandBuffer& command_buffer) const {
  bind_vertex_buffer(command_buffer);
  bind_index_buffer(command_buffer);
  for (const auto node : nodes) {
    draw_node(node, command_buffer);
  }
}

void Mesh::load_textures(tinygltf::Model& model) {
  m_textures.reserve(model.images.size());
  uint32_t index = 0;
  for (auto& img : model.images) {
    if (img.name.empty())
      img.name = std::to_string(UUID());

    unsigned char* buffer;
    bool deleteBuffer = false;
    // Most devices don't support RGB only on Vulkan so convert if necessary
    if (img.component == 3) {
      buffer = Texture::convert_to_four_channels(img.width, img.height, &img.image[0]);
      deleteBuffer = true;
    }
    else {
      buffer = &img.image[0];
    }

    m_textures.emplace_back(AssetManager::get_texture_asset(img.name,
      TextureLoadInfo{{}, (uint32_t)img.width, (uint32_t)img.height, buffer}));

    if (deleteBuffer)
      delete[] buffer;

    index += 1;
  }
}

void Mesh::load_materials(tinygltf::Model& model) {
  OX_SCOPED_ZONE;
  // Create a empty material if the mesh file doesn't have any.
  if (model.materials.empty()) {
    materials.emplace_back(create_ref<Material>());
    const bool dontCreateMaterials = loading_flags & FileLoadingFlags::DontCreateMaterials;
    if (!dontCreateMaterials)
      materials[0]->create();
    return;
  }

  for (tinygltf::Material& mat : model.materials) {
    Material material;
    material.create();
    if (!mat.name.empty())
      material.name = mat.name;
    material.parameters.double_sided = mat.doubleSided;
    if (mat.values.contains("baseColorTexture")) {
      material.albedo_texture = m_textures.at(model.textures[mat.values["baseColorTexture"].TextureIndex()].source);
      material.parameters.use_albedo = true;
    }
    if (mat.values.contains("metallicRoughnessTexture")) {
      material.metallic_roughness_texture = m_textures.at(model.textures[mat.values["metallicRoughnessTexture"].TextureIndex()].source);
      material.parameters.use_physical_map = true;
    }
    if (mat.values.contains("roughnessFactor")) {
      material.parameters.roughness = static_cast<float>(mat.values["roughnessFactor"].Factor());
    }
    if (mat.values.contains("metallicFactor")) {
      material.parameters.metallic = static_cast<float>(mat.values["metallicFactor"].Factor());
    }
    if (mat.values.contains("specularFactor")) {
      material.parameters.specular = static_cast<float>(mat.values["specularFactor"].Factor());
    }
    if (mat.values.contains("baseColorFactor")) {
      material.parameters.color = glm::make_vec4(mat.values["baseColorFactor"].ColorFactor().data());
    }
    if (mat.additionalValues.contains("emissiveFactor")) {
      material.parameters.emmisive = glm::vec4(glm::make_vec3(mat.additionalValues["emissiveFactor"].ColorFactor().data()), 1.0);
    }
    if (mat.additionalValues.contains("normalTexture")) {
      material.normal_texture = m_textures.at(model.textures[mat.additionalValues["normalTexture"].TextureIndex()].source);
      material.parameters.use_normal = true;
    }
    if (mat.additionalValues.contains("emissiveTexture")) {
      material.emissive_texture = m_textures.at(model.textures[mat.additionalValues["emissiveTexture"].TextureIndex()].source);
      material.parameters.use_emissive = true;
    }
    if (mat.additionalValues.contains("occlusionTexture")) {
      material.ao_texture = m_textures.at(model.textures[mat.additionalValues["occlusionTexture"].TextureIndex()].source);
    }
    if (mat.alphaMode == "BLEND") {
      material.parameters.alpha_mode = (uint32_t)Material::AlphaMode::Blend;
    }
    else if (mat.alphaMode == "MASK") {
      material.parameters.alpha_cutoff = (float)mat.alphaCutoff;
      material.parameters.alpha_mode = (uint32_t)Material::AlphaMode::Mask;
    }
    if (mat.additionalValues.contains("alphaCutoff")) {
      material.parameters.alpha_cutoff = static_cast<float>(mat.additionalValues["alphaCutoff"].Factor());
    }
    if (mat.extensions.contains("KHR_materials_ior")) {
      const auto ext = mat.extensions.find("KHR_materials_ior");
      if (ext->second.Has("ior")) {
        auto factor = ext->second.Get("ior");
        const auto value = (float)factor.Get<double>();
        material.parameters.specular = (float)std::pow((value - 1) / (value + 1), 2);
      }
    }

    materials.emplace_back(create_ref<Material>(material));
  }
}

Ref<Material> Mesh::get_material(const uint32_t index) const {
  OX_CORE_ASSERT(index < materials.size())
  return materials.at(index);
}

std::vector<Ref<Material>> Mesh::get_materials_as_ref() const {
  OX_SCOPED_ZONE;
  return materials;
}

void Mesh::destroy() {
  OX_SCOPED_ZONE;
  for (const auto node : nodes) {
    delete node;
  }
  for (const auto skin : skins) {
    delete skin;
  }
  skins.clear();
  linear_nodes.clear();
  nodes.clear();
  materials.clear();
}

void Mesh::calculate_bounding_box(Node* node, Node* parent) {
  BoundingBox parentBvh = parent ? parent->bvh : BoundingBox(dimensions.min, dimensions.max);

  if (node->mesh_data) {
    if (node->mesh_data->bb.valid) {
      node->aabb = node->mesh_data->bb.get_aabb(node->get_matrix());
      if (node->children.empty()) {
        node->bvh.min = node->aabb.min;
        node->bvh.max = node->aabb.max;
        node->bvh.valid = true;
      }
    }
  }

  parentBvh.min = glm::min(parentBvh.min, node->bvh.min);
  parentBvh.max = glm::min(parentBvh.max, node->bvh.max);

  for (const auto& child : node->children) {
    calculate_bounding_box(child, node);
  }
}

void Mesh::get_scene_dimensions() {
  // Calculate binary volume hierarchy for all nodes in the scene
  for (const auto node : linear_nodes) {
    calculate_bounding_box(node, nullptr);
  }

  dimensions.min = glm::vec3(FLT_MAX);
  dimensions.max = glm::vec3(-FLT_MAX);

  for (const auto node : linear_nodes) {
    if (node->bvh.valid) {
      dimensions.min = glm::min(dimensions.min, node->bvh.min);
      dimensions.max = glm::max(dimensions.max, node->bvh.max);
    }
  }

  // Calculate scene aabb
  aabb = glm::scale(glm::mat4(1.0f), glm::vec3(dimensions.max[0] - dimensions.min[0], dimensions.max[1] - dimensions.min[1], dimensions.max[2] - dimensions.min[2]));
  aabb[3][0] = dimensions.min[0];
  aabb[3][1] = dimensions.min[1];
  aabb[3][2] = dimensions.min[2];
}

void Mesh::update_animation(uint32_t index, const float time) const {
  if (animations.empty()) {
    OX_CORE_WARN("Mesh does not contain animation!");
    return;
  }
  if (index > static_cast<uint32_t>(animations.size()) - 1) {
    OX_CORE_WARN("No animation with index {} !", index);
    return;
  }

  const Ref<Animation>& animation = animations[index];

  bool updated = false;
  for (const auto& channel : animation->channels) {
    AnimationSampler& sampler = animation->samplers[channel.samplerIndex];
    if (sampler.inputs.size() > sampler.outputs_vec4.size()) {
      continue;
    }

    for (size_t i = 0; i < sampler.inputs.size() - 1; i++) {
      if ((time >= sampler.inputs[i]) && (time <= sampler.inputs[i + 1])) {
        const float u = std::max(0.0f, time - sampler.inputs[i]) / (sampler.inputs[i + 1] - sampler.inputs[i]);
        if (u <= 1.0f) {
          switch (channel.path) {
            case AnimationChannel::PathType::TRANSLATION: {
              glm::vec4 trans = glm::mix(sampler.outputs_vec4[i], sampler.outputs_vec4[i + 1], u);
              channel.node->translation = glm::vec3(trans);
              break;
            }
            case AnimationChannel::PathType::SCALE: {
              glm::vec4 trans = glm::mix(sampler.outputs_vec4[i], sampler.outputs_vec4[i + 1], u);
              channel.node->scale = glm::vec3(trans);
              break;
            }
            case AnimationChannel::PathType::ROTATION: {
              glm::quat q1;
              q1.x = sampler.outputs_vec4[i].x;
              q1.y = sampler.outputs_vec4[i].y;
              q1.z = sampler.outputs_vec4[i].z;
              q1.w = sampler.outputs_vec4[i].w;
              glm::quat q2;
              q2.x = sampler.outputs_vec4[i + 1].x;
              q2.y = sampler.outputs_vec4[i + 1].y;
              q2.z = sampler.outputs_vec4[i + 1].z;
              q2.w = sampler.outputs_vec4[i + 1].w;
              channel.node->rotation = glm::normalize(glm::slerp(q1, q2, u));
              break;
            }
          }
          updated = true;
        }
      }
    }
  }
  if (updated) {
    for (const auto node : nodes) {
      node->update();
    }
  }
}

Mesh::MeshData::MeshData(): bb(), aabb() {
  node_buffer = *vuk::allocate_buffer(*VulkanContext::get()->superframe_allocator, {vuk::MemoryUsage::eCPUtoGPU, sizeof(uniform_block), 1});
}

Mesh::MeshData::~MeshData() {
  for (const auto p : primitives)
    delete p;
}

void Mesh::MeshData::set_bounding_box(const glm::vec3 min, const glm::vec3 max) {
  bb.min = min;
  bb.max = max;
  bb.valid = true;
}

Mesh::BoundingBox Mesh::BoundingBox::get_aabb(glm::mat4 m) const {
  auto min = glm::vec3(m[3]);
  glm::vec3 max = min;
  glm::vec3 v0, v1;

  auto right = glm::vec3(m[0]);
  v0 = right * this->min.x;
  v1 = right * this->max.x;
  min += glm::min(v0, v1);
  max += glm::max(v0, v1);

  auto up = glm::vec3(m[1]);
  v0 = up * this->min.y;
  v1 = up * this->max.y;
  min += glm::min(v0, v1);
  max += glm::max(v0, v1);

  auto back = glm::vec3(m[2]);
  v0 = back * this->min.z;
  v1 = back * this->max.z;
  min += glm::min(v0, v1);
  max += glm::max(v0, v1);

  return {min, max};
}

void Mesh::Primitive::set_bounding_box(const glm::vec3 min, const glm::vec3 max) {
  bb.min = min;
  bb.max = max;
  bb.valid = true;
}

Mesh::Node::~Node() {
  delete mesh_data;
  for (const auto& child : children) {
    delete child;
  }
}

Mat4 Mesh::Node::local_matrix() const {
  OX_SCOPED_ZONE;
  return glm::translate(Mat4(1.0f), translation) * Mat4(rotation) * glm::scale(Mat4(1.0f), scale) * transform;
}

Mat4 Mesh::Node::get_matrix() const {
  OX_SCOPED_ZONE;
  glm::mat4 m = local_matrix();
  auto p = parent;
  while (p) {
    m = p->local_matrix() * m;
    p = p->parent;
  }
  return m;
}

void Mesh::Node::update() const {
  if (mesh_data) {
    if (skin) {
      const glm::mat4 m = get_matrix();
      // Update join matrices
      const glm::mat4 inverse_transform = glm::inverse(m);
      const size_t num_joints = std::min((uint32_t)skin->joints.size(), MAX_NUM_JOINTS);
      for (size_t i = 0; i < num_joints; i++) {
        const Node* joint_node = skin->joints[i];
        glm::mat4 joint_mat = joint_node->get_matrix() * skin->inverse_bind_matrices[i];
        joint_mat = inverse_transform * joint_mat;
        mesh_data->uniform_block.joint_matrix[i] = joint_mat;
      }
      mesh_data->uniform_block.jointcount = (float)num_joints;
      memcpy(mesh_data->node_buffer->mapped_ptr, &mesh_data->uniform_block, sizeof(mesh_data->uniform_block));
    }
  }

  for (const auto& child : children) {
    child->update();
  }
}

void Mesh::load_node(Node* parent,
                     const tinygltf::Node& node,
                     uint32_t node_index,
                     tinygltf::Model& model,
                     std::vector<uint32_t>& ibuffer,
                     std::vector<Vertex>& vbuffer,
                     float globalscale) {
  OX_SCOPED_ZONE;
  Node* new_node = new Node{};
  new_node->index = node_index;
  new_node->parent = parent;
  new_node->name = node.name;
  new_node->skin_index = node.skin;
  new_node->scale = Vec3(globalscale);
  new_node->transform = glm::identity<Mat4>();

  // Generate local node matrix
  Vec3 translation = Vec3(0.0f);
  if (node.translation.size() == 3) {
    translation = glm::make_vec3(node.translation.data());
    new_node->translation = translation;
  }
  if (node.rotation.size() == 4) {
    glm::quat q = glm::make_quat(node.rotation.data());
    new_node->rotation = Mat4(q);
  }
  Vec3 scale = Vec3(globalscale);
  if (node.scale.size() == 3) {
    scale = glm::make_vec3(node.scale.data());
    new_node->scale = scale;
  }
  if (node.matrix.size() == 16) {
    new_node->transform = glm::make_mat4x4(node.matrix.data());
    if (globalscale != 1.0f) { new_node->transform = glm::scale(new_node->transform, Vec3(globalscale)); }
  }

  // Node with children
  if (!node.children.empty()) {
    for (int children : node.children) {
      load_node(new_node, model.nodes[children], children, model, ibuffer, vbuffer, globalscale);
    }
  }

  // Node contains mesh data
  if (node.mesh > -1) {
    const tinygltf::Mesh mesh = model.meshes[node.mesh];
    auto new_mesh = new MeshData();
    new_node->mesh_index = node.mesh;
    for (const auto& primitive : mesh.primitives) {
      auto index_start = (uint32_t)ibuffer.size();
      auto vertex_start = (uint32_t)vbuffer.size();
      uint32_t index_count = 0;
      uint32_t vertex_count = 0;
      glm::vec3 posMin{};
      glm::vec3 posMax{};
      bool has_skin = false;
      bool has_indices = primitive.indices > -1;
      // Vertices
      {
        const float* buffer_pos = nullptr;
        const float* buffer_normals = nullptr;
        const float* buffer_tex_coord_set0 = nullptr;
        const float* buffer_color_set0 = nullptr;
        const void* buffer_joints = nullptr;
        const float* buffer_weights = nullptr;
        const float* buffer_tangents = nullptr;

        int pos_byte_stride = 0;
        int norm_byte_stride = 0;
        int uv0_byte_stride = 0;
        int color0_byte_stride = 0;
        int joint_byte_stride = 0;
        int weight_byte_stride = 0;

        int joint_component_type = 0;

        // Position attribute is required
        OX_CORE_ASSERT(primitive.attributes.contains("POSITION"))

        const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.find("POSITION")->second];
        const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
        buffer_pos = reinterpret_cast<const float*>(&model.buffers[posView.buffer].data[posAccessor.byteOffset + posView.byteOffset]);
        posMin = glm::vec3(posAccessor.minValues[0], posAccessor.minValues[1], posAccessor.minValues[2]);
        posMax = glm::vec3(posAccessor.maxValues[0], posAccessor.maxValues[1], posAccessor.maxValues[2]);
        vertex_count = static_cast<uint32_t>(posAccessor.count);
        pos_byte_stride = posAccessor.ByteStride(posView) ? posAccessor.ByteStride(posView) / sizeof(float) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);

        if (primitive.attributes.contains("NORMAL")) {
          const tinygltf::Accessor& normAccessor = model.accessors[primitive.attributes.find("NORMAL")->second];
          const tinygltf::BufferView& normView = model.bufferViews[normAccessor.bufferView];
          buffer_normals = reinterpret_cast<const float*>(&model.buffers[normView.buffer].data[normAccessor.byteOffset + normView.byteOffset]);
          norm_byte_stride = normAccessor.ByteStride(normView) ? normAccessor.ByteStride(normView) / sizeof(float) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
        }

        // UVs
        if (primitive.attributes.contains("TEXCOORD_0")) {
          const tinygltf::Accessor& uvAccessor = model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
          const tinygltf::BufferView& uvView = model.bufferViews[uvAccessor.bufferView];
          buffer_tex_coord_set0 = reinterpret_cast<const float*>(&model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]);
          uv0_byte_stride = uvAccessor.ByteStride(uvView) ? uvAccessor.ByteStride(uvView) / sizeof(float) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC2);
        }

        // Vertex colors
        if (primitive.attributes.contains("COLOR_0")) {
          const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.find("COLOR_0")->second];
          const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
          buffer_color_set0 = reinterpret_cast<const float*>(&model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]);
          color0_byte_stride = accessor.ByteStride(view) ? accessor.ByteStride(view) / sizeof(float) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
        }

        if (primitive.attributes.contains("TANGENT")) {
          const tinygltf::Accessor& tangentAccessor = model.accessors[primitive.attributes.find("TANGENT")->second];
          const tinygltf::BufferView& tangentView = model.bufferViews[tangentAccessor.bufferView];
          buffer_tangents = reinterpret_cast<const float*>(&model.buffers[tangentView.buffer].data[tangentAccessor.byteOffset + tangentView.byteOffset]);
        }

        // Skinning
        // Joints
        if (primitive.attributes.contains("JOINTS_0")) {
          const tinygltf::Accessor& jointAccessor = model.accessors[primitive.attributes.find("JOINTS_0")->second];
          const tinygltf::BufferView& jointView = model.bufferViews[jointAccessor.bufferView];
          buffer_joints = &(model.buffers[jointView.buffer].data[jointAccessor.byteOffset + jointView.byteOffset]);
          joint_component_type = jointAccessor.componentType;
          joint_byte_stride = jointAccessor.ByteStride(jointView) ? jointAccessor.ByteStride(jointView) / tinygltf::GetComponentSizeInBytes(joint_component_type) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4);
        }

        if (primitive.attributes.contains("WEIGHTS_0")) {
          const tinygltf::Accessor& weightAccessor = model.accessors[primitive.attributes.find("WEIGHTS_0")->second];
          const tinygltf::BufferView& weightView = model.bufferViews[weightAccessor.bufferView];
          buffer_weights = reinterpret_cast<const float*>(&(model.buffers[weightView.buffer].data[weightAccessor.byteOffset + weightView.byteOffset]));
          weight_byte_stride = weightAccessor.ByteStride(weightView) ? weightAccessor.ByteStride(weightView) / sizeof(float) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4);
        }

        has_skin = (buffer_joints && buffer_weights);

        for (size_t v = 0; v < posAccessor.count; v++) {
          Vertex vert = {};
          vert.position = glm::vec4(glm::make_vec3(&buffer_pos[v * pos_byte_stride]), 1.0f);
          vert.normal = glm::normalize(glm::vec3(buffer_normals ? glm::make_vec3(&buffer_normals[v * norm_byte_stride]) : glm::vec3(0.0f)));
          vert.uv = buffer_tex_coord_set0 ? glm::make_vec2(&buffer_tex_coord_set0[v * uv0_byte_stride]) : glm::vec3(0.0f);
          vert.color = buffer_color_set0 ? glm::make_vec4(&buffer_color_set0[v * color0_byte_stride]) : glm::vec4(1.0f);
          Vec3 c1 = glm::cross(vert.normal, Vec3(0, 0, 1));
          Vec3 c2 = glm::cross(vert.normal, Vec3(0, 1, 0));
          Vec3 tangent = glm::dot(c1, c1) > glm::dot(c2, c2) ? c1 : c2;
          vert.tangent = buffer_tangents ? glm::vec4(glm::make_vec4(&buffer_tangents[v * 4])) : Vec4(tangent, 0.0);

          if (has_skin) {
            switch (joint_component_type) {
              case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                const auto* buf = static_cast<const uint16_t*>(buffer_joints);
                vert.joint0 = glm::vec4(glm::make_vec4(&buf[v * joint_byte_stride]));
                break;
              }
              case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                const auto* buf = static_cast<const uint8_t*>(buffer_joints);
                vert.joint0 = glm::vec4(glm::make_vec4(&buf[v * joint_byte_stride]));
                break;
              }
              default:
                // Not supported by spec
                OX_CORE_ERROR("Joint component type {} {}", joint_component_type, " not supported!");
                break;
            }
          }
          else {
            vert.joint0 = glm::vec4(0.0f);
          }
          vert.weight0 = has_skin ? glm::make_vec4(&buffer_weights[v * weight_byte_stride]) : glm::vec4(0.0f);
          // Fix for all zero weights
          if (glm::length(vert.weight0) == 0.0f) {
            vert.weight0 = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
          }
          vbuffer.emplace_back(vert);
        }
      }
      // Indices
      if (has_indices) {
        const tinygltf::Accessor& accessor = model.accessors[primitive.indices > -1 ? primitive.indices : 0];
        const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

        index_count = static_cast<uint32_t>(accessor.count);

        switch (accessor.componentType) {
          case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
            auto* buf = new uint32_t[accessor.count];
            memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint32_t));
            for (size_t index = 0; index < accessor.count; index++) {
              ibuffer.emplace_back(buf[index] + vertex_start);
            }
            delete[] buf;
            break;
          }
          case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
            auto* buf = new uint16_t[accessor.count];
            memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint16_t));
            for (size_t index = 0; index < accessor.count; index++) {
              ibuffer.emplace_back(buf[index] + vertex_start);
            }
            delete[] buf;
            break;
          }
          case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
            auto* buf = new uint8_t[accessor.count];
            memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint8_t));
            for (size_t index = 0; index < accessor.count; index++) {
              ibuffer.emplace_back(buf[index] + vertex_start);
            }
            delete[] buf;
            break;
          }
          default: OX_CORE_ERROR("Index component type {0} not supported", accessor.componentType);
            return;
        }
      }

      auto* new_primitive = new Primitive(index_start, index_count, vertex_count, vertex_start);
      new_primitive->material_index = primitive.material;
      if (primitive.material < 0)
        new_primitive->material_index = 0;
      new_primitive->set_bounding_box(posMin, posMax);
      new_primitive->parent_node_index = node_index;
      new_mesh->primitives.push_back(new_primitive);
    }
    // Mesh BB from BBs of primitives
    for (auto& p : new_mesh->primitives) {
      if (p->bb.valid && !new_mesh->bb.valid) {
        new_mesh->bb = p->bb;
        new_mesh->bb.valid = true;
      }
      new_mesh->bb.min = glm::min(new_mesh->bb.min, p->bb.min);
      new_mesh->bb.max = glm::max(new_mesh->bb.max, p->bb.max);
    }
    new_node->mesh_data = new_mesh;
  }
  if (parent) {
    parent->children.emplace_back(new_node);
  }
  else {
    nodes.emplace_back(new_node);
  }
  linear_nodes.emplace_back(new_node);
}

void Mesh::load_animations(tinygltf::Model& gltf_model) {
  for (tinygltf::Animation& anim : gltf_model.animations) {
    Animation animation{};
    animation.name = anim.name;
    if (anim.name.empty()) {
      animation.name = std::to_string(animations.size());
    }

    // Samplers
    for (auto& samp : anim.samplers) {
      AnimationSampler sampler{};

      if (samp.interpolation == "LINEAR") {
        sampler.interpolation = AnimationSampler::InterpolationType::LINEAR;
      }
      if (samp.interpolation == "STEP") {
        sampler.interpolation = AnimationSampler::InterpolationType::STEP;
      }
      if (samp.interpolation == "CUBICSPLINE") {
        sampler.interpolation = AnimationSampler::InterpolationType::CUBICSPLINE;
      }

      // Read sampler input time values
      {
        const tinygltf::Accessor& accessor = gltf_model.accessors[samp.input];
        const tinygltf::BufferView& bufferView = gltf_model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = gltf_model.buffers[bufferView.buffer];

        OX_CORE_ASSERT(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

        const void* dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];
        const auto* buf = static_cast<const float*>(dataPtr);
        for (size_t index = 0; index < accessor.count; index++) {
          sampler.inputs.emplace_back(buf[index]);
        }

        for (const auto input : sampler.inputs) {
          if (input < animation.start) {
            animation.start = input;
          }
          if (input > animation.end) {
            animation.end = input;
          }
        }
      }

      // Read sampler output T/R/S values 
      {
        const tinygltf::Accessor& accessor = gltf_model.accessors[samp.output];
        const tinygltf::BufferView& bufferView = gltf_model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = gltf_model.buffers[bufferView.buffer];

        assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

        const void* dataPtr = &buffer.data[accessor.byteOffset + bufferView.byteOffset];

        switch (accessor.type) {
          case TINYGLTF_TYPE_VEC3: {
            const auto* buf = static_cast<const glm::vec3*>(dataPtr);
            for (size_t index = 0; index < accessor.count; index++) {
              sampler.outputs_vec4.emplace_back(glm::vec4(buf[index], 0.0f));
            }
            break;
          }
          case TINYGLTF_TYPE_VEC4: {
            const auto* buf = static_cast<const glm::vec4*>(dataPtr);
            for (size_t index = 0; index < accessor.count; index++) {
              sampler.outputs_vec4.emplace_back(buf[index]);
            }
            break;
          }
          default: {
            OX_CORE_WARN("unknown type");
            break;
          }
        }
      }

      animation.samplers.emplace_back(sampler);
    }

    // Channels
    for (auto& source : anim.channels) {
      AnimationChannel channel{};

      if (source.target_path == "rotation") {
        channel.path = AnimationChannel::PathType::ROTATION;
      }
      if (source.target_path == "translation") {
        channel.path = AnimationChannel::PathType::TRANSLATION;
      }
      if (source.target_path == "scale") {
        channel.path = AnimationChannel::PathType::SCALE;
      }
      if (source.target_path == "weights") {
        OX_CORE_WARN("weights not yet supported, skipping channel...");
        continue;
      }
      channel.samplerIndex = source.sampler;
      channel.node = node_from_index(source.target_node);
      if (!channel.node) {
        continue;
      }

      animation.channels.emplace_back(channel);
    }

    animations.emplace_back(create_ref<Animation>(std::move(animation)));
  }
}

void Mesh::load_skins(tinygltf::Model& gltf_model) {
  for (tinygltf::Skin& source : gltf_model.skins) {
    Skin* newSkin = new Skin{};
    newSkin->name = source.name;

    // Find skeleton root node
    if (source.skeleton > -1) {
      newSkin->skeleton_root = node_from_index(source.skeleton);
    }

    // Find joint nodes
    for (const int jointIndex : source.joints) {
      const Node* node = node_from_index(jointIndex);
      if (node) {
        newSkin->joints.emplace_back(node_from_index(jointIndex));
      }
    }

    // Get inverse bind matrices from buffer
    if (source.inverseBindMatrices > -1) {
      const tinygltf::Accessor& accessor = gltf_model.accessors[source.inverseBindMatrices];
      const tinygltf::BufferView& bufferView = gltf_model.bufferViews[accessor.bufferView];
      const tinygltf::Buffer& buffer = gltf_model.buffers[bufferView.buffer];
      newSkin->inverse_bind_matrices.resize(accessor.count);
      memcpy(newSkin->inverse_bind_matrices.data(), &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(glm::mat4));
    }

    skins.emplace_back(newSkin);
  }
}

Mesh::Node* Mesh::find_node(Node* parent, const uint32_t index) {
  Node* nodeFound = nullptr;
  if (parent->index == index) {
    return parent;
  }
  for (const auto& child : parent->children) {
    nodeFound = find_node(child, index);
    if (nodeFound) {
      break;
    }
  }
  return nodeFound;
}

Mesh::Node* Mesh::node_from_index(const uint32_t index) {
  Node* nodeFound = nullptr;
  for (const auto& node : nodes) {
    nodeFound = find_node(node, index);
    if (nodeFound) {
      break;
    }
  }
  return nodeFound;
}
}
