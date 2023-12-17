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

#include "Scene/Components.h"
#include "Scene/Entity.h"

#include "Scene/Scene.h"

#include "Utils/Log.h"
#include "Utils/Profiler.h"
#include "Utils/Timer.h"

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

bool Mesh::export_as_binary(const std::string& in_path, const std::string& out_path) {
  tinygltf::TinyGLTF gltf_context;
  tinygltf::Model gltf_model;
  std::string error, warning;

  bool file_loaded;
  if (std::filesystem::path(in_path).extension() == ".gltf")
    file_loaded = gltf_context.LoadASCIIFromFile(&gltf_model, &error, &warning, in_path);
  else
    file_loaded = gltf_context.LoadBinaryFromFile(&gltf_model, &error, &warning, in_path);

  if (!file_loaded) {
    OX_CORE_ERROR("Couldnt load gltf file: {}", error);
    return false;
  }
  if (!warning.empty()) {
    OX_CORE_WARN("GLTF loader warning: {}", warning);
  }

  return gltf_context.WriteGltfSceneToFile(&gltf_model, out_path, true, true, false, true);
}

void Mesh::load_from_file(const std::string& file_path, int file_loading_flags, const float scale) {
  OX_SCOPED_ZONE;
  Timer timer;

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

  get_scene_dimensions();

  auto context = VulkanContext::get();
  auto compiler = vuk::Compiler{};

  auto [vBuffer, vBufferFut] = create_buffer(*context->superframe_allocator, vuk::MemoryUsage::eGPUonly, vuk::DomainFlagBits::eTransferOnGraphics, std::span(vertices));

  vBufferFut.wait(*context->superframe_allocator, compiler);
  vertex_buffer = std::move(vBuffer);

  auto [iBuffer, iBufferFut] = create_buffer(*context->superframe_allocator, vuk::MemoryUsage::eGPUonly, vuk::DomainFlagBits::eTransferOnGraphics, std::span(indices));

  iBufferFut.wait(*context->superframe_allocator, compiler);
  index_buffer = std::move(iBuffer);

  m_textures.clear();

  OX_CORE_INFO("Mesh file loaded: ({}) {}, {} materials, {} animations", timer.get_elapsed_ms(), name.c_str(), gltf_model.materials.size(), animations.size());
}

const Mesh* Mesh::bind_vertex_buffer(vuk::CommandBuffer& command_buffer) const {
  OX_SCOPED_ZONE;
  OX_CORE_ASSERT(vertex_buffer)
  command_buffer.bind_vertex_buffer(0, *vertex_buffer, 0, vertex_pack);
  return this;
}

const Mesh* Mesh::bind_index_buffer(vuk::CommandBuffer& command_buffer) const {
  OX_SCOPED_ZONE;
  OX_CORE_ASSERT(index_buffer)
  command_buffer.bind_index_buffer(*index_buffer, vuk::IndexType::eUint32);
  return this;
}

void Mesh::set_scale(const Vec3& mesh_scale) {
  scale = mesh_scale;

  //TODO:
}

