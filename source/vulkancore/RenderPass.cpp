#include "RenderPass.hpp"

#include <optional>

#include "Context.hpp"
#include "Texture.hpp"

namespace VulkanCore {

RenderPass::RenderPass(const Context& context,
                       const std::vector<std::shared_ptr<Texture>> attachments,
                       const std::vector<std::shared_ptr<Texture>> resolveAttachments,
                       const std::vector<VkAttachmentLoadOp>& loadOp,
                       const std::vector<VkAttachmentStoreOp>& storeOp,
                       const std::vector<VkImageLayout>& layout,
                       VkPipelineBindPoint bindPoint, const std::string& name)
    : device_{context.device()} {
  // ASSERT(attachments.size() == loadOp.size() && attachments.size() == storeOp.size() &&
  //            attachments.size() == layout.size(),
  //        "The sizes of the attachments and their load and store operations and final "
  //        "layouts must match");
  // ASSERT(resolveAttachments.empty() || (attachments.size() ==
  // resolveAttachments.size()));

  std::vector<VkAttachmentDescription> attachmentDescriptors;
  std::vector<VkAttachmentReference> colorAttachmentReferences;
  std::vector<VkAttachmentReference> resolveAttachmentReferences;
  std::optional<VkAttachmentReference> depthStencilAttachmentReference;
  for (uint32_t index = 0; index < attachments.size(); ++index) {
    attachmentDescriptors.emplace_back(VkAttachmentDescription{
        .format = attachments[index]->vkFormat(),
        .samples = attachments[index]->VkSampleCount(),
        .loadOp = loadOp[index],
        .storeOp = storeOp[index],
        .stencilLoadOp = attachments[index]->isStencil()
                             ? loadOp[index]
                             : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = attachments[index]->isStencil()
                              ? storeOp[index]
                              : VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = attachments[index]->vkLayout(),
        .finalLayout = layout[index],
    });

    if (attachments[index]->isStencil() || attachments[index]->isDepth()) {
      depthStencilAttachmentReference = VkAttachmentReference{
          .attachment = index,
          .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      };
    } else {
      colorAttachmentReferences.emplace_back(VkAttachmentReference{
          .attachment = index,
          .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      });
    }
  }

  const uint32_t numAttachments = attachmentDescriptors.size();
  for (uint32_t index = 0; index < resolveAttachments.size(); ++index) {
    attachmentDescriptors.emplace_back(VkAttachmentDescription{
        .format = resolveAttachments[index]->vkFormat(),
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = loadOp[index + numAttachments],
        .storeOp = storeOp[index + numAttachments],
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = resolveAttachments[index]->vkLayout(),
        .finalLayout = layout[index + numAttachments],
    });

    resolveAttachmentReferences.emplace_back(VkAttachmentReference{
        .attachment = static_cast<uint32_t>(attachmentDescriptors.size() - 1),
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    });
  }

  const VkSubpassDescription spd = {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = static_cast<uint32_t>(colorAttachmentReferences.size()),
      .pColorAttachments = colorAttachmentReferences.data(),
      .pResolveAttachments = resolveAttachmentReferences.data(),
      .pDepthStencilAttachment = depthStencilAttachmentReference.has_value()
                                     ? &depthStencilAttachmentReference.value()
                                     : nullptr,
  };

  std::array<VkSubpassDependency, 2> dependencies;
  dependencies[0] = {
      .srcSubpass = VK_SUBPASS_EXTERNAL,
      .dstSubpass = 0,
      .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                      VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
      .dstAccessMask =
          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
  };

  dependencies[1] = {
      .srcSubpass = 0,
      .dstSubpass = VK_SUBPASS_EXTERNAL,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                      VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
      .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                       VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                       VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      .dstAccessMask =
          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
  };

  const VkRenderPassCreateInfo rpci = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .attachmentCount = static_cast<uint32_t>(attachmentDescriptors.size()),
      .pAttachments = attachmentDescriptors.data(),
      .subpassCount = 1,
      .pSubpasses = &spd,
      .dependencyCount = 2,
      .pDependencies =
          dependencies.data(),  // being extra liberal with this, in production this
                                // should be carefully provided depending upon each pass
  };

  VK_CHECK(vkCreateRenderPass(device_, &rpci, nullptr, &renderPass_));
  context.setVkObjectname(renderPass_, VK_OBJECT_TYPE_RENDER_PASS,
                          "Render pass: " + name);
}

