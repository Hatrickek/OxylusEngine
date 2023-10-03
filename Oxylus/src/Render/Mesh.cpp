#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "Mesh.h"

#include <glm/gtc/type_ptr.hpp>
#include <vuk/CommandBuffer.hpp>
#include <vuk/Partials.hpp>

#include "Assets/AssetManager.h"
#include "Utils/Profiler.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/VulkanRenderer.h"
#include "Utils/Log.h"

namespace Oxylus {
Mesh::Mesh(const std::string_view path, int file_loading_flags, float scale) {
  load_from_file(path.data(), file_loading_flags, scale);
}

Mesh::~Mesh() {
  destroy();
}

bool LoadImageDataCallback(tinygltf::Image* image,
                           const int imageIndex,
                           std::string* error,
                           std::string* warning,
                           int req_width,
                           int req_height,
                           const unsigned char* bytes,
                           int size,
                           void* userData) {
  return tinygltf::LoadImageData(image, imageIndex, error, warning, req_width, req_height, bytes, size, userData);
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
  ProfilerTimer timer;

  path = file_path;
  name = std::filesystem::path(file_path).stem().string();
  loading_flags = file_loading_flags;

  tinygltf::Model gltf_model;
  tinygltf::TinyGLTF gltf_context;
  gltf_context.SetImageLoader(LoadImageDataCallback, this);

  std::string error, warning;
  bool fileLoaded = false;
  if (std::filesystem::path(file_path).extension() == ".gltf")
    fileLoaded = gltf_context.LoadASCIIFromFile(&gltf_model, &error, &warning, file_path);
  else
    fileLoaded = gltf_context.LoadBinaryFromFile(&gltf_model, &error, &warning, file_path);
  if (!fileLoaded) {
    OX_CORE_ERROR("Couldnt load gltf file: {}", error);
    return;
  }
  if (!warning.empty())
    OX_CORE_WARN("GLTF loader warning: {}", warning);

  load_textures(gltf_model);
  load_materials(gltf_model);

  const tinygltf::Scene& scene = gltf_model.scenes[gltf_model.defaultScene > -1 ? gltf_model.defaultScene : 0];

  for (int nodeIndex : scene.nodes) {
    const tinygltf::Node node = gltf_model.nodes[nodeIndex];
    load_node(nullptr, node, nodeIndex, gltf_model, indices, vertices, scale);
  }
  //TODO: Skins and animations.

  const bool preMultiplyColor = file_loading_flags & FileLoadingFlags::PreMultiplyVertexColors;
  const bool flipY = file_loading_flags & FileLoadingFlags::FlipY;
  for (auto& node : linear_nodes) {
    if (!node)
      continue;
    const Mat4 localMatrix = node->get_matrix();
    for (const Primitive* primitive : node->primitives) {
      for (uint32_t i = 0; i < primitive->vertex_count; i++) {
        Vertex& vertex = vertices[primitive->first_vertex + i];
        vertex.position = Vec3(localMatrix * glm::vec4(vertex.position, 1.0f));
        vertex.normal = glm::normalize(glm::mat3(localMatrix) * vertex.normal);
        if (flipY) {
          vertex.position.y *= -1.0f;
          vertex.normal.y *= -1.0f;
        }
        if (preMultiplyColor) {
          //vertex.color = primitive->material.baseColorFactor * vertex.color;
        }
      }
    }
  }

  auto context = VulkanContext::get();
  auto compiler = vuk::Compiler{};

  auto [vBuffer, vBufferFut] = create_buffer(*context->superframe_allocator, vuk::MemoryUsage::eGPUonly, vuk::DomainFlagBits::eTransferOnGraphics, std::span(vertices));

  vBufferFut.wait(*context->superframe_allocator, compiler);
  verticies_buffer = std::move(vBuffer);

  auto [iBuffer, iBufferFut] = create_buffer(*context->superframe_allocator, vuk::MemoryUsage::eGPUonly, vuk::DomainFlagBits::eTransferOnGraphics, std::span(indices));

  iBufferFut.wait(*context->superframe_allocator, compiler);
  indicies_buffer = std::move(iBuffer);

  vertices.clear();
  indices.clear();

  m_textures.clear();

  timer.Stop();
  OX_CORE_TRACE("Mesh file loaded: {}, {} materials, {} ms", name.c_str(), gltf_model.materials.size(), timer.ElapsedMilliSeconds());
}

void Mesh::bind_vertex_buffer(vuk::CommandBuffer& commandBuffer) const {
  OX_CORE_ASSERT(verticies_buffer)
  commandBuffer.bind_vertex_buffer(0,
    *verticies_buffer,
    0,
    vuk::Packed{
      vuk::Format::eR32G32B32Sfloat,
      vuk::Format::eR32G32B32Sfloat,
      vuk::Format::eR32G32Sfloat,
      vuk::Format::eR32G32B32A32Sfloat,
      vuk::Ignore{48}
    });
}

void Mesh::bind_index_buffer(vuk::CommandBuffer& commandBuffer) const {
  OX_CORE_ASSERT(indicies_buffer)
  commandBuffer.bind_index_buffer(*indicies_buffer, vuk::IndexType::eUint32);
}

void Mesh::set_scale(const Vec3& mesh_scale) {
  scale = mesh_scale;

  //TODO:
}

void Mesh::draw(vuk::CommandBuffer& commandBuffer) const {
  OX_SCOPED_ZONE;
  bind_vertex_buffer(commandBuffer);
  bind_index_buffer(commandBuffer);
  for (const auto& node : nodes) {
    for (const auto& primitive : node->primitives)
      commandBuffer.draw_indexed(primitive->index_count, 1, primitive->first_index, 0, 0);
  }
}

void Mesh::load_textures(tinygltf::Model& model) {
  m_textures.resize(model.images.size());
  for (size_t imageIndex = 0; imageIndex < model.images.size(); imageIndex++) {
    auto& img = model.images[imageIndex];

    if (img.name.empty())
      img.name = name + std::to_string(imageIndex);

    unsigned char* buffer;
    bool deleteBuffer = false;
    // Most devices don't support RGB only on Vulkan so convert if necessary
    if (img.component == 3) {
      const auto bufferSize = img.width * img.height * 4;
      buffer = new unsigned char[bufferSize];
      unsigned char* rgba = buffer;
      unsigned char* rgb = &img.image[0];
      for (int32_t i = 0; i < img.width * img.height; ++i) {
        for (int32_t j = 0; j < 3; ++j) {
          rgba[j] = rgb[j];
        }
        rgba += 4;
        rgb += 3;
      }
      deleteBuffer = true;
    }
    else {
      buffer = &img.image[0];
    }

    m_textures[imageIndex] = AssetManager::get_texture_asset(img.name, TextureLoadInfo{{}, (uint32_t)img.width, (uint32_t)img.height, buffer});

    if (deleteBuffer)
      delete[] buffer;
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
      material.alpha_mode = Material::AlphaMode::Blend;
    }
    else if (mat.alphaMode == "MASK") {
      material.parameters.alpha_cutoff = 0.5f;
      material.alpha_mode = Material::AlphaMode::Mask;
    }
    if (mat.additionalValues.contains("alphaCutoff")) {
      material.parameters.alpha_cutoff = static_cast<float>(mat.additionalValues["alphaCutoff"].Factor());
    }
    if (mat.extensions.contains("KHR_materials_ior")) {
      auto ext = mat.extensions.find("KHR_materials_ior");
      if (ext->second.Has("ior")) {
        auto factor = ext->second.Get("ior");
        const auto value = (float)factor.Get<double>();
        material.parameters.specular = (float)std::pow((value - 1) / (value + 1), 2);
      }
    }

    materials.push_back(create_ref<Material>(material));
  }
}

