#pragma once

#include <memory>
#include <string>
#include <vector>

#include "Common.hpp"
#include "Utility.hpp"

namespace VulkanCore {

class Context;
class Texture;

class RenderPass final {
 public:
  MOVABLE_ONLY(RenderPass);

  RenderPass(const Context& context,
             const std::vector<std::shared_ptr<Texture>> attachments,
             const std::vector<std::shared_ptr<Texture>> resolveAttachments,
             const std::vector<VkAttachmentLoadOp>& loadOp,
             const std::vector<VkAttachmentStoreOp>& storeOp,
             const std::vector<VkImageLayout>& layout, VkPipelineBindPoint bindPoint,
             const std::string& name = "");

  RenderPass(const Context& context, const std::vector<VkFormat>& formats,
             const std::vector<VkImageLayout>& initialLayouts,
             const std::vector<VkImageLayout>& finalLayouts,
             const std::vector<VkAttachmentLoadOp>& loadOp,
             const std::vector<VkAttachmentStoreOp>& storeOp,
             VkPipelineBindPoint bindPoint,
             std::vector<uint32_t> resolveAttachmentsIndices,
             uint32_t depthAttachmentIndex, uint32_t stencilAttachmentIndex = UINT32_MAX,
             VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
             VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
             bool multiview = false, const std::string& name = "");

  // RenderPass2 - Fragment Density Map support
  RenderPass(const Context& context, const std::vector<VkFormat>& formats,
             const std::vector<VkImageLayout>& initialLayouts,
             const std::vector<VkImageLayout>& finalLayouts,
             const std::vector<VkAttachmentLoadOp>& loadOp,
             const std::vector<VkAttachmentStoreOp>& storeOp,
             VkPipelineBindPoint bindPoint,
             std::vector<uint32_t> resolveAttachmentsIndices,
             uint32_t depthAttachmentIndex, uint32_t fragmentDensityMapIndex,
             uint32_t stencilAttachmentIndex = UINT32_MAX,
             VkAttachmentLoadOp stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
             VkAttachmentStoreOp stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
             bool multiview = false, const std::string& name = "");

  ~RenderPass();

  VkRenderPass vkRenderPass() const;

 private:
  VkDevice device_ = VK_NULL_HANDLE;
  VkRenderPass renderPass_ = VK_NULL_HANDLE;
};

}  // namespace VulkanCore
