#include "FullScreenColorPass.hpp"

#include <filesystem>

#include "vulkancore/DynamicRendering.hpp"

struct alignas(16) FullScreenPushConst {
  glm::vec4 color0;
  glm::vec4 color1;
  glm::vec4 color2;
  glm::vec4 color3;
};

void FullScreenColorPass::init(VulkanCore::Context* context,
                               std::vector<VkFormat> colorTextureFormats) {
  context_ = context;
  width_ = context->swapchain()->extent().width;
  height_ = context->swapchain()->extent().height;

  const auto resourcesFolder = std::filesystem::current_path() / "resources/shaders/";

  auto vertexShader =
      context->createShaderModule((resourcesFolder / "fullscreenColor.vert").string(),
                                  VK_SHADER_STAGE_VERTEX_BIT, "full screen color vertex");
  auto fragmentShader = context->createShaderModule(
      (resourcesFolder / "fullscreenColor.frag").string(), VK_SHADER_STAGE_FRAGMENT_BIT,
      "full screen color fragment");

  const std::vector<VulkanCore::Pipeline::SetDescriptor> setLayout;
  //= {
  //    {
  //        .set_ = 0,  // set number
  //        .bindings_ =
  //            {
  //                // vector of bindings
  //                VkDescriptorSetLayoutBinding{0,
  //                                             VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
  //                                             1, VK_SHADER_STAGE_FRAGMENT_BIT},
  //            },
  //    },
  //};

  constexpr auto size = sizeof(FullScreenPushConst);
  const std::vector<VkPushConstantRange> pushConstants = {
      VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
          .offset = 0,
          .size = size,
      },
  };
  const VulkanCore::Pipeline::GraphicsPipelineDescriptor gpDesc = {
      .sets_ = setLayout,
      .vertexShader_ = vertexShader,
      .fragmentShader_ = fragmentShader,
      .pushConstants_ = pushConstants,
      .dynamicStates_ = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR},
      .useDynamicRendering_ = true,
      .colorTextureFormats = colorTextureFormats,
      .primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
      .sampleCount = VK_SAMPLE_COUNT_1_BIT,
      .cullMode = VK_CULL_MODE_NONE,
      .viewport = context->swapchain()->extent(),
      .depthTestEnable = false,
      .depthWriteEnable = false,
  };

  pipeline_ = context->createGraphicsPipeline(gpDesc, VK_NULL_HANDLE,
                                              "fullScreenColorPass pipeline");

  // pipeline_->allocateDescriptors({
  //     {.set_ = 0, .count_ = 1},
  // });
}

void FullScreenColorPass::render(VkCommandBuffer commandBuffer, uint32_t index,
                                 const std::vector<glm::vec4>& color) {
  const std::array<VkClearValue, 1> clearValues = {
      VkClearValue{.color = {0.0, 1.0, 0.0, 0.0f}}};

  context_->beginDebugUtilsLabel(commandBuffer, "FullScreenColor Pass",
                                 {0.0f, 0.5f, 0.7f, 1.0f});
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
  pushConst.color0 = color[0];
  pushConst.color1 = color[1];
  pushConst.color2 = color[2];
  pushConst.color3 = color[3];

  pipeline_->updatePushConstant(commandBuffer, VK_SHADER_STAGE_VERTEX_BIT,
                                sizeof(FullScreenPushConst), &pushConst);

  pipeline_->bind(commandBuffer);
  // pipeline_->bindDescriptorSets(commandBuffer, {
  //                                                  {.set = 0, .bindIdx = (uint32_t)0},
  //                                              });
  // pipeline_->updateDescriptorSets();

  vkCmdDraw(commandBuffer, 4, 1, 0, 0);

  VulkanCore::DynamicRendering::endRenderingCmd(
      commandBuffer, context_->swapchain()->texture(index)->vkImage(),
      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED);

  context_->endDebugUtilsLabel(commandBuffer);
}
