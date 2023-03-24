#include "oxpch.h"
#include "PipelineStats.h"

#include "VulkanUtils.h"

#include "Render/Vulkan/VulkanContext.h"

namespace Oxylus {
  PipelineStats::PipelineStats() = default;

  PipelineStats::~PipelineStats() = default;

  void PipelineStats::CreateQuery(const vk::QueryPipelineStatisticFlags statisticFlags, const uint32_t size) {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    vk::QueryPoolCreateInfo queryPoolInfo = {};
    queryPoolInfo.queryType = vk::QueryType::ePipelineStatistics;
    queryPoolInfo.pipelineStatistics = statisticFlags;
    queryPoolInfo.queryCount = size;
    VulkanUtils::CheckResult(LogicalDevice.createQueryPool(&queryPoolInfo, nullptr, &m_QueryPool));

    m_QueriedStats.resize(size);
  }

  void PipelineStats::ResetQuery(const vk::CommandBuffer& commandBuffer) const {
    commandBuffer.resetQueryPool(m_QueryPool, 0, (uint32_t)m_QueriedStats.size());
  }

  void PipelineStats::BeginQuery(const vk::CommandBuffer& commandBuffer) const {
    commandBuffer.beginQuery(m_QueryPool, 0, {});
  }

  void PipelineStats::EndQuery(const vk::CommandBuffer& commandBuffer) const {
    commandBuffer.endQuery(m_QueryPool, 0);
  }

  void PipelineStats::SubmitQuery() {
    const auto& LogicalDevice = VulkanContext::Context.Device;
    const auto count = (uint32_t)m_QueriedStats.size();
    VulkanUtils::CheckResult(LogicalDevice.getQueryPoolResults(m_QueryPool,
      0,
      1,
      count * sizeof(uint64_t),
      m_QueriedStats.data(),
      sizeof(uint64_t),
      vk::QueryResultFlagBits::e64));
  }
}
