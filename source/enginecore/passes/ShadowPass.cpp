#include "ShadowPass.hpp"

#include <filesystem>

#include "vulkancore/Context.hpp"

constexpr uint32_t CAMERA_SET = 0;
constexpr uint32_t TEXTURES_SET = 1;
constexpr uint32_t SAMPLER_SET = 2;
constexpr uint32_t STORAGE_BUFFER_SET =
    3;  // storing vertex/index/indirect/material buffer in array
constexpr uint32_t BINDING_0 = 0;
constexpr uint32_t BINDING_1 = 1;
constexpr uint32_t BINDING_2 = 2;
constexpr uint32_t BINDING_3 = 3;

ShadowPass::ShadowPass() {}

void ShadowPass::init(VulkanCore::Context* context) {
  context_ = context;
  initTextures(context);
  renderPass_ = context->createRenderPass(
      {depthTexture_}, {VK_ATTACHMENT_LOAD_OP_CLEAR}, {VK_ATTACHMENT_STORE_OP_STORE},
      // final layout for all attachments
      {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}, VK_PIPELINE_BIND_POINT_GRAPHICS, {},
      "ShadowMap RenderPass");

  frameBuffer_ = context->createFramebuffer(renderPass_->vkRenderPass(), {depthTexture_},
                                            nullptr, nullptr, "ShadowMap framebuffer ");

  const auto resourcesFolder = std::filesystem::current_path() / "resources/shaders/";

  auto vertexShader =
      context->createShaderModule((resourcesFolder / "shadowpass.vert").string(),
                                  VK_SHADER_STAGE_VERTEX_BIT, "shadowmap vertex");
  auto fragmentShader =
      context->createShaderModule((resourcesFolder / "empty.frag").string(),
                                  VK_SHADER_STAGE_FRAGMENT_BIT, "shadowmap fragment");

  const std::vector<VulkanCore::Pipeline::SetDescriptor> setLayout = {
      {
          .set_ = CAMERA_SET,  // set number
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding{
                      0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
              },
      },
      {
          .set_ = TEXTURES_SET,  // set number
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding{
                      0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000,
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
              },
      },
      {
          .set_ = SAMPLER_SET,  // set number
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding{
                      0, VK_DESCRIPTOR_TYPE_SAMPLER, 1000,
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
              },
      },
      {
          .set_ = STORAGE_BUFFER_SET,
          .bindings_ =
              {
                  VkDescriptorSetLayoutBinding{
                      0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 4,
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
              },
      },
  };
  const VulkanCore::Pipeline::GraphicsPipelineDescriptor gpDesc = {
      .sets_ = setLayout,
      .vertexShader_ = vertexShader,
      .fragmentShader_ = fragmentShader,
      .dynamicStates_ = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR},
      .colorTextureFormats = {},
      .depthTextureFormat = VK_FORMAT_D24_UNORM_S8_UINT,
      .sampleCount = VK_SAMPLE_COUNT_1_BIT,
      .cullMode = VK_CULL_MODE_NONE,
      .viewport =
          VkExtent2D{depthTexture_->vkExtents().width, depthTexture_->vkExtents().height},
      .depthTestEnable = true,
      .depthWriteEnable = true,
      .depthCompareOperation = VK_COMPARE_OP_LESS,
  };

  pipeline_ = context->createGraphicsPipeline(gpDesc, renderPass_->vkRenderPass(),
                                              "ShadowMap pipeline");

  pipeline_->allocateDescriptors({
      {.set_ = CAMERA_SET, .count_ = 3},
      {.set_ = TEXTURES_SET, .count_ = 1},
      {.set_ = SAMPLER_SET, .count_ = 1},
      {.set_ = STORAGE_BUFFER_SET, .count_ = 1},
  });
}

void ShadowPass::render(VkCommandBuffer commandBuffer, int frameIndex,
                        const std::vector<VulkanCore::Pipeline::SetAndBindingIndex>& sets,
                        VkBuffer indexBuffer, VkBuffer indirectDrawBuffer,
                        uint32_t numMeshes, uint32_t bufferSize) {
  const std::array<VkClearValue, 1> clearValues = {VkClearValue{.depthStencil = {1.0f}}};
  const VkRenderPassBeginInfo renderpassInfo = {
      .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
      .renderPass = renderPass_->vkRenderPass(),
      .framebuffer = frameBuffer_->vkFramebuffer(),
      .renderArea = {.offset =
                         {
                             0,
                             0,
                         },
                     .extent =
                         {
                             .width = depthTexture_->vkExtents().width,
                             .height = depthTexture_->vkExtents().height,
                         }},
      .clearValueCount = static_cast<uint32_t>(clearValues.size()),
      .pClearValues = clearValues.data(),
  };

  context_->beginDebugUtilsLabel(commandBuffer, "ShadowMap Pass",
                                 {0.0f, 1.0f, 0.0f, 1.0f});

  vkCmdBeginRenderPass(commandBuffer, &renderpassInfo, VK_SUBPASS_CONTENTS_INLINE);

  const VkViewport viewport = {
      .x = 0.0f,
      .y = static_cast<float>(depthTexture_->vkExtents().height),
      .width = static_cast<float>(depthTexture_->vkExtents().width),
      .height = -static_cast<float>(depthTexture_->vkExtents().height),
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
      .extent =
          VkExtent2D{depthTexture_->vkExtents().width, depthTexture_->vkExtents().height},
  };
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
  pipeline_->bind(commandBuffer);
  pipeline_->bindDescriptorSets(commandBuffer, sets);
  pipeline_->updateDescriptorSets();

  vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

  vkCmdDrawIndexedIndirect(commandBuffer, indirectDrawBuffer, 0, numMeshes, bufferSize);

  vkCmdEndRenderPass(commandBuffer);
  context_->endDebugUtilsLabel(commandBuffer);
  depthTexture_->setImageLayout(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

void ShadowPass::initTextures(VulkanCore::Context* context) {
  depthTexture_ = context->createTexture(
      VK_IMAGE_TYPE_2D, VK_FORMAT_D24_UNORM_S8_UINT, 0,
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
          VK_IMAGE_USAGE_SAMPLED_BIT,
      {
          .width =
              context->swapchain()->extent().width * 4,  // 4x resolution for shadow maps
          .height = context->swapchain()->extent().height * 4,
          .depth = 1,
      },
      1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
      "ShadowMap Depth buffer");
}
