#pragma once
#include <vector>
#include <vulkan/vulkan.hpp>

namespace Oxylus {
  class PipelineStats {
  public:
    PipelineStats();
    ~PipelineStats();

    void CreateQuery(vk::QueryPipelineStatisticFlags statisticFlags, uint32_t size);
    void ResetQuery(const vk::CommandBuffer& commandBuffer) const;
    void BeginQuery(const vk::CommandBuffer& commandBuffer) const;
    void EndQuery(const vk::CommandBuffer& commandBuffer) const;
    void SubmitQuery();

    const std::vector<uint64_t>& GetQueriedStats() const { return m_QueriedStats; }

  private:
    vk::QueryPool m_QueryPool{};
    std::vector<uint64_t> m_QueriedStats{};
  };
}