RenderPass::RenderPass(const Context& context, const std::vector<VkFormat>& formats,
                       const std::vector<VkImageLayout>& initialLayouts,
                       const std::vector<VkImageLayout>& finalLayouts,
                       const std::vector<VkAttachmentLoadOp>& loadOp,
                       const std::vector<VkAttachmentStoreOp>& storeOp,
                       VkPipelineBindPoint bindPoint,
                       std::vector<uint32_t> resolveAttachmentsIndices,
                       uint32_t depthAttachmentIndex, uint32_t stencilAttachmentIndex,
                       VkAttachmentLoadOp stencilLoadOp,
                       VkAttachmentStoreOp stencilStoreOp, bool multiview,
                       const std::string& name)
    : device_{context.device()} {
  const bool sameSizes =
      formats.size() == initialLayouts.size() && formats.size() == finalLayouts.size() &&
      formats.size() == loadOp.size() && formats.size() == storeOp.size();
  ASSERT(sameSizes,
         "The sizes of the attachments and their load and store operations and final "
         "layouts must match");

  std::vector<VkAttachmentDescription> attachmentDescriptors;
  std::vector<VkAttachmentReference> colorAttachmentReferences;
  std::optional<VkAttachmentReference> depthStencilAttachmentReference;
  for (uint32_t index = 0; index < formats.size(); ++index) {
    attachmentDescriptors.emplace_back(VkAttachmentDescription{
        .format = formats[index],
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = loadOp[index],
        .storeOp = storeOp[index],
        .stencilLoadOp = index == stencilAttachmentIndex
                             ? stencilLoadOp
                             : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = index == stencilAttachmentIndex
                              ? stencilStoreOp
                              : VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = initialLayouts[index],
        .finalLayout = finalLayouts[index],
    });
    if (index == depthAttachmentIndex || index == stencilAttachmentIndex) {
      depthStencilAttachmentReference = VkAttachmentReference{
          .attachment = index,
          .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      };
    } else {
      colorAttachmentReferences.emplace_back(VkAttachmentReference{
          .attachment = index,
          .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      });
    }
  }

  const VkSubpassDescription spd = {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = static_cast<uint32_t>(colorAttachmentReferences.size()),
      .pColorAttachments = colorAttachmentReferences.data(),
      .pDepthStencilAttachment = depthStencilAttachmentReference.has_value()
                                     ? &depthStencilAttachmentReference.value()
                                     : nullptr,
  };

  std::array<VkSubpassDependency, 2> dependencies;
  dependencies[0] = {
      .srcSubpass = VK_SUBPASS_EXTERNAL,
      .dstSubpass = 0,
      .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                      VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
      .dstAccessMask =
          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
  };

  dependencies[1] = {
      .srcSubpass = 0,
      .dstSubpass = VK_SUBPASS_EXTERNAL,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                      VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
      .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                       VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                       VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
  };

  uint32_t viewmask = 0x00000003;
  uint32_t correlationMask = 0x00000003;
  const VkRenderPassMultiviewCreateInfo mvci = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,
      .subpassCount = 1,
      .pViewMasks = &viewmask,
      .correlationMaskCount = 1,
      .pCorrelationMasks = &correlationMask,
  };

  const VkRenderPassCreateInfo rpci = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .pNext = multiview ? &mvci : nullptr,
      .attachmentCount = static_cast<uint32_t>(attachmentDescriptors.size()),
      .pAttachments = attachmentDescriptors.data(),
      .subpassCount = 1,
      .pSubpasses = &spd,
      .dependencyCount = 2,
      .pDependencies =
          dependencies.data(),  // being extra liberal with this, in production this
                                // should be carefully provided depending upon each pass
  };
  VK_CHECK(vkCreateRenderPass(device_, &rpci, nullptr, &renderPass_));
  context.setVkObjectname(renderPass_, VK_OBJECT_TYPE_RENDER_PASS,
                          "Render pass: " + name);
}

