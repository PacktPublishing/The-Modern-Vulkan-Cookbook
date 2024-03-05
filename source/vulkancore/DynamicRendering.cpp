#include "DynamicRendering.hpp"

namespace VulkanCore {

std::string DynamicRendering::instanceExtensions() {
  return "VK_KHR_get_physical_device_properties2";
}

void DynamicRendering::beginRenderingCmd(
    VkCommandBuffer commandBuffer, VkImage image, VkRenderingFlags renderingFlags,
    VkRect2D rectRenderSize, uint32_t layerCount, uint32_t viewMask,
    std::vector<AttachmentDescription> colorAttachmentDescList,
    const AttachmentDescription* depthAttachmentDescList,
    const AttachmentDescription* stencilAttachmentDescList, VkImageLayout oldLayout,
    VkImageLayout newLayout) {
  std::vector<VkRenderingAttachmentInfo> colorRenderingAttachmentInfoList;

  for (auto& renderingAttachmentInfoParam : colorAttachmentDescList) {
    VkRenderingAttachmentInfo renderingAttachmentInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = NULL,
        .imageView = renderingAttachmentInfoParam.imageView,
        .imageLayout = renderingAttachmentInfoParam.imageLayout,
        .resolveMode = renderingAttachmentInfoParam.resolveModeFlagBits,
        .resolveImageView = renderingAttachmentInfoParam.resolveImageView,
        .resolveImageLayout = renderingAttachmentInfoParam.resolveImageLayout,
        .loadOp = renderingAttachmentInfoParam.attachmentLoadOp,
        .storeOp = renderingAttachmentInfoParam.attachmentStoreOp,
        .clearValue = renderingAttachmentInfoParam.clearValue};

    colorRenderingAttachmentInfoList.push_back(renderingAttachmentInfo);
  }

  VkRenderingAttachmentInfo depthRenderingAttachmentInfo;
  VkRenderingAttachmentInfo* depthRenderingAttachmentInfoPtr = NULL;
  if (depthAttachmentDescList) {
    depthRenderingAttachmentInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = NULL,
        .imageView = depthAttachmentDescList->imageView,
        .imageLayout = depthAttachmentDescList->imageLayout,
        .resolveMode = depthAttachmentDescList->resolveModeFlagBits,
        .resolveImageView = depthAttachmentDescList->resolveImageView,
        .resolveImageLayout = depthAttachmentDescList->resolveImageLayout,
        .loadOp = depthAttachmentDescList->attachmentLoadOp,
        .storeOp = depthAttachmentDescList->attachmentStoreOp,
        .clearValue = depthAttachmentDescList->clearValue};

    depthRenderingAttachmentInfoPtr = &depthRenderingAttachmentInfo;
  }

  VkRenderingAttachmentInfo stencilRenderingAttachmentInfo;
  VkRenderingAttachmentInfo* stencilRenderingAttachmentInfoPtr = NULL;
  if (stencilAttachmentDescList) {
    stencilRenderingAttachmentInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .pNext = NULL,
        .imageView = stencilAttachmentDescList->imageView,
        .imageLayout = stencilAttachmentDescList->imageLayout,
        .resolveMode = stencilAttachmentDescList->resolveModeFlagBits,
        .resolveImageView = stencilAttachmentDescList->resolveImageView,
        .resolveImageLayout = stencilAttachmentDescList->resolveImageLayout,
        .loadOp = stencilAttachmentDescList->attachmentLoadOp,
        .storeOp = stencilAttachmentDescList->attachmentStoreOp,
        .clearValue = stencilAttachmentDescList->clearValue};

    stencilRenderingAttachmentInfoPtr = &stencilRenderingAttachmentInfo;
  }

  VkRenderingInfo renderingInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
      .pNext = NULL,
      .flags = renderingFlags,
      .renderArea = rectRenderSize,
      .layerCount = layerCount,
      .viewMask = viewMask,
      .colorAttachmentCount = (uint32_t)colorRenderingAttachmentInfoList.size(),
      .pColorAttachments = colorRenderingAttachmentInfoList.data(),
      .pDepthAttachment = depthRenderingAttachmentInfoPtr,
      .pStencilAttachment = stencilRenderingAttachmentInfoPtr};

  if (oldLayout != newLayout) {
    const VkImageMemoryBarrier imageMemoryBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }};

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,              // srcStageMask
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // dstStageMask
                         0, 0, nullptr, 0, nullptr, 1, &imageMemoryBarrier);
  }

  vkCmdBeginRendering(commandBuffer, &renderingInfo);
}

void DynamicRendering::endRenderingCmd(VkCommandBuffer commandBuffer, VkImage image,
                                       VkImageLayout oldLayout, VkImageLayout newLayout) {
  vkCmdEndRendering(commandBuffer);

  if (oldLayout != newLayout) {
    const VkImageMemoryBarrier image_memory_barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .image = image,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        }};

    vkCmdPipelineBarrier(commandBuffer,
                         VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,  // srcStageMask
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,           // dstStageMask
                         0, 0, nullptr, 0, nullptr,
                         1,                     // imageMemoryBarrierCount
                         &image_memory_barrier  // pImageMemoryBarriers
    );
  }
}

}  // namespace VulkanCore
