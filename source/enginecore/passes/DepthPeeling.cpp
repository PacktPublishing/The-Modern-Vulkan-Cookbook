#include "DepthPeeling.hpp"

#include <filesystem>

#include "vulkancore/Context.hpp"
#include "vulkancore/DynamicRendering.hpp"
#include "vulkancore/RenderPass.hpp"

constexpr uint32_t CAMERA_SET = 0;
constexpr uint32_t OBJECT_PROP_SET = 1;
constexpr uint32_t DEPTH_ATTACHMENTS_SET = 2;
constexpr uint32_t BINDING_0 = 0;
constexpr uint32_t BINDING_1 = 1;

constexpr uint32_t BINDING_PEEL_DEPTH = 0;
constexpr uint32_t BINDING_OPAQUE_DEPTH = 1;
constexpr uint32_t BINDING_TEMPCOLOR_DEPTH = 2;

void DepthPeeling::draw(VkCommandBuffer commandBuffer, int index,
                        const std::vector<std::shared_ptr<VulkanCore::Buffer>>& buffers,
                        uint32_t numMeshes) {
  {
    // Clear Depth 1
    const VkClearDepthStencilValue clearDepth = {
        .depth = 0.0f
    };
    const VkImageSubresourceRange range = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1};

    depthTextures_[1]->transitionImageLayout(commandBuffer,
                                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkCmdClearDepthStencilImage(commandBuffer, depthTextures_[1]->vkImage(),
                                VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearDepth, 1,
                                &range);
  }
  {
    // Clear color attachments
    const VkClearColorValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
    const VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};

    colorTextures_[0]->transitionImageLayout(commandBuffer,
                                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    colorTextures_[1]->transitionImageLayout(commandBuffer,
                                             VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkCmdClearColorImage(commandBuffer, colorTextures_[0]->vkImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
    vkCmdClearColorImage(commandBuffer, colorTextures_[1]->vkImage(),
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);
  }

  const std::array<VkClearValue, 2> clearValues = {
      VkClearValue{
          .color = {0.0f, 0.0f, 0.0f, 0.0f},
      },
      VkClearValue{
          .depthStencil = {1.0f},
      },
  };

  const VkImageBlit region{
      .srcSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                         .mipLevel = 0,
                         .baseArrayLayer = 0,
                         .layerCount = 1},
      .srcOffsets = {{0, 0, 0},
                     {static_cast<int32_t>(colorTextures_[0]->vkExtents().width),
                      static_cast<int32_t>(colorTextures_[0]->vkExtents().height), 1}},
      .dstSubresource = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                         .mipLevel = 0,
                         .baseArrayLayer = 0,
                         .layerCount = 1},
      .dstOffsets = {{0, 0, 0},
                     {static_cast<int32_t>(colorTextures_[0]->vkExtents().width),
                      static_cast<int32_t>(colorTextures_[0]->vkExtents().height), 1}},
  };

  depthTextures_[1]->transitionImageLayout(commandBuffer,
                                           VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  VulkanCore::DynamicRendering::AttachmentDescription colorAttachmentDesc{
      .imageView = colorTextures_[0]->vkImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .attachmentLoadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
      .attachmentStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearValues[0],
  };

  VulkanCore::DynamicRendering::AttachmentDescription depthAttachmentDesc{
      .imageView = depthTextures_[0]->vkImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .attachmentLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .attachmentStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearValues[1],
  };

  for (uint32_t currentPeel = 0; currentPeel < numPeels_; ++currentPeel) {
    // std::cerr << "Peel #" << currentPeel << std::endl;

    colorTextures_[currentPeel % 2]->transitionImageLayout(
        commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    colorTextures_[(currentPeel + 1) % 2]->transitionImageLayout(
        commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    depthTextures_[currentPeel % 2]->transitionImageLayout(
        commandBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    depthTextures_[(currentPeel + 1) % 2]->transitionImageLayout(
        commandBuffer, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    colorAttachmentDesc.imageView = colorTextures_[currentPeel % 2]->vkImageView();
    depthAttachmentDesc.imageView = depthTextures_[currentPeel % 2]->vkImageView();

    context_->beginDebugUtilsLabel(commandBuffer,
                                   "Depth Peeling: peel " + std::to_string(currentPeel),
                                   {1.0f, .55f, 0.0f, 1.0f});

    VulkanCore::DynamicRendering::beginRenderingCmd(
        commandBuffer, colorTextures_[currentPeel % 2]->vkImage(), 0,
        {{0, 0},
         {colorTextures_[currentPeel % 2]->vkExtents().width,
          colorTextures_[currentPeel % 2]->vkExtents().height}},
        1, 0, {colorAttachmentDesc}, &depthAttachmentDesc, nullptr,
        colorTextures_[currentPeel % 2]->vkLayout(),
        VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport_);
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor_);

    pipeline_->bind(commandBuffer);

    // std::cerr << "Render" << std::endl;
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
        commandBuffer, colorTextures_[currentPeel % 2]->vkImage(),
        VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED);

    context_->endDebugUtilsLabel(commandBuffer);

    context_->beginDebugUtilsLabel(commandBuffer, "Auto layout transition",
                                   {.9f, .55f, .7f, 1.f});

    colorTextures_[currentPeel % 2]->transitionImageLayout(
        commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    colorTextures_[(currentPeel + 1) % 2]->transitionImageLayout(
        commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    context_->endDebugUtilsLabel(commandBuffer);

    vkCmdBlitImage(commandBuffer, colorTextures_[currentPeel % 2]->vkImage(),
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   colorTextures_[(currentPeel + 1) % 2]->vkImage(),
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region, VK_FILTER_NEAREST);
  }
}

void DepthPeeling::init(
    uint32_t numMeshes, const VkVertexInputBindingDescription& vertexBindingDesc,
    const std::vector<VkVertexInputAttributeDescription>& vertexDescription) {
  ASSERT(!colorTextures_.empty(),
         "The number of color attachments for the depth peeling pass cannot be zero")
  ASSERT(depthTextures_[0], "Depth texture 0 cannot be nullptr")

  sampler_ = std::make_shared<VulkanCore::Sampler>(
      *context_, VK_FILTER_NEAREST, VK_FILTER_NEAREST,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER, 1, "depth peeling");

  const auto resourcesFolder = std::filesystem::current_path() / "resources/shaders/";

  auto vertexShader =
      context_->createShaderModule((resourcesFolder / "depthPeel.vert").string(),
                                   VK_SHADER_STAGE_VERTEX_BIT, "Depth Peeling vertex");
  auto fragmentShader = context_->createShaderModule(
      (resourcesFolder / "depthPeel.frag").string(), VK_SHADER_STAGE_FRAGMENT_BIT,
      "Depth Peeling fragment");

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
                      BINDING_OPAQUE_DEPTH, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                      VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
                  VkDescriptorSetLayoutBinding{
                      BINDING_TEMPCOLOR_DEPTH, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
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
      .colorTextureFormats = {colorTextures_[0]->vkFormat()},
      .depthTextureFormat = depthTextures_[0]->vkFormat(),
      .sampleCount = VK_SAMPLE_COUNT_1_BIT,
      .cullMode = VK_CULL_MODE_NONE,
      .viewport = context_->swapchain()->extent(),
      //.blendEnable = true,
      .depthTestEnable = true,
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
      //.blendAttachmentStates_ =
      //    {
      //        {
      //            .blendEnable = true,
      //            .srcColorBlendFactor = VK_BLEND_FACTOR_DST_ALPHA,  // Optional
      //            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE,
      //            .colorBlendOp = VK_BLEND_OP_ADD,
      //            .srcAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
      //            .dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
      //            .alphaBlendOp = VK_BLEND_OP_ADD,  // Optional
      //            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
      //                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
      //        },
      //    },
  };

  pipeline_ = context_->createGraphicsPipeline(gpDesc, VK_NULL_HANDLE, "depth peeling");

  pipeline_->allocateDescriptors({
      {.set_ = CAMERA_SET, .count_ = 3},
      {.set_ = OBJECT_PROP_SET, .count_ = numMeshes},
      {.set_ = DEPTH_ATTACHMENTS_SET, .count_ = 2},
  });
}

void DepthPeeling::init(VulkanCore::Context* context,
                        const EngineCore::RingBuffer& cameraBuffer,
                        EngineCore::RingBuffer& objectPropBuffer, size_t objectPropSize,
                        uint32_t numMeshes, uint32_t numPeels,
                        VkFormat colorTextureFormat, VkFormat depthTextureFormat,
                        std::shared_ptr<VulkanCore::Texture> opaquePassDepth) {
  numPeels_ = numPeels;
  context_ = context;

  initColorTextures(numPeels, colorTextureFormat);
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
    auto [format, offset] = vertexAttributesFormatAndOffset[i];
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

  pipeline_->bindResource(DEPTH_ATTACHMENTS_SET, BINDING_PEEL_DEPTH, 0, depthTextures_[1],
                          sampler_, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  pipeline_->bindResource(DEPTH_ATTACHMENTS_SET, BINDING_OPAQUE_DEPTH, 0, opaquePassDepth,
                          sampler_, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  pipeline_->bindResource(DEPTH_ATTACHMENTS_SET, BINDING_TEMPCOLOR_DEPTH, 0,
                          colorTextures_[1], sampler_,
                          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

  pipeline_->bindResource(DEPTH_ATTACHMENTS_SET, BINDING_PEEL_DEPTH, 1, depthTextures_[0],
                          sampler_, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  pipeline_->bindResource(DEPTH_ATTACHMENTS_SET, BINDING_OPAQUE_DEPTH, 1, opaquePassDepth,
                          sampler_, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);
  pipeline_->bindResource(DEPTH_ATTACHMENTS_SET, BINDING_TEMPCOLOR_DEPTH, 1,
                          colorTextures_[0], sampler_,
                          VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

  for (uint32_t meshIdx = 0; meshIdx < numMeshes; ++meshIdx) {
    pipeline_->bindResource(OBJECT_PROP_SET, BINDING_0, meshIdx,
                            objectPropBuffer.buffer(meshIdx), 0, objectPropSize,
                            VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  }
}

void DepthPeeling::initDepthTextures(VkFormat depthFormat) {
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

  for (uint32_t i = 0; i < depthTextures_.size(); ++i) {
    depthTextures_[i] = context_->createTexture(
        VK_IMAGE_TYPE_2D, depthFormat, 0,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        {
            .width = context_->swapchain()->extent().width,
            .height = context_->swapchain()->extent().height,
            .depth = 1,
        },
        1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
        "depth peeling - depth " + std::to_string(i));
  }
}

void DepthPeeling::initColorTextures(uint32_t numPeels, VkFormat colorTextureFormat) {
  colorTextures_.resize(2);

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
