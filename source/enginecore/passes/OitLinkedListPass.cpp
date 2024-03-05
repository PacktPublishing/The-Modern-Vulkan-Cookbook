#include "OitLinkedListPass.hpp"

#include <filesystem>

#include "enginecore/Camera.hpp"
#include "enginecore/Model.hpp"
#include "vulkancore/DynamicRendering.hpp"

constexpr uint32_t CAMERA_SET = 0;
constexpr uint32_t OBJECT_PROP_SET = 1;
constexpr uint32_t LINKED_LIST_DATA_SET = 2;
constexpr uint32_t BINDING_CameraMVP = 0;
constexpr uint32_t BINDING_ObjectProperties = 0;
constexpr uint32_t BINDING_AtomicCounter = 0;
constexpr uint32_t BINDING_LLBuffer = 1;
constexpr uint32_t BINDING_LLHeadPtr = 2;

const int slotsPerPixel = 10;

struct Node {
  glm::vec4 color;
  uint32_t previousIndex;
  float depth;
  uint32_t padding1;  // add 4 byte padding for alignment
  uint32_t padding2;  // add 4 byte padding for alignment
};

OitLinkedListPass::OitLinkedListPass() {}

void OitLinkedListPass::init(VulkanCore::Context* context,
                             const EngineCore::RingBuffer& cameraBuffer,
                             EngineCore::RingBuffer& objectPropBuffer,
                             size_t objectPropSize, uint32_t numMeshes,
                             VkFormat colorTextureFormat, VkFormat depthTextureFormat,
                             std::shared_ptr<VulkanCore::Texture> opaquePassDepth) {
  context_ = context;
  colorTexture_ =
      context->createTexture(VK_IMAGE_TYPE_2D, colorTextureFormat, 0,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
                                 VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                             {
                                 .width = context->swapchain()->extent().width,
                                 .height = context->swapchain()->extent().height,
                                 .depth = 1,
                             },
      1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
                             "OIT LL Color Pass - Color attachment");

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
      "OIT LL Color Pass - Depth attachment");

  atomicCounterBuffer_ = context->createBuffer(
      sizeof(AtomicCounter),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, "OIT LL Color Pass - Atomic Counter");

  auto bufferSize = context->swapchain()->extent().width *
                    context->swapchain()->extent().height * slotsPerPixel * sizeof(Node);

  linkedListBuffer_ = context_->createBuffer(
      bufferSize, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, "OIT LL Color Pass - linkedlist buffer");

  linkedListHeadPtrTexture_ =
      context->createTexture(VK_IMAGE_TYPE_2D, VK_FORMAT_R32_UINT, 0,
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                             {
                                 .width = context->swapchain()->extent().width,
                                 .height = context->swapchain()->extent().height,
                                 .depth = 1,
                             },
      1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
                             "OIT LL Color Pass - linked list head pointer");

  sampler_ = context_->createSampler(
      VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT,
      VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, 100.0f,
      "OIT LL Color Pass - sampler");

  const auto resourcesFolder = std::filesystem::current_path() / "resources/shaders/";

  auto vertexShader =
      context_->createShaderModule((resourcesFolder / "bindfull.vert").string(),
                                   VK_SHADER_STAGE_VERTEX_BIT, "OIT LL - vertex shader");
  auto fragmentShader = context_->createShaderModule(
      (resourcesFolder / "OitLinkedListBuildPass.frag").string(),
      VK_SHADER_STAGE_FRAGMENT_BIT, "OIT LL - fragment shader");

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
      {
          .set_ = LINKED_LIST_DATA_SET,  // set
                                         // number
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding{BINDING_AtomicCounter,
                                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                               VK_SHADER_STAGE_FRAGMENT_BIT},
                  VkDescriptorSetLayoutBinding{BINDING_LLHeadPtr,
                                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                                               VK_SHADER_STAGE_FRAGMENT_BIT},
                  VkDescriptorSetLayoutBinding{BINDING_LLBuffer,
                                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                               VK_SHADER_STAGE_FRAGMENT_BIT},
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
      .colorTextureFormats = {colorTexture_->vkFormat()},
      .depthTextureFormat = depthTexture_->vkFormat(),
      .sampleCount = VK_SAMPLE_COUNT_1_BIT,
      .cullMode = VK_CULL_MODE_NONE,
      .viewport = context->swapchain()->extent(),
      .blendEnable = true,
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
  };

  pipeline_ = context->createGraphicsPipeline(gpDesc, VK_NULL_HANDLE,
                                              "OIT LL ColorPass Pipeline");

  pipeline_->allocateDescriptors({
      {.set_ = CAMERA_SET, .count_ = 3},
      {.set_ = OBJECT_PROP_SET, .count_ = numMeshes},
      {.set_ = LINKED_LIST_DATA_SET, .count_ = 1},
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

  pipeline_->bindResource(LINKED_LIST_DATA_SET, BINDING_AtomicCounter, 0,
                          atomicCounterBuffer_, 0, atomicCounterBuffer_->size(),
                          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

  pipeline_->bindResource(LINKED_LIST_DATA_SET, BINDING_LLHeadPtr, 0,
                          linkedListHeadPtrTexture_, sampler_,
                          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

  pipeline_->bindResource(LINKED_LIST_DATA_SET, BINDING_LLBuffer, 0, linkedListBuffer_, 0,
                          linkedListBuffer_->size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

  initCompositePipeline();
}

void OitLinkedListPass::draw(
    VkCommandBuffer commandBuffer, int index,
    const std::vector<std::shared_ptr<VulkanCore::Buffer>>& buffers, uint32_t numMeshes) {
  linkedListHeadPtrTexture_->transitionImageLayout(commandBuffer,
                                                   VK_IMAGE_LAYOUT_GENERAL);

  context_->beginDebugUtilsLabel(commandBuffer, "OIT LL ColorPass - Clear Buffers",
                                 {0.0f, 1.0f, 0.0f, 1.0f});

  VkClearColorValue clearColor;
  clearColor.uint32[0] = 0;

  const VkImageSubresourceRange auxClearRanges = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
  };

  vkCmdClearColorImage(commandBuffer, linkedListHeadPtrTexture_->vkImage(),
                       VK_IMAGE_LAYOUT_GENERAL, &clearColor, 1, &auxClearRanges);

  // linkedListBuffer_ is huge, clearing it is good for easier debugging but unnecessary &
  // causes stalls
  // vkCmdFillBuffer(commandBuffer, linkedListBuffer_->vkBuffer(), 0, VK_WHOLE_SIZE, 0);
  vkCmdFillBuffer(commandBuffer, atomicCounterBuffer_->vkBuffer(), 0, VK_WHOLE_SIZE, 0);

  const VkPipelineStageFlags srcStageFlags = VK_PIPELINE_STAGE_TRANSFER_BIT;
  const VkPipelineStageFlags dstStageFlags = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  {
    const VkMemoryBarrier barrier = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
    };

    const VkBufferMemoryBarrier bufferBarriers[1] = {
        /*{
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            .buffer = linkedListBuffer_->vkBuffer(),
            .size = linkedListBuffer_->size(),
        },*/
        {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
            .buffer = atomicCounterBuffer_->vkBuffer(),
            .size = atomicCounterBuffer_->size(),
        },
    };

    vkCmdPipelineBarrier(commandBuffer, srcStageFlags, dstStageFlags,
                         0,                  //
                         1, &barrier,        //
                         0, VK_NULL_HANDLE,  //
                         0, VK_NULL_HANDLE);

    vkCmdPipelineBarrier(commandBuffer, srcStageFlags, dstStageFlags, 0, 0, nullptr, 1,
                         &bufferBarriers[0], 0, nullptr);
  }

  context_->endDebugUtilsLabel(commandBuffer);

  context_->beginDebugUtilsLabel(commandBuffer, "OIT LL Build ColorPass",
                                 {0.0f, 1.0f, 0.0f, 1.0f});

  const std::array<VkClearValue, 2> clearValues = {
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
          .depthStencil =
              {
                  1.0f,
                  0,
              },
      },
  };
  const VulkanCore::DynamicRendering::AttachmentDescription colorAttachmentDesc{
      .imageView = colorTexture_->vkImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
      .resolveModeFlagBits = VK_RESOLVE_MODE_NONE,
      .resolveImageView = VK_NULL_HANDLE,
      .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .attachmentLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .attachmentStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
      .clearValue = clearValues[0]};

  const VulkanCore::DynamicRendering::AttachmentDescription depthAttachmentDesc{
      .imageView = depthTexture_->vkImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
      .resolveModeFlagBits = VK_RESOLVE_MODE_NONE,
      .resolveImageView = VK_NULL_HANDLE,
      .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .attachmentLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
      .attachmentStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
      .clearValue = clearValues[1],
  };

  VulkanCore::DynamicRendering::beginRenderingCmd(
      commandBuffer, colorTexture_->vkImage(), 0,
      {{0, 0}, {colorTexture_->vkExtents().width, colorTexture_->vkExtents().height}}, 1,
      0, {colorAttachmentDesc}, &depthAttachmentDesc, nullptr);

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
                                      {.set = LINKED_LIST_DATA_SET, .bindIdx = 0},
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

  context_->beginDebugUtilsLabel(commandBuffer, "OIT LL Barriers before CompositePass",
                                 {0.0f, 1.0f, 1.0f, 1.0f});

  const VkMemoryBarrier barrier = {
      .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
  };

  const VkBufferMemoryBarrier bufferBarriers[1] = {
      {
          .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
          .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
          .dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT,
          .buffer = linkedListBuffer_->vkBuffer(),
          .size = linkedListBuffer_->size(),
      },
  };

  // required for head ptr image
  vkCmdPipelineBarrier(commandBuffer, srcStageFlags, dstStageFlags,
                       0,                  //
                       1, &barrier,        //
                       0, VK_NULL_HANDLE,  //
                       0, VK_NULL_HANDLE);

  vkCmdPipelineBarrier(commandBuffer, srcStageFlags, dstStageFlags, 0, 0, nullptr, 1,
                       &bufferBarriers[0], 0, nullptr);

  context_->endDebugUtilsLabel(commandBuffer);

  context_->beginDebugUtilsLabel(commandBuffer, "OIT LL CompositePass",
                                 {0.0f, 1.0f, 1.0f, 1.0f});

  VulkanCore::DynamicRendering::beginRenderingCmd(
      commandBuffer, colorTexture_->vkImage(), 0,
      {{0, 0}, {colorTexture_->vkExtents().width, colorTexture_->vkExtents().height}}, 1,
      0, {colorAttachmentDesc}, &depthAttachmentDesc, nullptr);

  compositePipeline_->bind(commandBuffer);

  compositePipeline_->bindDescriptorSets(commandBuffer,
                                         {
                                             {.set = 0, .bindIdx = (uint32_t)0},
                                         });
  compositePipeline_->updateDescriptorSets();

  vkCmdDraw(commandBuffer, 4, 1, 0, 0);

  VulkanCore::DynamicRendering::endRenderingCmd(commandBuffer, colorTexture_->vkImage(),
                                                VK_IMAGE_LAYOUT_UNDEFINED,
                                                VK_IMAGE_LAYOUT_UNDEFINED);

  context_->endDebugUtilsLabel(commandBuffer);
}

void OitLinkedListPass::initCompositePipeline() {
  const auto resourcesFolder = std::filesystem::current_path() / "resources/shaders/";

  auto vertexShader =
      context_->createShaderModule((resourcesFolder / "fullscreen.vert").string(),
                                   VK_SHADER_STAGE_VERTEX_BIT, "main vertex");
  auto fragmentShader = context_->createShaderModule(
      (resourcesFolder / "OITLinkedListCompositePass.frag").string(),
      VK_SHADER_STAGE_FRAGMENT_BIT, "main fragment");

  const std::vector<VulkanCore::Pipeline::SetDescriptor> setLayout = {
      {
          .set_ = 0,
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding{0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                                               VK_SHADER_STAGE_FRAGMENT_BIT},
                  VkDescriptorSetLayoutBinding{1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                               VK_SHADER_STAGE_FRAGMENT_BIT},
              },

      },
  };

  const VulkanCore::Pipeline::GraphicsPipelineDescriptor gpDesc = {
      .sets_ = setLayout,
      .vertexShader_ = vertexShader,
      .fragmentShader_ = fragmentShader,
      .dynamicStates_ = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR},
      .useDynamicRendering_ = true,
      .colorTextureFormats = {colorTexture_->vkFormat()},
      .depthTextureFormat = depthTexture_->vkFormat(),
      .primitiveTopology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
      .sampleCount = VK_SAMPLE_COUNT_1_BIT,
      .cullMode = VK_CULL_MODE_NONE,
      .viewport = context_->swapchain()->extent(),
      .depthTestEnable = false,
      .depthWriteEnable = false,
  };

  compositePipeline_ =
      context_->createGraphicsPipeline(gpDesc, VK_NULL_HANDLE, "OIT Composite pipeline");

  compositePipeline_->allocateDescriptors({
      {.set_ = 0, .count_ = 1},
  });

  compositePipeline_->bindResource(0, 0, 0, linkedListHeadPtrTexture_, sampler_,
                                   VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

  compositePipeline_->bindResource(0, 1, 0, linkedListBuffer_, 0,
                                   linkedListBuffer_->size(),
                                   VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
}