Ref<Material> Mesh::get_material(uint32_t index) const {
  OX_CORE_ASSERT(index < materials.size())
  return materials.at(index);
}

std::vector<Ref<Material>> Mesh::get_materials_as_ref() const {
  OX_SCOPED_ZONE;
  return materials;
}

void Mesh::destroy() {
  OX_SCOPED_ZONE;
  for (const auto& node : linear_nodes) {
    for (const auto primitive : node->primitives) {
      delete primitive;
    }
    delete node;
  }
  linear_nodes.clear();
  nodes.clear();
  materials.clear();
}

void Mesh::Primitive::set_dimensions(Vec3 min, Vec3 max) {
  OX_SCOPED_ZONE;
  dimensions.min = min;
  dimensions.max = max;
  dimensions.size = max - min;
  dimensions.center = (min + max) / 2.0f;
  dimensions.radius = glm::distance(min, max) / 2.0f;
}

Mat4 Mesh::Node::local_matrix() const {
  OX_SCOPED_ZONE;
  return glm::translate(Mat4(1.0f), translation) * Mat4(rotation) * glm::scale(Mat4(1.0f), scale) * matrix;
}

Mat4 Mesh::Node::get_matrix() const {
  OX_SCOPED_ZONE;
  Mat4 m = local_matrix();
  const Node* p = parent;
  while (p) {
    m = p->local_matrix() * m;
    p = p->parent;
  }
  return m;
}