RenderPass::RenderPass(const Context& context, const std::vector<VkFormat>& formats,
                       const std::vector<VkImageLayout>& initialLayouts,
                       const std::vector<VkImageLayout>& finalLayouts,
                       const std::vector<VkAttachmentLoadOp>& loadOp,
                       const std::vector<VkAttachmentStoreOp>& storeOp,
                       VkPipelineBindPoint bindPoint,
                       std::vector<uint32_t> resolveAttachmentsIndices,
                       uint32_t depthAttachmentIndex, uint32_t fragmentDensityMapIndex,
                       uint32_t stencilAttachmentIndex, VkAttachmentLoadOp stencilLoadOp,
                       VkAttachmentStoreOp stencilStoreOp, bool multiview,
                       const std::string& name)
    : device_{context.device()} {
  const bool sameSizes =
      formats.size() == initialLayouts.size() && formats.size() == finalLayouts.size() &&
      formats.size() == loadOp.size() && formats.size() == storeOp.size();
  ASSERT(sameSizes,
         "The sizes of the attachments and their load and store operations and final "
         "layouts must match");

  std::vector<VkAttachmentDescription> attachmentDescriptors;
  std::vector<VkAttachmentReference> colorAttachmentReferences;
  std::optional<VkAttachmentReference> depthStencilAttachmentReference;
  uint32_t fragmentDensityAttachmentReference = UINT32_MAX;
  for (uint32_t index = 0; index < formats.size(); ++index) {
    attachmentDescriptors.emplace_back(VkAttachmentDescription{
        .format = formats[index],
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = loadOp[index],
        .storeOp = storeOp[index],
        .stencilLoadOp = index == stencilAttachmentIndex
                             ? stencilLoadOp
                             : VK_ATTACHMENT_LOAD_OP_DONT_CARE,
        .stencilStoreOp = index == stencilAttachmentIndex
                              ? stencilStoreOp
                              : VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .initialLayout = initialLayouts[index],
        .finalLayout = finalLayouts[index],
    });
    if (index == depthAttachmentIndex || index == stencilAttachmentIndex) {
      depthStencilAttachmentReference = VkAttachmentReference{
          .attachment = index,
          .layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      };
    } else if (index == fragmentDensityMapIndex) {
      fragmentDensityAttachmentReference = index;
    } else {
      colorAttachmentReferences.emplace_back(VkAttachmentReference{
          .attachment = index,
          .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      });
    }
  }

#if defined(VK_EXT_fragment_density_map)
  const VkRenderPassFragmentDensityMapCreateInfoEXT fdmAttachmentci = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_FRAGMENT_DENSITY_MAP_CREATE_INFO_EXT,
      .fragmentDensityMapAttachment =
          {
              .attachment = fragmentDensityAttachmentReference,
              .layout = VK_IMAGE_LAYOUT_FRAGMENT_DENSITY_MAP_OPTIMAL_EXT,
          },
  };
#endif

  const VkSubpassDescription spd = {
      .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
      .colorAttachmentCount = static_cast<uint32_t>(colorAttachmentReferences.size()),
      .pColorAttachments = colorAttachmentReferences.data(),
      .pDepthStencilAttachment = depthStencilAttachmentReference.has_value()
                                     ? &depthStencilAttachmentReference.value()
                                     : nullptr,
  };

  std::array<VkSubpassDependency, 2> dependencies;
  dependencies[0] = {
      .srcSubpass = VK_SUBPASS_EXTERNAL,
      .dstSubpass = 0,
      .srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                      VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT |
                      VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
      .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
      .dstAccessMask =
          VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
          VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT,
  };

  dependencies[1] = {
      .srcSubpass = 0,
      .dstSubpass = VK_SUBPASS_EXTERNAL,
      .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                      VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                      VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
      .dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
      .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT |
                       VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                       VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT |
                       VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
  };

  uint32_t viewmask = 0x00000003;
  uint32_t correlationMask = 0x00000003;
  const VkRenderPassMultiviewCreateInfo mvci = {
    .sType = VK_STRUCTURE_TYPE_RENDER_PASS_MULTIVIEW_CREATE_INFO,
#if defined(VK_EXT_fragment_density_map)
    .pNext = fragmentDensityAttachmentReference < UINT32_MAX ? &fdmAttachmentci : nullptr,
#else
    .pNext = nullptr
#endif
    .subpassCount = 1,
    .pViewMasks = &viewmask,
    .correlationMaskCount = 1,
    .pCorrelationMasks = &correlationMask,
  };

  const VkRenderPassCreateInfo rpci = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
      .pNext = multiview ? &mvci : nullptr,
      .attachmentCount = static_cast<uint32_t>(attachmentDescriptors.size()),
      .pAttachments = attachmentDescriptors.data(),
      .subpassCount = 1,
      .pSubpasses = &spd,
      .dependencyCount = 2,
      .pDependencies =
          dependencies.data(),  // being extra liberal with this, in production this
                                // should be carefully provided depending upon each pass
  };
  VK_CHECK(vkCreateRenderPass(device_, &rpci, nullptr, &renderPass_));
  context.setVkObjectname(renderPass_, VK_OBJECT_TYPE_RENDER_PASS,
                          "Render pass (fdm support): " + name);
}

RenderPass::~RenderPass() { vkDestroyRenderPass(device_, renderPass_, nullptr); }

VkRenderPass RenderPass::vkRenderPass() const { return renderPass_; }

}  // namespace VulkanCore
