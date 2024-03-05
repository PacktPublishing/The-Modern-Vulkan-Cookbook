#include "GBufferPass.hpp"

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
constexpr uint32_t BINDING_4 = 4;

GBufferPass::GBufferPass() {}

void GBufferPass::init(VulkanCore::Context* context, unsigned int width,
                       unsigned int height) {
  context_ = context;
  initTextures(context, width, height);
  renderPass_ = context->createRenderPass(
      {gBufferBaseColorTexture_, gBufferNormalTexture_, gBufferEmissiveTexture_,
       gBufferSpecularTexture_, gBufferPositionTexture_, gBufferVelocityTexture_,
       depthTexture_},
      {VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_CLEAR,
       VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_CLEAR,
       VK_ATTACHMENT_LOAD_OP_CLEAR, VK_ATTACHMENT_LOAD_OP_CLEAR,
       VK_ATTACHMENT_LOAD_OP_CLEAR},
      {VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_STORE_OP_STORE,
       VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_STORE_OP_STORE,
       VK_ATTACHMENT_STORE_OP_STORE, VK_ATTACHMENT_STORE_OP_STORE,
       VK_ATTACHMENT_STORE_OP_STORE},
      // final layout for all attachments
      {VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
      VK_PIPELINE_BIND_POINT_GRAPHICS, {}, "GBuffer RenderPass");

  frameBuffer_ = context->createFramebuffer(
      renderPass_->vkRenderPass(),
      {gBufferBaseColorTexture_, gBufferNormalTexture_, gBufferEmissiveTexture_,
       gBufferSpecularTexture_, gBufferPositionTexture_, gBufferVelocityTexture_,
       depthTexture_},
      nullptr, nullptr, "GBuffer framebuffer ");

  const auto resourcesFolder = std::filesystem::current_path() / "resources/shaders/";

  auto vertexShader =
      context->createShaderModule((resourcesFolder / "gbuffer.vert").string(),
                                  VK_SHADER_STAGE_VERTEX_BIT, "gbuffer vertex");
  auto fragmentShader =
      context->createShaderModule((resourcesFolder / "gbuffer.frag").string(),
                                  VK_SHADER_STAGE_FRAGMENT_BIT, "gbuffer fragment");

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

  std::vector<VkPushConstantRange> pushConstants = {
      VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
          .offset = 0,
          .size = sizeof(GBufferPushConstants),
      },
  };

  const VulkanCore::Pipeline::GraphicsPipelineDescriptor gpDesc = {
      .sets_ = setLayout,
      .vertexShader_ = vertexShader,
      .fragmentShader_ = fragmentShader,
      .pushConstants_ = pushConstants,
      .dynamicStates_ =
          {
              VK_DYNAMIC_STATE_VIEWPORT,
              VK_DYNAMIC_STATE_SCISSOR,
          },
      .colorTextureFormats = {VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R16G16B16A16_SFLOAT,
                              VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R8G8B8A8_UNORM,
                              VK_FORMAT_R16G16B16A16_SFLOAT, VK_FORMAT_R32G32_SFLOAT},
      .depthTextureFormat = VK_FORMAT_D24_UNORM_S8_UINT,
      .sampleCount = VK_SAMPLE_COUNT_1_BIT,
      .cullMode = VK_CULL_MODE_NONE,
      .viewport = context->swapchain()->extent(),
      .depthTestEnable = true,
      .depthWriteEnable = true,
      .depthCompareOperation = VK_COMPARE_OP_LESS,
  };

  pipeline_ = context->createGraphicsPipeline(gpDesc, renderPass_->vkRenderPass(),
                                              "GBuffer pipeline");

  pipeline_->allocateDescriptors({
      {.set_ = CAMERA_SET, .count_ = 3},
      {.set_ = TEXTURES_SET, .count_ = 1},
      {.set_ = SAMPLER_SET, .count_ = 1},
      {.set_ = STORAGE_BUFFER_SET, .count_ = 1},
  });
}

