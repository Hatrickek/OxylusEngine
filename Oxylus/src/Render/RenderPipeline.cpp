#include "RenderPipeline.h"

#include "Utils/Log.hpp"

namespace ox {
void RenderPipeline::detach_swapchain(const vuk::Extent3D dim, Vec2 offset) {
  attach_swapchain = false;
  OX_CHECK_GT(extent.width, 0);
  OX_CHECK_GT(extent.height, 0);
  extent = dim;
  viewport_offset = offset;
}
}
