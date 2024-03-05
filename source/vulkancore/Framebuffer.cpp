#include "Framebuffer.hpp"

#include <algorithm>

#include "Context.hpp"
#include "Texture.hpp"

namespace VulkanCore {

Framebuffer::Framebuffer(const Context& context, VkDevice device, VkRenderPass renderPass,
                         const std::vector<std::shared_ptr<Texture>>& attachments,
                         const std::shared_ptr<Texture> depthAttachment,
                         const std::shared_ptr<Texture> stencilAttachment,
                         const std::string& name)
    : device_{device} {
  std::vector<VkImageView> imageViews;
  for (const auto texture : attachments) {
    imageViews.push_back(texture->vkImageView(0));
  }
  if (depthAttachment) {
    imageViews.push_back(depthAttachment->vkImageView(0));
  }
  if (stencilAttachment) {
    imageViews.push_back(stencilAttachment->vkImageView(0));
  }

  ASSERT(!imageViews.empty(),
         "Creating a framebuffer with no attachments is not supported");

  const uint32_t width = !attachments.empty() ? attachments[0]->vkExtents().width
                                              : depthAttachment->vkExtents().width;
  const uint32_t height = !attachments.empty() ? attachments[0]->vkExtents().height
                                               : depthAttachment->vkExtents().height;

  const VkFramebufferCreateInfo framebufferInfo = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      //.flags = VK_FRAMEBUFFER_CREATE_IMAGELESS_BIT,
      .renderPass = renderPass,
      .attachmentCount = static_cast<uint32_t>(imageViews.size()),
      .pAttachments = imageViews.data(),
      .width = width,
      .height = height,
      .layers = 1,
  };
  VK_CHECK(vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &framebuffer_));
  context.setVkObjectname(framebuffer_, VK_OBJECT_TYPE_FRAMEBUFFER,
                          "Framebuffer: " + name);
}

Framebuffer::~Framebuffer() { vkDestroyFramebuffer(device_, framebuffer_, nullptr); }

VkFramebuffer Framebuffer::vkFramebuffer() const { return framebuffer_; }

}  // namespace VulkanCore