void Mesh::draw_node(const Node* node, vuk::CommandBuffer& command_buffer) const {
  if (node->mesh_data) {
    for (const auto primitive : node->mesh_data->primitives) {
      command_buffer.draw_indexed(primitive->index_count, 1, primitive->first_index, 0, 0);
    }
  }
  for (const auto& child : node->children) {
    draw_node(child, command_buffer);
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
  m_textures.resize(model.images.size());
  for (uint32_t image_index = 0; image_index < model.images.size(); image_index++) {
    auto& img = model.images[image_index];
    if (img.name.empty())
      img.name = std::to_string(UUID());

    unsigned char* buffer;
    bool delete_buffer = false;
    // Most devices don't support RGB only on Vulkan so convert if necessary
    if (img.component == 3) {
      buffer = Texture::convert_to_four_channels(img.width, img.height, &img.image[0]);
      delete_buffer = true;
    }
    else {
      buffer = &img.image[0];
    }

    m_textures[image_index] = AssetManager::get_texture_asset(img.name, TextureLoadInfo{{}, (uint32_t)img.width, (uint32_t)img.height, buffer});

    if (delete_buffer)
      delete[] buffer;
  }
}

void Mesh::load_materials(tinygltf::Model& model) {
  OX_SCOPED_ZONE;
  // Create a empty material if the mesh file doesn't have any.
  if (model.materials.empty()) {
    materials.emplace_back(create_ref<Material>());
    const bool dont_create_materials = loading_flags & DontCreateMaterials;
    if (!dont_create_materials)
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
      material.parameters.reflectance = static_cast<float>(mat.values["specularFactor"].Factor());
    }
    if (mat.values.contains("baseColorFactor")) {
      material.parameters.color = glm::make_vec4(mat.values["baseColorFactor"].ColorFactor().data());
    }
    if (mat.additionalValues.contains("emissiveFactor")) {
      material.parameters.emmisive = Vec4(glm::make_vec3(mat.additionalValues["emissiveFactor"].ColorFactor().data()), 1.0);
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
      material.parameters.use_ao = true;
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
        material.parameters.reflectance = (float)std::pow((value - 1) / (value + 1), 2);
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

void Mesh::calculate_node_bounding_box(Node* node) {
  if (node->mesh_data) {
    node->aabb = node->mesh_data->aabb;
    node->aabb.transform(node->get_matrix());
  }

  for (const auto& child : node->children) {
    calculate_node_bounding_box(child);
  }
}

void Mesh::get_scene_dimensions() {
  // Calculate binary volume hierarchy for all nodes in the scene
  for (const auto node : linear_nodes) {
    calculate_node_bounding_box(node);
  }

  dimensions.min = Vec3(FLT_MAX);
  dimensions.max = Vec3(-FLT_MAX);

  for (const auto node : linear_nodes) {
    if (node->mesh_data) {
      dimensions.min = min(dimensions.min, node->mesh_data->aabb.min);
      dimensions.max = max(dimensions.max, node->mesh_data->aabb.max);
    }
  }

  aabb = AABB(dimensions.min, dimensions.max);
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
              Vec4 trans = mix(sampler.outputs_vec4[i], sampler.outputs_vec4[i + 1], u);
              channel.node->translation = Vec3(trans);
              break;
            }
            case AnimationChannel::PathType::SCALE: {
              Vec4 trans = mix(sampler.outputs_vec4[i], sampler.outputs_vec4[i + 1], u);
              channel.node->scale = Vec3(trans);
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
              channel.node->rotation = normalize(slerp(q1, q2, u));
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

Mesh::MeshData::MeshData() {
  node_buffer = *allocate_buffer(*VulkanContext::get()->superframe_allocator, {vuk::MemoryUsage::eCPUtoGPU, sizeof(uniform_block), 1});
}

Mesh::MeshData::~MeshData() {
  for (const auto p : primitives)
    delete p;
}

void Mesh::Primitive::set_bounding_box(const Vec3 min, const Vec3 max) {
  aabb.min = min;
  aabb.max = max;
}

Mesh::Node::~Node() {
  delete mesh_data;
  for (const auto& child : children) {
    delete child;
  }
}

Mat4 Mesh::Node::local_matrix() const {
  OX_SCOPED_ZONE;
  return translate(Mat4(1.0f), translation) * Mat4(rotation) * glm::scale(Mat4(1.0f), scale) * transform;
}

Mat4 Mesh::Node::get_matrix() const {
  OX_SCOPED_ZONE;
  Mat4 m = local_matrix();
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
      const Mat4 m = get_matrix();
      // Update join matrices
      const Mat4 inverse_transform = inverse(m);
      const size_t num_joints = std::min((uint32_t)skin->joints.size(), MAX_NUM_JOINTS);
      for (size_t i = 0; i < num_joints; i++) {
        const Node* joint_node = skin->joints[i];
        Mat4 joint_mat = joint_node->get_matrix() * skin->inverse_bind_matrices[i];
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
      Vec3 pos_min{};
      Vec3 pos_max{};
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

        const tinygltf::Accessor& pos_accessor = model.accessors[primitive.attributes.find("POSITION")->second];
        const tinygltf::BufferView& pos_view = model.bufferViews[pos_accessor.bufferView];
        buffer_pos = reinterpret_cast<const float*>(&model.buffers[pos_view.buffer].data[pos_accessor.byteOffset + pos_view.byteOffset]);
        pos_min = Vec3(pos_accessor.minValues[0], pos_accessor.minValues[1], pos_accessor.minValues[2]);
        pos_max = Vec3(pos_accessor.maxValues[0], pos_accessor.maxValues[1], pos_accessor.maxValues[2]);
        vertex_count = static_cast<uint32_t>(pos_accessor.count);
        pos_byte_stride = pos_accessor.ByteStride(pos_view) ? pos_accessor.ByteStride(pos_view) / sizeof(float) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);

        if (primitive.attributes.contains("NORMAL")) {
          const tinygltf::Accessor& norm_accessor = model.accessors[primitive.attributes.find("NORMAL")->second];
          const tinygltf::BufferView& norm_view = model.bufferViews[norm_accessor.bufferView];
          buffer_normals = reinterpret_cast<const float*>(&model.buffers[norm_view.buffer].data[norm_accessor.byteOffset + norm_view.byteOffset]);
          norm_byte_stride = norm_accessor.ByteStride(norm_view) ? norm_accessor.ByteStride(norm_view) / sizeof(float) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
        }

        // UVs
        if (primitive.attributes.contains("TEXCOORD_0")) {
          const tinygltf::Accessor& uv_accessor = model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
          const tinygltf::BufferView& uv_view = model.bufferViews[uv_accessor.bufferView];
          buffer_tex_coord_set0 = reinterpret_cast<const float*>(&model.buffers[uv_view.buffer].data[uv_accessor.byteOffset + uv_view.byteOffset]);
          uv0_byte_stride = uv_accessor.ByteStride(uv_view) ? uv_accessor.ByteStride(uv_view) / sizeof(float) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC2);
        }

        // Vertex colors
        if (primitive.attributes.contains("COLOR_0")) {
          const tinygltf::Accessor& accessor = model.accessors[primitive.attributes.find("COLOR_0")->second];
          const tinygltf::BufferView& view = model.bufferViews[accessor.bufferView];
          buffer_color_set0 = reinterpret_cast<const float*>(&model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]);
          color0_byte_stride = accessor.ByteStride(view) ? accessor.ByteStride(view) / sizeof(float) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
        }

        if (primitive.attributes.contains("TANGENT")) {
          const tinygltf::Accessor& tangent_accessor = model.accessors[primitive.attributes.find("TANGENT")->second];
          const tinygltf::BufferView& tangent_view = model.bufferViews[tangent_accessor.bufferView];
          buffer_tangents = reinterpret_cast<const float*>(&model.buffers[tangent_view.buffer].data[tangent_accessor.byteOffset + tangent_view.byteOffset]);
        }

        // Skinning
        // Joints
        if (primitive.attributes.contains("JOINTS_0")) {
          const tinygltf::Accessor& joint_accessor = model.accessors[primitive.attributes.find("JOINTS_0")->second];
          const tinygltf::BufferView& joint_view = model.bufferViews[joint_accessor.bufferView];
          buffer_joints = &(model.buffers[joint_view.buffer].data[joint_accessor.byteOffset + joint_view.byteOffset]);
          joint_component_type = joint_accessor.componentType;
          joint_byte_stride = joint_accessor.ByteStride(joint_view) ? joint_accessor.ByteStride(joint_view) / tinygltf::GetComponentSizeInBytes(joint_component_type) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4);
        }

        if (primitive.attributes.contains("WEIGHTS_0")) {
          const tinygltf::Accessor& weight_accessor = model.accessors[primitive.attributes.find("WEIGHTS_0")->second];
          const tinygltf::BufferView& weight_view = model.bufferViews[weight_accessor.bufferView];
          buffer_weights = reinterpret_cast<const float*>(&(model.buffers[weight_view.buffer].data[weight_accessor.byteOffset + weight_view.byteOffset]));
          weight_byte_stride = weight_accessor.ByteStride(weight_view) ? weight_accessor.ByteStride(weight_view) / sizeof(float) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC4);
        }

        has_skin = (buffer_joints && buffer_weights);

        for (size_t v = 0; v < pos_accessor.count; v++) {
          Vertex vert = {};
          vert.position = Vec4(glm::make_vec3(&buffer_pos[v * pos_byte_stride]), 1.0f);
          vert.normal = normalize(Vec3(buffer_normals ? glm::make_vec3(&buffer_normals[v * norm_byte_stride]) : Vec3(0.0f)));
          vert.uv = buffer_tex_coord_set0 ? glm::make_vec2(&buffer_tex_coord_set0[v * uv0_byte_stride]) : Vec3(0.0f);
          vert.color = buffer_color_set0 ? glm::make_vec4(&buffer_color_set0[v * color0_byte_stride]) : Vec4(1.0f);
          Vec3 c1 = cross(vert.normal, Vec3(0, 0, 1));
          Vec3 c2 = cross(vert.normal, Vec3(0, 1, 0));
          Vec3 tangent = dot(c1, c1) > dot(c2, c2) ? c1 : c2;
          vert.tangent = buffer_tangents ? Vec4(glm::make_vec4(&buffer_tangents[v * 4])) : Vec4(tangent, 0.0);

          if (has_skin) {
            switch (joint_component_type) {
              case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
                const auto* buf = static_cast<const uint16_t*>(buffer_joints);
                vert.joint0 = Vec4(glm::make_vec4(&buf[v * joint_byte_stride]));
                break;
              }
              case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
                const auto* buf = static_cast<const uint8_t*>(buffer_joints);
                vert.joint0 = Vec4(glm::make_vec4(&buf[v * joint_byte_stride]));
                break;
              }
              default:
                // Not supported by spec
                OX_CORE_ERROR("Joint component type {} {}", joint_component_type, " not supported!");
                break;
            }
          }
          else {
            vert.joint0 = Vec4(0.0f);
          }
          vert.weight0 = has_skin ? glm::make_vec4(&buffer_weights[v * weight_byte_stride]) : Vec4(0.0f);
          // Fix for all zero weights
          if (length(vert.weight0) == 0.0f) {
            vert.weight0 = Vec4(1.0f, 0.0f, 0.0f, 0.0f);
          }
          vbuffer.emplace_back(vert);
        }
      }
      // Indices
      if (has_indices) {
        const tinygltf::Accessor& accessor = model.accessors[primitive.indices > -1 ? primitive.indices : 0];
        const tinygltf::BufferView& buffer_view = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[buffer_view.buffer];

        index_count = static_cast<uint32_t>(accessor.count);

        switch (accessor.componentType) {
          case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
            auto* buf = new uint32_t[accessor.count];
            memcpy(buf, &buffer.data[accessor.byteOffset + buffer_view.byteOffset], accessor.count * sizeof(uint32_t));
            for (size_t index = 0; index < accessor.count; index++) {
              ibuffer.emplace_back(buf[index] + vertex_start);
            }
            delete[] buf;
            break;
          }
          case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
            auto* buf = new uint16_t[accessor.count];
            memcpy(buf, &buffer.data[accessor.byteOffset + buffer_view.byteOffset], accessor.count * sizeof(uint16_t));
            for (size_t index = 0; index < accessor.count; index++) {
              ibuffer.emplace_back(buf[index] + vertex_start);
            }
            delete[] buf;
            break;
          }
          case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
            auto* buf = new uint8_t[accessor.count];
            memcpy(buf, &buffer.data[accessor.byteOffset + buffer_view.byteOffset], accessor.count * sizeof(uint8_t));
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
      new_primitive->parent_node_index = node_index;

      new_primitive->material_index = primitive.material;
      if (primitive.material < 0)
        new_primitive->material_index = 0;
      new_primitive->material = materials[new_primitive->material_index];

      new_primitive->set_bounding_box(pos_min, pos_max);

      new_mesh->primitives.push_back(new_primitive);

      total_primitive_count += 1;
    }

    new_mesh->aabb = {};

    // Mesh BB from BBs of primitives
    for (auto& p : new_mesh->primitives) {
      new_mesh->aabb.min = min(new_mesh->aabb.min, p->aabb.min);
      new_mesh->aabb.max = max(new_mesh->aabb.max, p->aabb.max);
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
  if (new_node->mesh_data)
    linear_mesh_nodes.emplace_back(new_node);
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
        const tinygltf::BufferView& buffer_view = gltf_model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = gltf_model.buffers[buffer_view.buffer];

        OX_CORE_ASSERT(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

        const void* data_ptr = &buffer.data[accessor.byteOffset + buffer_view.byteOffset];
        const auto* buf = static_cast<const float*>(data_ptr);
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
        const tinygltf::BufferView& buffer_view = gltf_model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = gltf_model.buffers[buffer_view.buffer];

        assert(accessor.componentType == TINYGLTF_COMPONENT_TYPE_FLOAT);

        const void* data_ptr = &buffer.data[accessor.byteOffset + buffer_view.byteOffset];

        switch (accessor.type) {
          case TINYGLTF_TYPE_VEC3: {
            const auto* buf = static_cast<const Vec3*>(data_ptr);
            for (size_t index = 0; index < accessor.count; index++) {
              sampler.outputs_vec4.emplace_back(Vec4(buf[index], 0.0f));
            }
            break;
          }
          case TINYGLTF_TYPE_VEC4: {
            const auto* buf = static_cast<const Vec4*>(data_ptr);
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
    Skin* new_skin = new Skin{};
    new_skin->name = source.name;

    // Find skeleton root node
    if (source.skeleton > -1) {
      new_skin->skeleton_root = node_from_index(source.skeleton);
    }

    // Find joint nodes
    for (const int joint_index : source.joints) {
      const Node* node = node_from_index(joint_index);
      if (node) {
        new_skin->joints.emplace_back(node_from_index(joint_index));
      }
    }

    // Get inverse bind matrices from buffer
    if (source.inverseBindMatrices > -1) {
      const tinygltf::Accessor& accessor = gltf_model.accessors[source.inverseBindMatrices];
      const tinygltf::BufferView& buffer_view = gltf_model.bufferViews[accessor.bufferView];
      const tinygltf::Buffer& buffer = gltf_model.buffers[buffer_view.buffer];
      new_skin->inverse_bind_matrices.resize(accessor.count);
      memcpy(new_skin->inverse_bind_matrices.data(), &buffer.data[accessor.byteOffset + buffer_view.byteOffset], accessor.count * sizeof(Mat4));
    }

    skins.emplace_back(new_skin);
  }
}

Mesh::Node* Mesh::find_node(Node* parent, const uint32_t index) {
  Node* node_found = nullptr;
  if (parent->index == index) {
    return parent;
  }
  for (const auto& child : parent->children) {
    node_found = find_node(child, index);
    if (node_found) {
      break;
    }
  }
  return node_found;
}

Mesh::Node* Mesh::node_from_index(const uint32_t index) {
  Node* node_found = nullptr;
  for (const auto& node : nodes) {
    node_found = find_node(node, index);
    if (node_found) {
      break;
    }
  }
  return node_found;
}
}
