#include "OitWeightedPass.hpp"

#include <filesystem>

#include "enginecore/Camera.hpp"
#include "enginecore/Model.hpp"
#include "vulkancore/DynamicRendering.hpp"

constexpr uint32_t CAMERA_SET = 0;
constexpr uint32_t OBJECT_PROP_SET = 1;
constexpr uint32_t BINDING_CameraMVP = 0;
constexpr uint32_t BINDING_ObjectProperties = 0;

OitWeightedPass::OitWeightedPass() {}

void OitWeightedPass::init(VulkanCore::Context* context,
                           const EngineCore::RingBuffer& cameraBuffer,
                           EngineCore::RingBuffer& objectPropBuffer,
                           size_t objectPropSize, uint32_t numMeshes,
                           VkFormat colorTextureFormat, VkFormat depthTextureFormat,
                           std::shared_ptr<VulkanCore::Texture> opaquePassDepth) {
  context_ = context;
  colorTexture_ =
      context->createTexture(VK_IMAGE_TYPE_2D, VK_FORMAT_R16G16B16A16_SFLOAT, 0,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                             {
                                 .width = context->swapchain()->extent().width,
                                 .height = context->swapchain()->extent().height,
                                 .depth = 1,
                             },
      1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
                             "OIT Weighted Color Pass - Color Texture");

  alphaTexture_ =
      context->createTexture(VK_IMAGE_TYPE_2D, VK_FORMAT_R16_SFLOAT, 0,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                             {
                                 .width = context->swapchain()->extent().width,
                                 .height = context->swapchain()->extent().height,
                                 .depth = 1,
                             },
      1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
                             "OIT Weighted Color Pass - Alpha Texture");

  depthTexture_ = context->createTexture(
      VK_IMAGE_TYPE_2D, depthTextureFormat, 0,
      VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
          VK_IMAGE_USAGE_SAMPLED_BIT,
      {
          .width = context->swapchain()->extent().width,
          .height = context->swapchain()->extent().height,
          .depth = 1,
      },
      1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
      "OIT Weighted Color Pass - Depth attachment");

  sampler_ = context_->createSampler(
      VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT,
      VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, 100.0f,
      "OIT Weighted Color Pass - sampler");

  const auto resourcesFolder = std::filesystem::current_path() / "resources/shaders/";

  auto vertexShader = context_->createShaderModule(
      (resourcesFolder / "bindfull.vert").string(), VK_SHADER_STAGE_VERTEX_BIT,
      "OIT Weighted - vertex shader");
  auto fragmentShader = context_->createShaderModule(
      (resourcesFolder / "OitWeighted.frag").string(), VK_SHADER_STAGE_FRAGMENT_BIT,
      "OIT Weighted - fragment shader");

  const std::vector<VulkanCore::Pipeline::SetDescriptor> setLayout = {
      {
          .set_ = CAMERA_SET,  // set number
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding{BINDING_CameraMVP,
                                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                               VK_SHADER_STAGE_VERTEX_BIT},
              },
      },
      {
          .set_ = OBJECT_PROP_SET,  // set
                                    // number
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding{
                      BINDING_ObjectProperties, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
              },
      },
  };

  VkVertexInputBindingDescription bindingDesc = {
      .binding = 0,
      .stride = sizeof(EngineCore::Vertex),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };

  std::vector<std::pair<VkFormat, size_t>> vertexAttributesFormatAndOffset = {
      {VK_FORMAT_R32G32B32_SFLOAT, offsetof(EngineCore::Vertex, pos)},
      {VK_FORMAT_R32G32B32_SFLOAT, offsetof(EngineCore::Vertex, normal)},
      {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(EngineCore::Vertex, tangent)},
      {VK_FORMAT_R32G32_SFLOAT, offsetof(EngineCore::Vertex, texCoord)},
      {VK_FORMAT_R32_SINT, offsetof(EngineCore::Vertex, material)}};

  std::vector<VkVertexInputAttributeDescription> vertexInputAttributes;

  for (uint32_t i = 0; i < vertexAttributesFormatAndOffset.size(); ++i) {
    auto [format, offset] = vertexAttributesFormatAndOffset[i];
    vertexInputAttributes.push_back(VkVertexInputAttributeDescription{
        .location = i,
        .binding = 0,
        .format = format,
        .offset = uint32_t(offset),
    });
  }

  const VulkanCore::Pipeline::GraphicsPipelineDescriptor gpDesc = {
      .sets_ = setLayout,
      .vertexShader_ = vertexShader,
      .fragmentShader_ = fragmentShader,
      .dynamicStates_ = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR},
      .useDynamicRendering_ = true,
      .colorTextureFormats = {colorTexture_->vkFormat(), alphaTexture_->vkFormat()},
      .depthTextureFormat = depthTexture_->vkFormat(),
      .sampleCount = VK_SAMPLE_COUNT_1_BIT,
      .cullMode = VK_CULL_MODE_NONE,
      .viewport = context->swapchain()->extent(),
      .blendEnable = true,
      .numberBlendAttachments = 2,
      .depthTestEnable = false,
      .depthWriteEnable = true,
      .depthCompareOperation = VK_COMPARE_OP_LESS,
      .vertexInputCreateInfo =
          {
              .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
              .vertexBindingDescriptionCount = 1u,
              .pVertexBindingDescriptions = &bindingDesc,
              .vertexAttributeDescriptionCount = uint32_t(vertexInputAttributes.size()),
              .pVertexAttributeDescriptions = vertexInputAttributes.data(),
          },
      .blendAttachmentStates_ =
          {
              VkPipelineColorBlendAttachmentState{
                  .blendEnable = VK_TRUE,
                  .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
                  .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
                  .colorBlendOp = VK_BLEND_OP_ADD,
                  .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                  .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
                  .alphaBlendOp = VK_BLEND_OP_ADD,
                  .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
              },
              VkPipelineColorBlendAttachmentState{
                  .blendEnable = VK_TRUE,
                  .srcColorBlendFactor = VK_BLEND_FACTOR_ZERO,
                  .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
                  .colorBlendOp = VK_BLEND_OP_ADD,
                  .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                  .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                  .alphaBlendOp = VK_BLEND_OP_ADD,
                  .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
              },
          },
  };

  pipeline_ = context->createGraphicsPipeline(gpDesc, VK_NULL_HANDLE,
                                              "OIT Weighted ColorPass Pipeline");

  pipeline_->allocateDescriptors({
      {.set_ = CAMERA_SET, .count_ = 3},
      {.set_ = OBJECT_PROP_SET, .count_ = numMeshes},
  });

  pipeline_->bindResource(CAMERA_SET, BINDING_CameraMVP, 0, cameraBuffer.buffer(0), 0,
                          sizeof(UniformTransforms), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  pipeline_->bindResource(CAMERA_SET, BINDING_CameraMVP, 1, cameraBuffer.buffer(1), 0,
                          sizeof(UniformTransforms), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  pipeline_->bindResource(CAMERA_SET, BINDING_CameraMVP, 2, cameraBuffer.buffer(2), 0,
                          sizeof(UniformTransforms), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

  for (uint32_t meshIdx = 0; meshIdx < numMeshes; ++meshIdx) {
    pipeline_->bindResource(OBJECT_PROP_SET, BINDING_ObjectProperties, meshIdx,
                            objectPropBuffer.buffer(meshIdx), 0, objectPropSize,
                            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  }

  initCompositePipeline(colorTextureFormat);
}

void OitWeightedPass::draw(
    VkCommandBuffer commandBuffer, int index,
    const std::vector<std::shared_ptr<VulkanCore::Buffer>>& buffers, uint32_t numMeshes) {
  context_->beginDebugUtilsLabel(commandBuffer, "OIT Weighted ColorPass",
                                 {0.0f, 1.0f, 0.0f, 1.0f});

  const std::array<VkClearValue, 3> clearValues = {
      VkClearValue{
          .color =
              {
                  0.0,
                  0.0,
                  0.0,
                  0.0f,
              },
      },
      VkClearValue{
          .color =
              {
                  1.0,
                  0.0,
                  0.0,
                  0.0f,
              },
      },
      VkClearValue{
          .depthStencil =
              {
                  1.0f,
                  0,
              },
      },
  };

  colorTexture_->transitionImageLayout(commandBuffer,
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  alphaTexture_->transitionImageLayout(commandBuffer,
                                       VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  const std::vector<VulkanCore::DynamicRendering::AttachmentDescription>
      colorAttachmentDesc{
          VulkanCore::DynamicRendering::AttachmentDescription{
              .imageView = colorTexture_->vkImageView(),
              .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
              .attachmentLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
              .attachmentStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
              .clearValue = clearValues[0],
          },
          VulkanCore::DynamicRendering::AttachmentDescription{
              .imageView = alphaTexture_->vkImageView(),
              .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
              .attachmentLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
              .attachmentStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
              .clearValue = clearValues[1],
          },
      };

  const VulkanCore::DynamicRendering::AttachmentDescription depthAttachmentDesc{
      .imageView = depthTexture_->vkImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .attachmentLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .attachmentStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .clearValue = clearValues[2],
  };

  VulkanCore::DynamicRendering::beginRenderingCmd(
      commandBuffer, colorTexture_->vkImage(), 0,
      {{0, 0}, {colorTexture_->vkExtents().width, colorTexture_->vkExtents().height}}, 1,
      0, colorAttachmentDesc, &depthAttachmentDesc, nullptr, VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_UNDEFINED);

#pragma region Dynamic States
  const VkViewport viewport = {
      .x = 0.0f,
      .y = static_cast<float>(context_->swapchain()->extent().height),
      .width = static_cast<float>(context_->swapchain()->extent().width),
      .height = -static_cast<float>(context_->swapchain()->extent().height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
  const VkRect2D scissor = {
      .offset = {0, 0},
      .extent = context_->swapchain()->extent(),
  };
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
#pragma endregion

  pipeline_->bind(commandBuffer);

  for (uint32_t meshIdx = 0; meshIdx < numMeshes; ++meshIdx) {
    pipeline_->bindDescriptorSets(commandBuffer,
                                  {
                                      {.set = CAMERA_SET, .bindIdx = (uint32_t)index},
                                      {.set = OBJECT_PROP_SET, .bindIdx = meshIdx},
                                  });

    pipeline_->updateDescriptorSets();

    auto vertexbufferIndex = meshIdx * 2;
    auto indexbufferIndex = meshIdx * 2 + 1;

    pipeline_->bindVertexBuffer(commandBuffer, buffers[vertexbufferIndex]->vkBuffer());
    pipeline_->bindIndexBuffer(commandBuffer, buffers[indexbufferIndex]->vkBuffer());

    const auto vertexCount = buffers[indexbufferIndex]->size() / sizeof(uint32_t);

    vkCmdDrawIndexed(commandBuffer, vertexCount, 1, 0, 0, 0);
  }

  VulkanCore::DynamicRendering::endRenderingCmd(commandBuffer, colorTexture_->vkImage(),
                                                VK_IMAGE_LAYOUT_UNDEFINED,
                                                VK_IMAGE_LAYOUT_UNDEFINED);

  context_->endDebugUtilsLabel(commandBuffer);

  colorTexture_->transitionImageLayout(commandBuffer,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  alphaTexture_->transitionImageLayout(commandBuffer,
                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  context_->beginDebugUtilsLabel(commandBuffer, "OIT Weighted CompositePass",
                                 {0.0f, 1.0f, 1.0f, 1.0f});

  compositeColorTexture_->transitionImageLayout(commandBuffer,
                                                VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

  const VulkanCore::DynamicRendering::AttachmentDescription compositeColorAttachmentDesc{
      .imageView = compositeColorTexture_->vkImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .attachmentLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .attachmentStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearValues[0]};

  VulkanCore::DynamicRendering::beginRenderingCmd(
      commandBuffer, compositeColorTexture_->vkImage(), 0,
      {{0, 0},
       {compositeColorTexture_->vkExtents().width,
        compositeColorTexture_->vkExtents().height}},
      1, 0, {compositeColorAttachmentDesc}, &depthAttachmentDesc, nullptr,
      VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED);

  compositePipeline_->bind(commandBuffer);

  compositePipeline_->bindDescriptorSets(commandBuffer,
                                         {
                                             {.set = 0, .bindIdx = (uint32_t)0},
                                         });
  compositePipeline_->updateDescriptorSets();

  vkCmdDraw(commandBuffer, 4, 1, 0, 0);

  VulkanCore::DynamicRendering::endRenderingCmd(
      commandBuffer, compositeColorTexture_->vkImage(), VK_IMAGE_LAYOUT_UNDEFINED,
      VK_IMAGE_LAYOUT_UNDEFINED);

  compositeColorTexture_->transitionImageLayout(commandBuffer,
                                                VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  context_->endDebugUtilsLabel(commandBuffer);
}

void OitWeightedPass::initCompositePipeline(VkFormat colorTextureFormat) {
  compositeColorTexture_ =
      context_->createTexture(VK_IMAGE_TYPE_2D, colorTextureFormat, 0,
                              VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                  VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                              {
                                  .width = context_->swapchain()->extent().width,
                                  .height = context_->swapchain()->extent().height,
                                  .depth = 1,
                              },
      1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
                              "OIT Weighted Composite Pass - Color attachment");

  const auto resourcesFolder = std::filesystem::current_path() / "resources/shaders/";

  auto vertexShader =
      context_->createShaderModule((resourcesFolder / "fullscreen.vert").string(),
                                   VK_SHADER_STAGE_VERTEX_BIT, "main vertex");
  auto fragmentShader = context_->createShaderModule(
      (resourcesFolder / "OitWeightedComposite.frag").string(),
      VK_SHADER_STAGE_FRAGMENT_BIT, "main fragment");

  const std::vector<VulkanCore::Pipeline::SetDescriptor> setLayout = {
      {
          .set_ = 0,
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding{0,
                                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               1, VK_SHADER_STAGE_FRAGMENT_BIT},
                  VkDescriptorSetLayoutBinding{1,
                                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               1, VK_SHADER_STAGE_FRAGMENT_BIT},
              },

      },
  };

  const VulkanCore::Pipeline::GraphicsPipelineDescriptor gpDesc = {
      .sets_ = setLayout,
      .vertexShader_ = vertexShader,
      .fragmentShader_ = fragmentShader,
      .dynamicStates_ = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR},
      .useDynamicRendering_ = true,
      .colorTextureFormats = {compositeColorTexture_->vkFormat()},
      .depthTextureFormat = depthTexture_->vkFormat(),
      .primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
      .sampleCount = VK_SAMPLE_COUNT_1_BIT,
      .cullMode = VK_CULL_MODE_NONE,
      .viewport = context_->swapchain()->extent(),
      .depthTestEnable = false,
      .depthWriteEnable = false,
      .blendAttachmentStates_ =
          {
              VkPipelineColorBlendAttachmentState{
                  .blendEnable = VK_TRUE,
                  .srcColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                  .dstColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                  .colorBlendOp = VK_BLEND_OP_ADD,
                  .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                  .dstAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
                  .alphaBlendOp = VK_BLEND_OP_ADD,
                  .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                    VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
              },
          },
  };

  compositePipeline_ = context_->createGraphicsPipeline(
      gpDesc, VK_NULL_HANDLE, "OIT Weighted Composite pipeline");

  compositePipeline_->allocateDescriptors({
      {.set_ = 0, .count_ = 1},
  });

  compositePipeline_->bindResource(0, 0, 0, colorTexture_, sampler_);
  compositePipeline_->bindResource(0, 1, 0, alphaTexture_, sampler_);
}
