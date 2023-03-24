#pragma once
#include "Render/Mesh.h"
#include "Render/Vulkan/VulkanImage.h"

namespace Oxylus {
  class Prefilter {
  public:
    static void GenerateBRDFLUT(VulkanImage& target);
    static void GenerateIrradianceCube(VulkanImage& target,
                                       const Mesh& skybox,
                                       const VertexLayout& vertexLayout,
                                       const vk::DescriptorImageInfo& skyboxDescriptor);
    static void GeneratePrefilteredCube(VulkanImage& target,
                                        const Mesh& skybox,
                                        const VertexLayout& vertexLayout,
                                        const vk::DescriptorImageInfo& skyboxDescriptor);
  };
}
