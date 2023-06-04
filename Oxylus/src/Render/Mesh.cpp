#include "src/oxpch.h"
#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include "Mesh.h"

#include <glm/gtc/type_ptr.hpp>

#include "Assets/AssetManager.h"
#include "Utils/Profiler.h"
#include "Vulkan/VulkanContext.h"
#include "Vulkan/VulkanRenderer.h"
#include "Utils/Log.h"
#include "Core/Resources.h"

namespace Oxylus {
  Mesh::Mesh(const std::string_view path, int fileLoadingFlags, float scale) {
    LoadFromFile(path.data(), fileLoadingFlags, scale);
  }

  Mesh::~Mesh() {
    Destroy();
  }

  bool IsImageKtx(const tinygltf::Image& image) {
    if (image.uri.find_last_of('.') != std::string::npos) {
      if (image.uri.substr(image.uri.find_last_of('.') + 1) == "ktx") { return true; }
    }
    return false;
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
    if (IsImageKtx(*image))
      return true;

    return tinygltf::LoadImageData(image, imageIndex, error, warning, req_width, req_height, bytes, size, userData);
  }

  void Mesh::LoadFromFile(const std::string& path, int fileLoadingFlags, const float scale) {
    ZoneScoped;
    ProfilerTimer timer;

    Path = path;
    LoadingFlags = fileLoadingFlags;
    ShouldUpdate = true;

    tinygltf::Model gltfModel;
    tinygltf::TinyGLTF gltfContext;
    gltfContext.SetImageLoader(LoadImageDataCallback, nullptr);

    std::string error, warning;
    bool fileLoaded = false;
    if (std::filesystem::path(path).extension() == ".gltf")
      fileLoaded = gltfContext.LoadASCIIFromFile(&gltfModel, &error, &warning, path);
    else
      fileLoaded = gltfContext.LoadBinaryFromFile(&gltfModel, &error, &warning, path);
    if (!fileLoaded) {
      OX_CORE_ERROR("Couldnt load gltf file: {}", error);
      LoadFailFallback();
    }
    if (!warning.empty())
      OX_CORE_WARN("GLTF loader warning: {}", warning);

    LoadTextures(gltfModel);
    LoadMaterials(gltfModel);

    const tinygltf::Scene& scene = gltfModel.scenes[gltfModel.defaultScene > -1 ? gltfModel.defaultScene : 0];

    for (int nodeIndex : scene.nodes) {
      const tinygltf::Node node = gltfModel.nodes[nodeIndex];
      LoadNode(nullptr, node, nodeIndex, gltfModel, m_IndexBuffer, m_VertexBuffer, scale);
    }
    //TODO: Skins and animations.

    const bool preMultiplyColor = fileLoadingFlags & FileLoadingFlags::PreMultiplyVertexColors;
    const bool flipY = fileLoadingFlags & FileLoadingFlags::FlipY;
    for (auto& node : LinearNodes) {
      if (!node)
        continue;
      const Mat4 localMatrix = node->GetMatrix();
      for (const Primitive* primitive : node->Primitives) {
        for (uint32_t i = 0; i < primitive->vertexCount; i++) {
          Vertex& vertex = m_VertexBuffer[primitive->firstVertex + i];
          vertex.Pos = Vec3(localMatrix * glm::vec4(vertex.Pos, 1.0f));
          vertex.Normal = glm::normalize(glm::mat3(localMatrix) * vertex.Normal);
          if (flipY) {
            vertex.Pos.y *= -1.0f;
            vertex.Normal.y *= -1.0f;
          }
          if (preMultiplyColor) {
            //vertex.color = primitive->material.baseColorFactor * vertex.color;
          }
        }
      }
    }

    const vk::DeviceSize vBufferSize = m_VertexBuffer.size() * sizeof(Vertex);
    const vk::DeviceSize iBufferSize = m_IndexBuffer.size() * sizeof(uint32_t);
    IndexCount = static_cast<uint32_t>(m_IndexBuffer.size());
    VertexCount = static_cast<uint32_t>(m_VertexBuffer.size());

    OX_CORE_ASSERT(iBufferSize);
    OX_CORE_ASSERT(vBufferSize);

    VulkanBuffer vertexStaging, indexStaging;

    vertexStaging.CreateBuffer(vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      vBufferSize,
      m_VertexBuffer.data(),
      VMA_MEMORY_USAGE_AUTO_PREFER_HOST);

    VerticiesBuffer.CreateBuffer(vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eTransferDst,
      vk::MemoryPropertyFlagBits::eDeviceLocal,
      vBufferSize,
      nullptr,
      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

    indexStaging.CreateBuffer(vk::BufferUsageFlagBits::eTransferSrc,
      vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent,
      iBufferSize,
      m_IndexBuffer.data(),
      VMA_MEMORY_USAGE_AUTO_PREFER_HOST);

    IndiciesBuffer.CreateBuffer(vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eTransferDst,
      vk::MemoryPropertyFlagBits::eDeviceLocal,
      iBufferSize,
      nullptr,
      VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE);

    VulkanRenderer::SubmitOnce(CommandPoolManager::Get()->GetFreePool(),
      [&](const VulkanCommandBuffer copyCmd) {
        vk::BufferCopy copyRegion{};

        copyRegion.size = vBufferSize;
        vertexStaging.CopyTo(VerticiesBuffer.Get(), copyCmd.Get(), copyRegion);

        copyRegion.size = iBufferSize;
        indexStaging.CopyTo(IndiciesBuffer.Get(), copyCmd.Get(), copyRegion);
      });

    // Destroy staging resources
    vertexStaging.Destroy();
    indexStaging.Destroy();

    m_VertexBuffer.clear();
    m_IndexBuffer.clear();

    m_Textures.clear();

    timer.Stop();
    Name = std::filesystem::path(path).filename().string();
    OX_CORE_TRACE("Mesh file loaded: {}, {} materials, {} ms",
      Name.c_str(),
      gltfModel.materials.size(),
      timer.ElapsedMilliSeconds());
  }

  void Mesh::SetScale(const Vec3& scale) {
    m_Scale = scale;

    //TODO:
  }

  void Mesh::Draw(const vk::CommandBuffer& cmdBuffer) const {
    ZoneScoped;
    constexpr vk::DeviceSize offsets[1] = {0};
    cmdBuffer.bindVertexBuffers(0, VerticiesBuffer.Get(), offsets);
    cmdBuffer.bindIndexBuffer(IndiciesBuffer.Get(), 0, vk::IndexType::eUint32);
    for (const auto& node : Nodes) {
      for (const auto& primitive : node->Primitives)
        cmdBuffer.drawIndexed(primitive->indexCount, 1, primitive->firstIndex, 0, 0);
    }
  }

  void Mesh::UpdateMaterials() const {
    for (const auto& material : m_Materials) {
      material->Update();
    }
  }

  void Mesh::LoadTextures(const tinygltf::Model& model) {
    m_Textures.resize(model.images.size());
    for (size_t i = 0; i < model.images.size(); i++) {
      auto& img = model.images[i];
      VulkanImageDescription desc;
      desc.CreateDescriptorSet = true;
      desc.Width = img.width;
      desc.Height = img.height;
      //desc.MipLevels = VulkanImage::GetMaxMipmapLevel(img.width, img.height, 1) - 1;
      if (IsImageKtx(img) || !img.uri.empty()) {
        desc.Path = (std::filesystem::path(Path).remove_filename() / img.uri).string();
        m_Textures[i] = AssetManager::GetImageAsset(desc).Data;
      }
      else {
        desc.EmbeddedStbData = img.image.data();
        desc.EmbeddedDataLength = static_cast<uint32_t>(img.image.size());
        m_Textures[i] = CreateRef<VulkanImage>(desc);
      }
    }
  }

  void Mesh::LoadMaterials(tinygltf::Model& model) {
    ZoneScoped;
    // Create a empty material if the mesh file doesn't have any.
    if (model.materials.empty()) {
      m_Materials.emplace_back(CreateRef<Material>());
      const bool dontCreateMaterials = LoadingFlags & FileLoadingFlags::DontCreateMaterials;
      if (!dontCreateMaterials)
        m_Materials[0]->Create();
      return;
    }

    for (tinygltf::Material& mat : model.materials) {
      Material material;
      material.Create();
      if (!mat.name.empty())
        material.Name = mat.name;
      material.Parameters.DoubleSided = mat.doubleSided;
      if (mat.values.contains("baseColorTexture")) {
        material.AlbedoTexture = m_Textures.at(model.textures[mat.values["baseColorTexture"].TextureIndex()].source);
        material.Parameters.UseAlbedo = true;
      }
      if (mat.values.contains("metallicRoughnessTexture")) {
        material.MetallicTexture = m_Textures.at(model.textures[mat.values["metallicRoughnessTexture"].TextureIndex()].source);
        material.Parameters.UseMetallic = true;
      }
      if (mat.values.contains("roughnessFactor")) {
        material.Parameters.Roughness = static_cast<float>(mat.values["roughnessFactor"].Factor());
      }
      if (mat.values.contains("metallicFactor")) {
        material.Parameters.Metallic = static_cast<float>(mat.values["metallicFactor"].Factor());
      }
      if (mat.values.contains("specularFactor")) {
        material.Parameters.Specular = static_cast<float>(mat.values["specularFactor"].Factor());
      }
      if (mat.values.contains("baseColorFactor")) {
        material.Parameters.Color = glm::make_vec4(mat.values["baseColorFactor"].ColorFactor().data());
      }
      if (mat.additionalValues.contains("emissiveFactor")) {
        material.Parameters.Emmisive = glm::vec4(glm::make_vec3(mat.additionalValues["emissiveFactor"].ColorFactor().data()), 1.0);
      }
      if (mat.additionalValues.contains("normalTexture")) {
        material.NormalTexture = m_Textures.at(model.textures[mat.additionalValues["normalTexture"].TextureIndex()].source);
        material.Parameters.UseNormal = true;
      }
      if (mat.additionalValues.contains("emissiveTexture")) {
        material.EmissiveTexture = m_Textures.at(model.textures[mat.additionalValues["emissiveTexture"].TextureIndex()].source);
        material.Parameters.UseEmissive = true;
      }
      if (mat.additionalValues.contains("occlusionTexture")) {
        material.AOTexture = m_Textures.at(model.textures[mat.additionalValues["occlusionTexture"].TextureIndex()].source);
      }
      if (mat.additionalValues.contains("alphaMode")) {
        tinygltf::Parameter param = mat.additionalValues["alphaMode"];
        if (param.string_value == "BLEND") {
          material.AlphaMode = Material::AlphaMode::Blend;
        }
        if (param.string_value == "MASK") {
          material.Parameters.AlphaCutoff = 0.5f;
          material.AlphaMode = Material::AlphaMode::Mask;
        }
      }
      if (mat.additionalValues.contains("alphaCutoff")) {
        material.Parameters.AlphaCutoff = static_cast<float>(mat.additionalValues["alphaCutoff"].Factor());
      }
      if (mat.extensions.contains("KHR_materials_pbrSpecularGlossiness")) {
        auto ext = mat.extensions.find("KHR_materials_pbrSpecularGlossiness");
        if (ext->second.Has("specularGlossinessTexture")) {
          auto index = ext->second.Get("specularGlossinessTexture").Get("index");
          material.SpecularTexture = m_Textures.at(model.textures[mat.additionalValues["specularGlossinessTexture"].TextureIndex()].source);
          material.Parameters.UseSpecular = true;
        }
        if (ext->second.Has("specularFactor")) {
          auto factor = ext->second.Get("specularFactor");
          for (uint32_t i = 0; i < factor.ArrayLen(); i++) {
            auto& val = factor.Get(i);
            material.Parameters.Specular = val.IsNumber() ? (float)val.Get<double>() : (float)val.Get<int>();
          }
        }
#if 0 //NOTE: Currently Not used.
        if (ext->second.Has("diffuseTexture")) {
          material.DiffuseTexture = m_Textures.at(model.textures[mat.additionalValues["diffuseTexture"].TextureIndex()].source);
          material.Parameters.UseDiffuse = true; 
        }
        if (ext->second.Has("diffuseFactor")) {
          auto factor = ext->second.Get("diffuseFactor");
          for (uint32_t i = 0; i < factor.ArrayLen(); i++) {
            auto val = factor.Get(i);
            material.Parameters.DiffuseFactor[i] = val.IsNumber() ? (float)val.Get<double>() : (float)val.Get<int>();
          }
        }
#endif
      }
      if (mat.extensions.contains("KHR_materials_ior")) {
        auto ext = mat.extensions.find("KHR_materials_ior");
        if (ext->second.Has("ior")) {
          auto factor = ext->second.Get("ior");
          const auto value = (float)factor.Get<double>();
          material.Parameters.Specular = (float)std::pow((value - 1) / (value + 1), 2);
        }
      }

      m_Materials.push_back(CreateRef<Material>(material));
    }
  }

  const Ref<Material> Mesh::GetMaterial(uint32_t index) const {
    OX_CORE_ASSERT(index < m_Materials.size());
    return m_Materials.at(index);
  }

  std::vector<Ref<Material>> Mesh::GetMaterialsAsRef() const {
    ZoneScoped;
    return m_Materials;
  }

  void Mesh::Destroy() {
    ZoneScoped;
    for (const auto& node : LinearNodes) {
      for (const auto primitive : node->Primitives) {
        delete primitive;
      }
      delete node;
    }
    LinearNodes.clear();
    Nodes.clear();
    VerticiesBuffer.Destroy();
    IndiciesBuffer.Destroy();
    m_Materials.clear();
  }

  void Mesh::Primitive::SetDimensions(Vec3 min, Vec3 max) {
    ZoneScoped;
    dimensions.min = min;
    dimensions.max = max;
    dimensions.size = max - min;
    dimensions.center = (min + max) / 2.0f;
    dimensions.radius = glm::distance(min, max) / 2.0f;
  }

  Mat4 Mesh::Node::LocalMatrix() const {
    ZoneScoped;
    return glm::translate(Mat4(1.0f), Translation) * Mat4(Rotation) * glm::scale(Mat4(1.0f), Scale) *
           Matrix;
  }

  Mat4 Mesh::Node::GetMatrix() const {
    ZoneScoped;
    Mat4 m = LocalMatrix();
    const Node* p = Parent;
    while (p) {
      m = p->LocalMatrix() * m;
      p = p->Parent;
    }
    return m;
  }

  void Mesh::LoadNode(Node* parent,
                      const tinygltf::Node& node,
                      uint32_t nodeIndex,
                      tinygltf::Model& model,
                      std::vector<uint32_t>& indexBuffer,
                      std::vector<Vertex>& vertexBuffer,
                      float globalscale) {
    ZoneScoped;
    Node* newNode = new Node{};
    newNode->Index = nodeIndex;
    newNode->Parent = parent;
    newNode->Name = node.name;
    newNode->SkinIndex = node.skin;
    newNode->Matrix = Mat4(1.0f);
    newNode->Scale = Vec3(globalscale);

    // Generate local node matrix
    Vec3 translation = Vec3(0.0f);
    if (node.translation.size() == 3) {
      translation = glm::make_vec3(node.translation.data());
      newNode->Translation = translation;
    }
    if (node.rotation.size() == 4) {
      glm::quat q = glm::make_quat(node.rotation.data());
      newNode->Rotation = Mat4(q);
    }
    Vec3 scale = Vec3(globalscale);
    if (node.scale.size() == 3) {
      scale = glm::make_vec3(node.scale.data());
      newNode->Scale = scale;
    }
    if (node.matrix.size() == 16) {
      newNode->Matrix = glm::make_mat4x4(node.matrix.data());
      if (globalscale != 1.0f) { newNode->Matrix = glm::scale(newNode->Matrix, Vec3(globalscale)); }
    }

    // Node with children
    if (!node.children.empty()) {
      for (int children : node.children) {
        LoadNode(newNode, model.nodes[children], children, model, indexBuffer, vertexBuffer, globalscale);
      }
    }

    if (node.mesh > -1) {
      newNode->ContainsMesh = true;
      newNode->MeshIndex = node.mesh;
      const tinygltf::Mesh mesh = model.meshes[node.mesh];
      for (const auto& primitive : mesh.primitives) {
        if (primitive.indices < 0) { continue; }
        uint32_t indexStart = static_cast<uint32_t>(indexBuffer.size());
        uint32_t vertexStart = static_cast<uint32_t>(vertexBuffer.size());
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

          // Position attribute is required
          assert(primitive.attributes.find("POSITION") != primitive.attributes.end());

          const tinygltf::Accessor& posAccessor = model.accessors[primitive.attributes.find("POSITION")->second];
          const tinygltf::BufferView& posView = model.bufferViews[posAccessor.bufferView];
          bufferPos = reinterpret_cast<const float*>(&model.buffers[posView.buffer].data[posAccessor.byteOffset + posView.byteOffset]);
          posMin = Vec3(posAccessor.minValues[0], posAccessor.minValues[1], posAccessor.minValues[2]);
          posMax = Vec3(posAccessor.maxValues[0], posAccessor.maxValues[1], posAccessor.maxValues[2]);

          if (primitive.attributes.contains("NORMAL")) {
            const tinygltf::Accessor& normAccessor = model.accessors[primitive.attributes.find("NORMAL")->second];
            const tinygltf::BufferView& normView = model.bufferViews[normAccessor.bufferView];
            bufferNormals = reinterpret_cast<const float*>(&model.buffers[normView.buffer].data[normAccessor.byteOffset + normView.byteOffset]);
          }

          if (primitive.attributes.contains("TEXCOORD_0")) {
            const tinygltf::Accessor& uvAccessor = model.accessors[primitive.attributes.find("TEXCOORD_0")->second];
            const tinygltf::BufferView& uvView = model.bufferViews[uvAccessor.bufferView];
            bufferTexCoords = reinterpret_cast<const float*>(&model.buffers[uvView.buffer].data[uvAccessor.byteOffset + uvView.byteOffset]);
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
            vert.Pos = glm::vec4(glm::make_vec3(&bufferPos[v * 3]), 1.0f);
            vert.Normal = glm::normalize(
              Vec3(bufferNormals ? glm::make_vec3(&bufferNormals[v * 3]) : Vec3(0.0f)));
            vert.UV = bufferTexCoords ? glm::make_vec2(&bufferTexCoords[v * 2]) : Vec3(0.0f);
            if (bufferColors) {
              switch (numColorComponents) {
                case 3: vert.Color = glm::vec4(glm::make_vec3(&bufferColors[v * 3]), 1.0f);
                case 4: vert.Color = glm::make_vec4(&bufferColors[v * 4]);
              }
            }
            else { vert.Color = glm::vec4(1.0f); }
            vert.Tangent = bufferTangents ? glm::vec4(glm::make_vec4(&bufferTangents[v * 4])) : glm::vec4(0.0f);
            vert.Joint0 = hasSkin ? glm::vec4(glm::make_vec4(&bufferJoints[v * 4])) : glm::vec4(0.0f);
            vert.Weight0 = hasSkin ? glm::make_vec4(&bufferWeights[v * 4]) : glm::vec4(0.0f);
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
        newPrimitive->materialIndex = primitive.material;
        if (newPrimitive->materialIndex < 0)
          newPrimitive->materialIndex = 0;
        newPrimitive->firstVertex = vertexStart;
        newPrimitive->vertexCount = vertexCount;
        newPrimitive->SetDimensions(posMin, posMax);
        newNode->Primitives.push_back(newPrimitive);
      }
    }
    if (parent) {
      parent->Children.push_back(newNode);
    }
    else {
      Nodes.push_back(newNode);
    }
    LinearNodes.push_back(newNode);
  }

  void Mesh::LoadFailFallback() {
    ZoneScoped;
    if (std::filesystem::exists("Resources/Objects/cube.glb")) {
      LoadFromFile("Resources/Objects/cube.glb");
      OX_CORE_ERROR("Could not load mesh file {0}. Falled back to a cube.", Path);
    }
    else
      OX_CORE_ERROR("Couldn't load mesh file {0}", Path);
    m_Materials[0]->Parameters.Color.r = 1.0f;
    m_Materials[0]->Parameters.Specular = 0.0f;
  }
}