void Mesh::load_node(Node* parent,
                    const tinygltf::Node& node,
                    uint32_t nodeIndex,
                    tinygltf::Model& model,
                    std::vector<uint32_t>& indexBuffer,
                    std::vector<Vertex>& vertexBuffer,
                    float globalscale) {
  OX_SCOPED_ZONE;
  Node* newNode = new Node{};
  newNode->index = nodeIndex;
  newNode->parent = parent;
  newNode->name = node.name;
  newNode->skin_index = node.skin;
  newNode->matrix = Mat4(1.0f);
  newNode->scale = Vec3(globalscale);

  // Generate local node matrix
  Vec3 translation = Vec3(0.0f);
  if (node.translation.size() == 3) {
    translation = glm::make_vec3(node.translation.data());
    newNode->translation = translation;
  }
  if (node.rotation.size() == 4) {
    glm::quat q = glm::make_quat(node.rotation.data());
    newNode->rotation = Mat4(q);
  }
  Vec3 scale = Vec3(globalscale);
  if (node.scale.size() == 3) {
    scale = glm::make_vec3(node.scale.data());
    newNode->scale = scale;
  }
  if (node.matrix.size() == 16) {
    newNode->matrix = glm::make_mat4x4(node.matrix.data());
    if (globalscale != 1.0f) { newNode->matrix = glm::scale(newNode->matrix, Vec3(globalscale)); }
  }

  // Node with children
  if (!node.children.empty()) {
    for (int children : node.children) {
      load_node(newNode, model.nodes[children], children, model, indexBuffer, vertexBuffer, globalscale);
    }
  }

  if (node.mesh > -1) {
    newNode->contains_mesh = true;
    newNode->mesh_index = node.mesh;
    const tinygltf::Mesh mesh = model.meshes[node.mesh];
    for (const auto& primitive : mesh.primitives) {
      if (primitive.indices < 0) { continue; }
      uint32_t indexStart = (uint32_t)indexBuffer.size();
      uint32_t vertexStart = (uint32_t)vertexBuffer.size();
      uint32_t indexCount = 0;
      uint32_t vertexCount = 0;
      Vec3 posMin{};
      Vec3 posMax{};
      bool hasSkin = false;
      // Vertices
      {
        const float* bufferPos = nullptr;
        const float* bufferNormals = nullptr;
        const float* bufferTexCoords = nullptr;
        const float* bufferColors = nullptr;
        const float* bufferTangents = nullptr;
        uint32_t numColorComponents;
        const uint16_t* bufferJoints = nullptr;
        const float* bufferWeights = nullptr;

        int posByteStride;
        int normByteStride;
        int uv0ByteStride;

        // Position attribute is required
        OX_CORE_ASSERT(primitive.attributes.contains("POSITION"))

        const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.find("POSITION")->second];
        const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
        bufferPos = reinterpret_cast<const float*>(&model.buffers[posView.buffer].data[posAccessor.byteOffset + posView.byteOffset]);
        posMin = Vec3(posAccessor.minValues[0], posAccessor.minValues[1], posAccessor.minValues[2]);
        posMax = Vec3(posAccessor.maxValues[0], posAccessor.maxValues[1], posAccessor.maxValues[2]);
        posByteStride = posAccessor.ByteStride(posView) ? (posAccessor.ByteStride(posView) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);

        if (primitive.attributes.contains("NORMAL")) {
          const tinygltf::Accessor& normAccessor = model.accessors[primitive.attributes.find("NORMAL")->second];
          const tinygltf::BufferView& normView = model.bufferViews[normAccessor.bufferView];
          bufferNormals = reinterpret_cast<const float*>(&model.buffers[normView.buffer].data[normAccessor.byteOffset + normView.byteOffset]);
          normByteStride = normAccessor.ByteStride(normView) ? (normAccessor.ByteStride(normView) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC3);
        }

        if (primitive.attributes.contains("TEXCOORD_0")) {
          const tinygltf::Accessor& uvAccessor = model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
          const tinygltf::BufferView& uvView = model.bufferViews[uvAccessor.bufferView];
          bufferTexCoords = reinterpret_cast<const float*>(&model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]);
          uv0ByteStride = uvAccessor.ByteStride(uvView) ? (uvAccessor.ByteStride(uvView) / sizeof(float)) : tinygltf::GetNumComponentsInType(TINYGLTF_TYPE_VEC2);
        }

        if (primitive.attributes.contains("COLOR_0")) {
          const tinygltf::Accessor& colorAccessor = model.accessors[primitive.attributes.find("COLOR_0")->second];
          const tinygltf::BufferView& colorView = model.bufferViews[colorAccessor.bufferView];
          // Color buffer are either of type vec3 or vec4
          numColorComponents = colorAccessor.type == TINYGLTF_PARAMETER_TYPE_FLOAT_VEC3 ? 3 : 4;
          bufferColors = reinterpret_cast<const float*>(&model.buffers[colorView.buffer].data[colorAccessor.byteOffset + colorView.byteOffset]);
        }

        if (primitive.attributes.contains("TANGENT")) {
          const tinygltf::Accessor& tangentAccessor = model.accessors[primitive.attributes.find("TANGENT")->second];
          const tinygltf::BufferView& tangentView = model.bufferViews[tangentAccessor.bufferView];
          bufferTangents = reinterpret_cast<const float*>(&model.buffers[tangentView.buffer].data[tangentAccessor.byteOffset + tangentView.byteOffset]);
        }

        // Skinning
        // Joints
        if (primitive.attributes.contains("JOINTS_0")) {
          const tinygltf::Accessor& jointAccessor = model.accessors[primitive.attributes.find("JOINTS_0")->second];
          const tinygltf::BufferView& jointView = model.bufferViews[jointAccessor.bufferView];
          bufferJoints = reinterpret_cast<const uint16_t*>(&model.buffers[jointView.buffer].data[jointAccessor.byteOffset + jointView.byteOffset]);
        }

        if (primitive.attributes.contains("WEIGHTS_0")) {
          const tinygltf::Accessor& uvAccessor = model.accessors[primitive.attributes.find("WEIGHTS_0")->second];
          const tinygltf::BufferView& uvView = model.bufferViews[uvAccessor.bufferView];
          bufferWeights = reinterpret_cast<const float*>(&model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]);
        }

        hasSkin = bufferJoints && bufferWeights;

        vertexCount = static_cast<uint32_t>(posAccessor.count);

        for (size_t v = 0; v < posAccessor.count; v++) {
          Vertex vert{};
          vert.position = glm::vec4(glm::make_vec3(&bufferPos[v * posByteStride]), 1.0f);
          vert.normal = glm::normalize(Vec3(bufferNormals ? glm::make_vec3(&bufferNormals[v * normByteStride]) : Vec3(0.0f)));
          vert.uv = bufferTexCoords ? glm::make_vec2(&bufferTexCoords[v * uv0ByteStride]) : Vec3(0.0f);
          if (bufferColors) {
            switch (numColorComponents) {
              case 3: vert.color = glm::vec4(glm::make_vec3(&bufferColors[v * 3]), 1.0f);
              case 4: vert.color = glm::make_vec4(&bufferColors[v * 4]);
            }
          }
          else {
            vert.color = glm::vec4(1.0f);
          }
          vert.tangent = bufferTangents ? glm::vec4(glm::make_vec4(&bufferTangents[v * 4])) : glm::vec4(0.0f);
          vert.joint0 = hasSkin ? glm::vec4(glm::make_vec4(&bufferJoints[v * 4])) : glm::vec4(0.0f);
          vert.weight0 = hasSkin ? glm::make_vec4(&bufferWeights[v * 4]) : glm::vec4(0.0f);
          vertexBuffer.push_back(vert);
        }
      }
      // Indices
      {
        const tinygltf::Accessor& accessor = model.accessors[primitive.indices];
        const tinygltf::BufferView& bufferView = model.bufferViews[accessor.bufferView];
        const tinygltf::Buffer& buffer = model.buffers[bufferView.buffer];

        indexCount = static_cast<uint32_t>(accessor.count);

        switch (accessor.componentType) {
          case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
            uint32_t* buf = new uint32_t[accessor.count];
            memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint32_t));
            for (size_t index = 0; index < accessor.count; index++) {
              indexBuffer.push_back(buf[index] + vertexStart);
            }
            delete[] buf;
            break;
          }
          case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
            uint16_t* buf = new uint16_t[accessor.count];
            memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint16_t));
            for (size_t index = 0; index < accessor.count; index++) {
              indexBuffer.push_back(buf[index] + vertexStart);
            }
            delete[] buf;
            break;
          }
          case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
            uint8_t* buf = new uint8_t[accessor.count];
            memcpy(buf, &buffer.data[accessor.byteOffset + bufferView.byteOffset], accessor.count * sizeof(uint8_t));
            for (size_t index = 0; index < accessor.count; index++) {
              indexBuffer.push_back(buf[index] + vertexStart);
            }
            delete[] buf;
            break;
          }
          default: OX_CORE_ERROR("Index component type {0} not supported", accessor.componentType);
            return;
        }
      }

      auto newPrimitive = new Primitive(indexStart, indexCount);
      newPrimitive->material_index = primitive.material;
      if (newPrimitive->material_index < 0)
        newPrimitive->material_index = 0;
      newPrimitive->first_vertex = vertexStart;
      newPrimitive->vertex_count = vertexCount;
      newPrimitive->set_dimensions(posMin, posMax);
      newNode->primitives.push_back(newPrimitive);
    }
  }
  if (parent) {
    parent->children.push_back(newNode);
  }
  else {
    nodes.push_back(newNode);
  }
  linear_nodes.push_back(newNode);
}
}