void GBufferPass::render(
    VkCommandBuffer commandBuffer, int frameIndex,
    const std::vector<VulkanCore::Pipeline::SetAndBindingIndex>& sets,
    VkBuffer indexBuffer, VkBuffer indirectDrawBuffer, VkBuffer indirectDrawCountBuffer,
    uint32_t numMeshes, uint32_t bufferSize, bool applyJitter) {
  const std::array<VkClearValue, 7> clearValues = {
      VkClearValue{.color = {0.196f, 0.6f, 0.8f, 1.0f}},  // base color texture
      VkClearValue{.color = {0.0f, 0.0f, 0.0f, 1.0f}},    // normal texture
      VkClearValue{.color = {0.0f, 0.0f, 0.0f, 1.0f}},    // emissive texture
      VkClearValue{.color = {0.0f, 0.0f, 0.0f, 1.0f}},    // specular texture
      VkClearValue{.color = {0.0f, 0.0f, 0.0f, 0.0f}},    // position texture
      VkClearValue{.color = {0.0f, 0.0f, 0.0f, 0.0f}},    // velocity texture
      VkClearValue{.depthStencil = {1.0f}}};
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
                             .width = gBufferBaseColorTexture_->vkExtents().width,
                             .height = gBufferBaseColorTexture_->vkExtents().height,
                         }},
      .clearValueCount = static_cast<uint32_t>(clearValues.size()),
      .pClearValues = clearValues.data(),
  };

  context_->beginDebugUtilsLabel(commandBuffer, "GBuffer Pass", {0.0f, 1.0f, 0.0f, 1.0f});

  vkCmdBeginRenderPass(commandBuffer, &renderpassInfo, VK_SUBPASS_CONTENTS_INLINE);

  const VkViewport viewport = {
      .x = 0.0f,
      .y = static_cast<float>(gBufferBaseColorTexture_->vkExtents().height),
      .width = static_cast<float>(gBufferBaseColorTexture_->vkExtents().width),
      .height = -static_cast<float>(gBufferBaseColorTexture_->vkExtents().height),
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
      .extent = VkExtent2D{gBufferBaseColorTexture_->vkExtents().width,
                           gBufferBaseColorTexture_->vkExtents().height},
  };
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

  GBufferPushConstants pushConst{
      .applyJitter = uint32_t(applyJitter),
  };

  pipeline_->bind(commandBuffer);

  pipeline_->updatePushConstant(commandBuffer, VK_SHADER_STAGE_VERTEX_BIT,
                                sizeof(GBufferPushConstants), &pushConst);

  pipeline_->bindDescriptorSets(commandBuffer, sets);
  pipeline_->updateDescriptorSets();

  vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);

  vkCmdDrawIndexedIndirectCount(commandBuffer, indirectDrawBuffer, 0,
                                indirectDrawCountBuffer, 0, numMeshes, bufferSize);

  vkCmdEndRenderPass(commandBuffer);
  context_->endDebugUtilsLabel(commandBuffer);
  gBufferBaseColorTexture_->setImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  gBufferNormalTexture_->setImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  gBufferEmissiveTexture_->setImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  gBufferSpecularTexture_->setImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  gBufferPositionTexture_->setImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  gBufferVelocityTexture_->setImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  depthTexture_->setImageLayout(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void GBufferPass::initTextures(VulkanCore::Context* context, unsigned int width,
                               unsigned int height) {
  gBufferBaseColorTexture_ = context->createTexture(
      VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, 0,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
          VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
          VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      {
          .width = width,
          .height = height,
          .depth = 1,
      },
      1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
      "GBuffer BaseColorTexture");

  gBufferNormalTexture_ = context->createTexture(
      VK_IMAGE_TYPE_2D, VK_FORMAT_R16G16B16A16_SFLOAT, 0,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
          VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
          VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      {
          .width = width,
          .height = height,
          .depth = 1,
      },
      1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
      "GBuffer NormalColorTexture");

  gBufferEmissiveTexture_ = context->createTexture(
      VK_IMAGE_TYPE_2D, VK_FORMAT_R16G16B16A16_SFLOAT, 0,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
          VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
          VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      {
          .width = width,
          .height = height,
          .depth = 1,
      },
      1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
      "GBuffer EmissiveColorTexture");

  gBufferSpecularTexture_ = context->createTexture(
      VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, 0,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
          VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
          VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      {
          .width = context->swapchain()->extent().width,
          .height = context->swapchain()->extent().height,
          .depth = 1,
      },
      1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
      "GBuffer SpecularColorTexture");

  gBufferVelocityTexture_ = context->createTexture(
      VK_IMAGE_TYPE_2D, VK_FORMAT_R32G32_SFLOAT, 0,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
          VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
          VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      {
          .width = width,
          .height = height,
          .depth = 1,
      },
      1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
      "GBuffer Velocity texture");

  gBufferPositionTexture_ = context->createTexture(
      VK_IMAGE_TYPE_2D, VK_FORMAT_R16G16B16A16_SFLOAT, 0,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
          VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
          VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      {
          .width = width,
          .height = height,
          .depth = 1,
      },
      1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
      "GBuffer PositionTexture");

  depthTexture_ = context->createTexture(VK_IMAGE_TYPE_2D, VK_FORMAT_D24_UNORM_S8_UINT, 0,
                                         VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT |
                                             VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                             VK_IMAGE_USAGE_SAMPLED_BIT,
                                         {
                                             .width = width,
                                             .height = height,
                                             .depth = 1,
                                         },
                                         1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false,
                                         VK_SAMPLE_COUNT_1_BIT, "GBuffer Depth buffer");
}
