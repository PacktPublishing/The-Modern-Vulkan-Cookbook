#include "FullScreenPass.hpp"

#include <filesystem>

#include "../ImguiManager.hpp"
#include "vulkancore/DynamicRendering.hpp"

struct FullScreenPushConst {
  glm::vec4 showAsDepth;
};

FullScreenPass::FullScreenPass(bool useDynamicRendering)
    : useDynamicRendering_(useDynamicRendering) {}

void FullScreenPass::init(VulkanCore::Context* context,
                          std::vector<VkFormat> colorTextureFormats) {
  context_ = context;
  width_ = context->swapchain()->extent().width;
  height_ = context->swapchain()->extent().height;

  if (!useDynamicRendering_) {
    renderPass_ = context->createRenderPass(
        {context->swapchain()->texture(0)}, {VK_ATTACHMENT_LOAD_OP_CLEAR},
        {VK_ATTACHMENT_STORE_OP_STORE}, {VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
        VK_PIPELINE_BIND_POINT_GRAPHICS, {}, "fullscreen render pass ");

    frameBuffers_.resize(context->swapchain()->numberImages());
    for (size_t index = 0; index < context->swapchain()->numberImages(); ++index) {
      frameBuffers_[index] = context->createFramebuffer(
          renderPass_->vkRenderPass(), {context->swapchain()->texture(index)}, nullptr,
          nullptr, "swapchain framebuffer " + std::to_string(frameBuffers_.size()));
    }
  }

  const auto resourcesFolder = std::filesystem::current_path() / "resources/shaders/";

  auto vertexShader =
      context->createShaderModule((resourcesFolder / "fullscreen.vert").string(),
                                  VK_SHADER_STAGE_VERTEX_BIT, "main vertex");
  auto fragmentShader =
      context->createShaderModule((resourcesFolder / "fullscreen.frag").string(),
                                  VK_SHADER_STAGE_FRAGMENT_BIT, "main fragment");

  const std::vector<VulkanCore::Pipeline::SetDescriptor> setLayout = {
      {
          .set_ = 0,  // set number
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding{0,
                                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               1, VK_SHADER_STAGE_FRAGMENT_BIT},
              },
      },
  };

  std::vector<VkPushConstantRange> pushConstants = {
      VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .offset = 0,
          .size = sizeof(FullScreenPushConst),
      },
  };
  const VulkanCore::Pipeline::GraphicsPipelineDescriptor gpDesc = {
      .sets_ = setLayout,
      .vertexShader_ = vertexShader,
      .fragmentShader_ = fragmentShader,
      .pushConstants_ = pushConstants,
      .dynamicStates_ = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR},
      .useDynamicRendering_ = useDynamicRendering_ ? true : false,
      .colorTextureFormats = colorTextureFormats,
      .primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
      .sampleCount = VK_SAMPLE_COUNT_1_BIT,
      .cullMode = VK_CULL_MODE_NONE,
      .viewport = context->swapchain()->extent(),
      .depthTestEnable = false,
      .depthWriteEnable = false,
  };

  pipeline_ = context->createGraphicsPipeline(
      gpDesc, useDynamicRendering_ ? VK_NULL_HANDLE : renderPass_->vkRenderPass(),
      "fullScreenPass pipeline");

  pipeline_->allocateDescriptors({
      {.set_ = 0, .count_ = 1},
  });
}

void FullScreenPass::render(VkCommandBuffer commandBuffer, uint32_t index,
                            EngineCore::GUI::ImguiManager* imgui, bool showAsDepth) {
  const std::array<VkClearValue, 1> clearValues = {
      VkClearValue{.color = {0.0, 1.0, 0.0, 0.0f}}};

  context_->beginDebugUtilsLabel(commandBuffer, "FullScreen Pass",
                                 {0.0f, 0.0f, 1.0f, 1.0f});
  if (!useDynamicRendering_) {
    const VkRenderPassBeginInfo renderpassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass_->vkRenderPass(),
        .framebuffer = frameBuffers_[index]->vkFramebuffer(),
        .renderArea = {.offset =
                           {
                               0,
                               0,
                           },
                       .extent =
                           {
                               .width = width_,
                               .height = height_,
                           }},
        .clearValueCount = static_cast<uint32_t>(clearValues.size()),
        .pClearValues = clearValues.data(),
    };

    vkCmdBeginRenderPass(commandBuffer, &renderpassInfo, VK_SUBPASS_CONTENTS_INLINE);
  } else {
    const VulkanCore::DynamicRendering::AttachmentDescription colorAttachmentDesc{
        .imageView = context_->swapchain()->texture(index)->vkImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveModeFlagBits = VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .attachmentLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .attachmentStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clearValues[0]};

    VulkanCore::DynamicRendering::beginRenderingCmd(
        commandBuffer, context_->swapchain()->texture(index)->vkImage(), 0,
        {{0, 0},
         {context_->swapchain()->texture(index)->vkExtents().width,
          context_->swapchain()->texture(index)->vkExtents().height}},
        1, 0, {colorAttachmentDesc}, nullptr, nullptr);
  }
  const VkViewport viewport = {
      .x = 0.0f,
      .y = static_cast<float>(height_),
      .width = static_cast<float>(width_),
      .height = -static_cast<float>(height_),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
  const VkRect2D scissor = {
      .offset =
          {
              0,
              0,
          },
      .extent = VkExtent2D{width_, height_},
  };
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

  FullScreenPushConst pushConst;
  pushConst.showAsDepth.x = showAsDepth;

  pipeline_->updatePushConstant(commandBuffer, VK_SHADER_STAGE_FRAGMENT_BIT,
                                sizeof(FullScreenPushConst), &pushConst);

  pipeline_->bind(commandBuffer);
  pipeline_->bindDescriptorSets(commandBuffer, {
                                                   {.set = 0, .bindIdx = (uint32_t)0},
                                               });
  pipeline_->updateDescriptorSets();

  vkCmdDraw(commandBuffer, 4, 1, 0, 0);

#if defined(_WIN32)

    if (imgui) {
    imgui->recordCommands(commandBuffer);
  }
#endif
  if (!useDynamicRendering_) {
    vkCmdEndRenderPass(commandBuffer);
  } else {
#if defined(_WIN32)

      if (imgui) {
      VulkanCore::DynamicRendering::endRenderingCmd(
          commandBuffer, context_->swapchain()->texture(index)->vkImage());
    } else {
      VulkanCore::DynamicRendering::endRenderingCmd(
          commandBuffer, context_->swapchain()->texture(index)->vkImage(),
          VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED);
    }
#endif
  }
  context_->endDebugUtilsLabel(commandBuffer);
}
