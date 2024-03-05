#include "DualDepthPeeling.hpp"

#include <filesystem>

#include "vulkancore/Context.hpp"
#include "vulkancore/DynamicRendering.hpp"
#include "vulkancore/RenderPass.hpp"

constexpr uint32_t SET_0 = 0;
constexpr uint32_t CAMERA_SET = 0;
constexpr uint32_t OBJECT_PROP_SET = 1;
constexpr uint32_t DEPTH_ATTACHMENTS_SET = 2;
constexpr uint32_t BINDING_0 = 0;
constexpr uint32_t BINDING_1 = 1;

constexpr uint32_t BINDING_PEEL_DEPTH = 0;
constexpr uint32_t BINDING_OPAQUE_DEPTH = 1;
constexpr uint32_t BINDING_TEMPCOLOR_DEPTH = 2;

void DualDepthPeeling::draw(
    VkCommandBuffer commandBuffer, int index,
    const std::vector<std::shared_ptr<VulkanCore::Buffer>>& buffers, uint32_t numMeshes) {
  // Clear Depth 0
  {
    const VkClearColorValue clearColor = {-99999.0f, 99999.0f, 0.0f, 0.0f};
    const VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    depthMinMaxTextures_[0]->transitionImageLayout(commandBuffer,
                                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkCmdClearColorImage(commandBuffer, depthMinMaxTextures_[0]->vkImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
  }
  // Clear Depth 1
  {
    const VkClearColorValue clearColor = {0.0f, 1.0f, 0.0f, 0.0f};
    const VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    depthMinMaxTextures_[1]->transitionImageLayout(commandBuffer,
                                                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkCmdClearColorImage(commandBuffer, depthMinMaxTextures_[1]->vkImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
  }
  // Clear color attachments
  {
    const VkClearColorValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
    const VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    for (uint32_t i = 0; i < colorTextures_.size(); ++i) {
      colorTextures_[i]->transitionImageLayout(commandBuffer,
                                               VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

      vkCmdClearColorImage(commandBuffer, colorTextures_[i]->vkImage(),
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
    }
  }

  depthMinMaxTextures_[0]->transitionImageLayout(
      commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
  depthMinMaxTextures_[1]->transitionImageLayout(
      commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  VulkanCore::DynamicRendering::AttachmentDescription colorAttachmentDesc_Front{
      .imageView = colorTextures_[0]->vkImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .attachmentLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
      .attachmentStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
  };
  VulkanCore::DynamicRendering::AttachmentDescription colorAttachmentDesc_Back{
      .imageView = colorTextures_[1]->vkImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .attachmentLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
      .attachmentStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
  };
  const VkClearValue clearDepthMinMax = {
      .color = {-99999.0f, -99999.0f, 0.0f, 0.0f},
  };
  VulkanCore::DynamicRendering::AttachmentDescription depthMinMaxAttachmentDesc{
      .imageView = depthMinMaxTextures_[0]->vkImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .attachmentLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .attachmentStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearDepthMinMax,
  };
  const VkClearValue clearDepth = {
      .depthStencil = {1.0f},
  };

  for (uint32_t currentPeel = 0; currentPeel < numPeels_; ++currentPeel) {
    const uint32_t readIdx = currentPeel % 2;

    colorTextures_[0]->transitionImageLayout(commandBuffer,
                                             VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    colorTextures_[1]->transitionImageLayout(commandBuffer,
                                             VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    depthMinMaxTextures_[currentPeel % 2]->transitionImageLayout(
        commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    depthMinMaxTextures_[(currentPeel + 1) % 2]->transitionImageLayout(
        commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    depthMinMaxAttachmentDesc.imageView = depthMinMaxTextures_[readIdx]->vkImageView();

    context_->beginDebugUtilsLabel(
        commandBuffer, "Dual Depth Peeling: peel " + std::to_string(currentPeel),
        {1.0f, .55f, 0.0f, 1.0f});

    VulkanCore::DynamicRendering::beginRenderingCmd(
        commandBuffer, colorTextures_[0]->vkImage(), 0,
        {{0, 0},
         {colorTextures_[0]->vkExtents().width, colorTextures_[0]->vkExtents().height}},
        1, 0,
        {depthMinMaxAttachmentDesc, colorAttachmentDesc_Front, colorAttachmentDesc_Back},
        nullptr, nullptr, colorTextures_[0]->vkLayout(),
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport_);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor_);

    pipeline_->bind(commandBuffer);

    for (uint32_t meshIdx = 0; meshIdx < numMeshes; ++meshIdx) {
      const auto vertexbufferIndex = meshIdx * 2;
      const auto indexbufferIndex = meshIdx * 2 + 1;

      pipeline_->bindVertexBuffer(commandBuffer, buffers[vertexbufferIndex]->vkBuffer());
      pipeline_->bindIndexBuffer(commandBuffer, buffers[indexbufferIndex]->vkBuffer());

      pipeline_->bindDescriptorSets(
          commandBuffer,
          {
              {.set = CAMERA_SET,
               .bindIdx = (uint32_t)context_->swapchain()->currentImageIndex()},
              {.set = OBJECT_PROP_SET, .bindIdx = meshIdx},
              {.set = DEPTH_ATTACHMENTS_SET, .bindIdx = (currentPeel % 2)},
          });

      pipeline_->updateDescriptorSets();

      const auto vertexCount = buffers[indexbufferIndex]->size() / sizeof(uint32_t);

      vkCmdDrawIndexed(commandBuffer, vertexCount, 1, 0, 0, 0);
    }

    VulkanCore::DynamicRendering::endRenderingCmd(
        commandBuffer, colorTextures_[0]->vkImage(), VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_UNDEFINED);

    context_->endDebugUtilsLabel(commandBuffer);
  }

  {
    VulkanCore::DynamicRendering::AttachmentDescription finalAttachmentDesc{
        .imageView = colorTextures_[0]->vkImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .attachmentLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
        .attachmentStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
    };

    VulkanCore::DynamicRendering::beginRenderingCmd(
        commandBuffer, colorTextures_[0]->vkImage(), 0,
        {{0, 0}, {colorTexture()->vkExtents().width, colorTexture()->vkExtents().height}},
        1, 0, {finalAttachmentDesc}, nullptr, nullptr, colorTextures_[0]->vkLayout(),
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport_);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor_);

    pipelineFinal_->bind(commandBuffer);
    pipelineFinal_->bindDescriptorSets(commandBuffer, {
                                                          {
                                                              .set = SET_0,
                                                              .bindIdx = 0,
                                                          },
                                                      });

    pipelineFinal_->updateDescriptorSets();

    vkCmdDraw(commandBuffer, 4, 1, 0, 0);

    VulkanCore::DynamicRendering::endRenderingCmd(
        commandBuffer, colorTextures_[0]->vkImage(), VK_IMAGE_LAYOUT_UNDEFINED,
        VK_IMAGE_LAYOUT_UNDEFINED);
  }
}

void DualDepthPeeling::init(
    uint32_t numMeshes, const VkVertexInputBindingDescription& vertexBindingDesc,
    const std::vector<VkVertexInputAttributeDescription>& vertexDescription) {
  ASSERT(!colorTextures_.empty(),
         "The number of color attachments for the dual depth peeling pass cannot be zero")
  ASSERT(depthMinMaxTextures_[0], "Depth texture 0 cannot be nullptr")

  sampler_ = std::make_shared<VulkanCore::Sampler>(
      *context_, VK_FILTER_NEAREST, VK_FILTER_NEAREST,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 1, "dual depth peeling");

  // Pipeline for passes/peels
  {
    const auto resourcesFolder = std::filesystem::current_path() / "resources/shaders/";

    auto vertexShader = context_->createShaderModule(
        (resourcesFolder / "depthPeel.vert").string(), VK_SHADER_STAGE_VERTEX_BIT,
        "Dual Depth Peeling vertex");
    auto fragmentShader = context_->createShaderModule(
        (resourcesFolder / "dualDepthPeel.frag").string(), VK_SHADER_STAGE_FRAGMENT_BIT,
        "Dual Depth Peeling fragment");

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
            .set_ = OBJECT_PROP_SET,  // set number
            .bindings_ =
                {
                    // vector of bindings
                    VkDescriptorSetLayoutBinding{
                        0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
                },
        },
        {
            .set_ = DEPTH_ATTACHMENTS_SET,  // set number
            .bindings_ =
                {
                    // vector of bindings
                    VkDescriptorSetLayoutBinding{
                        BINDING_PEEL_DEPTH, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
                    VkDescriptorSetLayoutBinding{
                        BINDING_OPAQUE_DEPTH, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                        1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
                },
        },
    };
    const VulkanCore::Pipeline::GraphicsPipelineDescriptor gpDesc = {
        .sets_ = setLayout,
        .vertexShader_ = vertexShader,
        .fragmentShader_ = fragmentShader,
        .dynamicStates_ = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR},
        .useDynamicRendering_ = true,
        .colorTextureFormats =
            {
                VK_FORMAT_R32G32_SFLOAT,
                colorTextures_[0]->vkFormat(),
                colorTextures_[0]->vkFormat(),
            },
        .sampleCount = VK_SAMPLE_COUNT_1_BIT,
        .cullMode = VK_CULL_MODE_NONE,
        .viewport = context_->swapchain()->extent(),
        .depthTestEnable = false,
        .depthWriteEnable = true,
        .depthCompareOperation = VK_COMPARE_OP_LESS,
        .vertexInputCreateInfo =
            {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
                .vertexBindingDescriptionCount = 1u,
                .pVertexBindingDescriptions = &vertexBindingDesc,
                .vertexAttributeDescriptionCount = uint32_t(vertexDescription.size()),
                .pVertexAttributeDescriptions = vertexDescription.data(),
            },
        .blendAttachmentStates_ =
            {{
                 // depth
                 .blendEnable = true,
                 .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,            // Optional
                 .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,  // Optional
                 .colorBlendOp = VK_BLEND_OP_MAX,
                 .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,  // Optional
                 .dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA,  // Optional
                 .alphaBlendOp = VK_BLEND_OP_MAX,                   // Optional
                 .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
             },
             {
                 // front color attachment
                 .blendEnable = true,
                 .srcColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA,
                 .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
                 .colorBlendOp = VK_BLEND_OP_ADD,
                 .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
                 .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                 .alphaBlendOp = VK_BLEND_OP_ADD,  // Optional
                 .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
             },
             {
                 // back color attachment
                 .blendEnable = true,
                 .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,            // Optional
                 .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,  // Optional
                 .colorBlendOp = VK_BLEND_OP_ADD,
                 .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,                 // Optional
                 .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,  // Optional
                 .alphaBlendOp = VK_BLEND_OP_ADD,                             // Optional
                 .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                   VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
             }},
    };

    pipeline_ = context_->createGraphicsPipeline(gpDesc, VK_NULL_HANDLE,
                                                 "dual depth peeling passes/peels");

    pipeline_->allocateDescriptors({
        {.set_ = CAMERA_SET, .count_ = 3},
        {.set_ = OBJECT_PROP_SET, .count_ = numMeshes},
        {.set_ = DEPTH_ATTACHMENTS_SET, .count_ = 2},
    });
  }

  // Pipeline for final blend pass
  {
    const auto resourcesFolder = std::filesystem::current_path() / "resources/shaders/";

    auto vertexShader = context_->createShaderModule(
        (resourcesFolder / "fullscreen.vert").string(), VK_SHADER_STAGE_VERTEX_BIT,
        "dual depth peeling vertex final");
    auto fragmentShader = context_->createShaderModule(
        (resourcesFolder / "dualDepthPeelFinal.frag").string(),
        VK_SHADER_STAGE_FRAGMENT_BIT, "dual depth peeling fragment final");

    const std::vector<VulkanCore::Pipeline::SetDescriptor> setLayoutFinal = {
        {
            .set_ = SET_0,  // set number
            .bindings_ =
                {
                    // vector of bindings
                    VkDescriptorSetLayoutBinding{
                        0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                        VK_SHADER_STAGE_FRAGMENT_BIT},
                    VkDescriptorSetLayoutBinding{
                        1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                        VK_SHADER_STAGE_FRAGMENT_BIT},
                },
        },
    };
    const VulkanCore::Pipeline::GraphicsPipelineDescriptor gpDesc = {
        .sets_ = setLayoutFinal,
        .vertexShader_ = vertexShader,
        .fragmentShader_ = fragmentShader,
        //.pushConstants_ = pushConstants,
        .dynamicStates_ = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR},
        .useDynamicRendering_ = true,
        .colorTextureFormats = {colorTextures_[0]->vkFormat()},
        .primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        .sampleCount = VK_SAMPLE_COUNT_1_BIT,
        .cullMode = VK_CULL_MODE_NONE,
        .viewport = context_->swapchain()->extent(),
        .depthTestEnable = false,
        .depthWriteEnable = false,
    };

    pipelineFinal_ = context_->createGraphicsPipeline(gpDesc, VK_NULL_HANDLE,
                                                      "dual depth peeling final");

    pipelineFinal_->allocateDescriptors({
        {.set_ = SET_0, .count_ = 1},
    });

    pipelineFinal_->bindResource(SET_0, BINDING_0, 0, colorTextures_[0], sampler_,
                                 VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
    pipelineFinal_->bindResource(SET_0, BINDING_1, 0, colorTextures_[1], sampler_,
                                 VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  }
}

void DualDepthPeeling::init(VulkanCore::Context* context,
                            const EngineCore::RingBuffer& cameraBuffer,
                            EngineCore::RingBuffer& objectPropBuffer,
                            size_t objectPropSize, uint32_t numMeshes, uint32_t numPeels,
                            VkFormat colorTextureFormat, VkFormat depthTextureFormat,
                            std::shared_ptr<VulkanCore::Texture> opaquePassDepth) {
  numPeels_ = numPeels;
  context_ = context;

  initColorTextures(colorTextureFormat);
  initDepthTextures(depthTextureFormat);

  const VkVertexInputBindingDescription bindingDesc = {
      .binding = 0,
      .stride = sizeof(EngineCore::Vertex),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };
  const std::vector<std::pair<VkFormat, size_t>> vertexAttributesFormatAndOffset = {
      {VK_FORMAT_R32G32B32_SFLOAT, offsetof(EngineCore::Vertex, pos)},
      {VK_FORMAT_R32G32B32_SFLOAT, offsetof(EngineCore::Vertex, normal)},
      {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(EngineCore::Vertex, tangent)},
      {VK_FORMAT_R32G32_SFLOAT, offsetof(EngineCore::Vertex, texCoord)},
      {VK_FORMAT_R32_SINT, offsetof(EngineCore::Vertex, material)}};

  std::vector<VkVertexInputAttributeDescription> vertexInputAttributes;

  for (uint32_t i = 0; i < vertexAttributesFormatAndOffset.size(); ++i) {
    const auto [format, offset] = vertexAttributesFormatAndOffset[i];
    vertexInputAttributes.push_back(VkVertexInputAttributeDescription{
        .location = i,
        .binding = 0,
        .format = format,
        .offset = uint32_t(offset),
    });
  }

  init(numMeshes, bindingDesc, vertexInputAttributes);

  pipeline_->bindResource(CAMERA_SET, BINDING_0, 0, cameraBuffer.buffer(0), 0,
                          sizeof(UniformTransforms), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  pipeline_->bindResource(CAMERA_SET, BINDING_0, 1, cameraBuffer.buffer(1), 0,
                          sizeof(UniformTransforms), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  pipeline_->bindResource(CAMERA_SET, BINDING_0, 2, cameraBuffer.buffer(2), 0,
                          sizeof(UniformTransforms), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

  pipeline_->bindResource(DEPTH_ATTACHMENTS_SET, BINDING_PEEL_DEPTH, 0,
                          depthMinMaxTextures_[1], sampler_,
                          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  pipeline_->bindResource(DEPTH_ATTACHMENTS_SET, BINDING_OPAQUE_DEPTH, 0,
                          colorTextures_[0], sampler_,
                          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

  pipeline_->bindResource(DEPTH_ATTACHMENTS_SET, BINDING_PEEL_DEPTH, 1,
                          depthMinMaxTextures_[0], sampler_,
                          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  pipeline_->bindResource(DEPTH_ATTACHMENTS_SET, BINDING_OPAQUE_DEPTH, 1,
                          colorTextures_[0], sampler_,
                          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

  for (uint32_t meshIdx = 0; meshIdx < numMeshes; ++meshIdx) {
    pipeline_->bindResource(OBJECT_PROP_SET, BINDING_0, meshIdx,
                            objectPropBuffer.buffer(meshIdx), 0, objectPropSize,
                            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  }
}

void DualDepthPeeling::initDepthTextures(VkFormat depthFormat) {
  scissor_ = {
      .offset = {0, 0},
      .extent = VkExtent2D{context_->swapchain()->extent().width,
                           context_->swapchain()->extent().height},
  };

  viewport_ = {
      .x = 0.0f,
      .y = static_cast<float>(context_->swapchain()->extent().height),
      .width = static_cast<float>(context_->swapchain()->extent().width),
      .height = -static_cast<float>(context_->swapchain()->extent().height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };

  depthMinMaxTextures_[0] = context_->createTexture(
      VK_IMAGE_TYPE_2D, VK_FORMAT_R32G32_SFLOAT, 0,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
          VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      {
          .width = context_->swapchain()->extent().width,
          .height = context_->swapchain()->extent().height,
          .depth = 1,
      },
      1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
      "depth peeling - min/max depth even");

  depthMinMaxTextures_[1] = context_->createTexture(
      VK_IMAGE_TYPE_2D, VK_FORMAT_R32G32_SFLOAT, 0,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
          VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      {
          .width = context_->swapchain()->extent().width,
          .height = context_->swapchain()->extent().height,
          .depth = 1,
      },
      1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
      "depth peeling - min/max depth odd");
}

void DualDepthPeeling::initColorTextures(VkFormat colorTextureFormat) {
  colorTextures_[0] = context_->createTexture(
      VK_IMAGE_TYPE_2D, colorTextureFormat, 0,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
          VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      {
          .width = context_->swapchain()->extent().width,
          .height = context_->swapchain()->extent().height,
          .depth = 1,
      },
      1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
      "depth peeling - color 0");

  colorTextures_[1] = context_->createTexture(
      VK_IMAGE_TYPE_2D, colorTextureFormat, 0,
      VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
          VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
      {
          .width = context_->swapchain()->extent().width,
          .height = context_->swapchain()->extent().height,
          .depth = 1,
      },
      1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
      "depth peeling - color 1");
}
