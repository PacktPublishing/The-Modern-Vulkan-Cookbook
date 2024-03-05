#include "RayTraceShadowPass.hpp"

#include <filesystem>

#include "vulkancore/Utility.hpp"

struct Transforms {
  glm::mat4 viewInverse;
  glm::mat4 projInverse;
  unsigned int frameId;
  int showAOImage;
};

constexpr uint32_t MAIN_SET = 0;
constexpr uint32_t BINDING_TLAS = 0;
constexpr uint32_t BINDING_OUTPUT_IMG = 1;

constexpr uint32_t GBUFFER_SET = 1;
constexpr uint32_t BINDING_GBUFFER_NORMAL = 0;
constexpr uint32_t BINDING_GBUFFER_POSITION = 1;

constexpr uint32_t LIGHT_DATA_SET = 2;
constexpr uint32_t BINDING_LIGHT_DATA = 0;

uint32_t alignedSize(uint32_t value, uint32_t alignment) {
  return (value + alignment - 1) & ~(alignment - 1);
}

RayTraceShadowPass::~RayTraceShadowPass() {
  vkDestroyAccelerationStructureKHR(context_->device(), tLAS_.handle, nullptr);

  for (auto& [id, accelStruct] : bLAS_) {
    vkDestroyAccelerationStructureKHR(context_->device(), accelStruct.handle, nullptr);
  }
}

void RayTraceShadowPass::init(VulkanCore::Context* context,
                              std::shared_ptr<EngineCore::Model> model,
                              std::vector<std::shared_ptr<VulkanCore::Buffer>> buffers,
                              std::shared_ptr<VulkanCore::Texture> gBufferNormal,
                              std::shared_ptr<VulkanCore::Texture> gBufferPosition) {
  context_ = context;

  const auto resourcesFolder = std::filesystem::current_path() / "resources/shaders/";

  auto rayGenShader = context_->createShaderModule(
      (resourcesFolder / "raytrace_raygen_shadow_hybrid.rgen").string(),
      VK_SHADER_STAGE_RAYGEN_BIT_KHR, "RayTracer RayGen Shader");

  auto rayMissShader = context_->createShaderModule(
      (resourcesFolder / "raytrace_miss_shadow_hybrid.rmiss").string(),
      VK_SHADER_STAGE_MISS_BIT_KHR, "RayTracer Miss Shader");

  auto rayClosestHitShader = context_->createShaderModule(
      (resourcesFolder / "raytrace_closesthit_shadow_hybrid.rchit").string(),
      VK_SHADER_STAGE_CLOSEST_HIT_BIT_KHR, "RayTracer Closest hit Shader");

  const std::vector<VulkanCore::Pipeline::SetDescriptor> setLayout = {
      {
          .set_ = MAIN_SET,  // set number
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding{
                      BINDING_TLAS, VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 1,
                      VK_SHADER_STAGE_RAYGEN_BIT_KHR},  // TLAS
                  VkDescriptorSetLayoutBinding{
                      BINDING_OUTPUT_IMG, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                      VK_SHADER_STAGE_RAYGEN_BIT_KHR},  // Output Image
              },
      },
      {
          .set_ = GBUFFER_SET,  // set number
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding{BINDING_GBUFFER_NORMAL,
                                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               1,
                                               VK_SHADER_STAGE_RAYGEN_BIT_KHR},
                  VkDescriptorSetLayoutBinding{BINDING_GBUFFER_POSITION,
                                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               1,
                                               VK_SHADER_STAGE_RAYGEN_BIT_KHR},
              },
      },
      {
          .set_ = LIGHT_DATA_SET,  // set number
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding{BINDING_LIGHT_DATA,
                                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                               VK_SHADER_STAGE_RAYGEN_BIT_KHR},
              },
      },

  };

  const VulkanCore::Pipeline::RayTracingPipelineDescriptor rayTracingDesc = {
      .sets_ = setLayout,
      .rayGenShader_ = rayGenShader,
      .rayMissShaders_ = {rayMissShader},
      .rayClosestHitShaders_ = {rayClosestHitShader},
  };

  pipeline_ = context->createRayTracingPipeline(rayTracingDesc, "RayTracing pipeline");

  pipeline_->allocateDescriptors({
      {.set_ = MAIN_SET, .count_ = 1},
      {.set_ = GBUFFER_SET, .count_ = 1},
      {.set_ = LIGHT_DATA_SET, .count_ = 1},
  });

  createShaderBindingTable();

  initRayTracedStorageImages();

  initBottomLevelAccelStruct(model, buffers);
  initTopLevelAccelStruct(model, buffers);

  lightBuffer_ = context_->createPersistentBuffer(
      sizeof(LightData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      "LightingPass LightData Uniform buffer");

  sampler_ = context_->createSampler(
      VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT,
      VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, 10.0f, true,
      VK_COMPARE_OP_ALWAYS, "default sampler");

  pipeline_->bindResource(MAIN_SET, BINDING_TLAS, 0, &tLAS_.handle);

  pipeline_->bindResource(MAIN_SET, BINDING_OUTPUT_IMG, 0, rayTracedImage_,
                          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

  pipeline_->bindResource(GBUFFER_SET, BINDING_GBUFFER_NORMAL, 0, gBufferNormal,
                          sampler_);

  pipeline_->bindResource(GBUFFER_SET, BINDING_GBUFFER_POSITION, 0, gBufferPosition,
                          sampler_);

  pipeline_->bindResource(LIGHT_DATA_SET, BINDING_LIGHT_DATA, 0, lightBuffer_, 0,
                          sizeof(LightData), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

void RayTraceShadowPass::createShaderBindingTable() {
  const uint32_t handleSize =
      context_->physicalDevice().rayTracingProperties().shaderGroupHandleSize;
  const uint32_t handleSizeAligned = alignedSize(
      context_->physicalDevice().rayTracingProperties().shaderGroupHandleSize,
      context_->physicalDevice().rayTracingProperties().shaderGroupHandleAlignment);

  const uint32_t numRayGenShaders = 1;
  const uint32_t numRayMissShaders = 1;
  const uint32_t numRayClosestHitShaders = 1;

  const uint32_t numShaderGroups =
      numRayGenShaders + numRayMissShaders + numRayClosestHitShaders;
  const uint32_t groupCount = static_cast<uint32_t>(numShaderGroups);
  const uint32_t sbtSize = groupCount * handleSizeAligned;

  std::vector<uint8_t> shaderHandleStorage(sbtSize);
  VK_CHECK(vkGetRayTracingShaderGroupHandlesKHR(context_->device(),
                                                pipeline_->vkPipeline(), 0, groupCount,
                                                sbtSize, shaderHandleStorage.data()));

  raygenSBT_.buffer = context_->createBuffer(
      context_->physicalDevice().rayTracingProperties().shaderGroupHandleSize *
          numRayGenShaders,
      VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VMA_MEMORY_USAGE_CPU_ONLY, "RayGen SBT Buffer");

  raygenSBT_.sbtAddress.deviceAddress = raygenSBT_.buffer->vkDeviceAddress();
  raygenSBT_.sbtAddress.size = handleSizeAligned * numRayGenShaders;
  raygenSBT_.sbtAddress.stride = handleSizeAligned;

  raygenSBT_.buffer->copyDataToBuffer(shaderHandleStorage.data(),
                                      handleSize * numRayGenShaders);

  raymissSBT_.buffer = context_->createBuffer(
      context_->physicalDevice().rayTracingProperties().shaderGroupHandleSize *
          numRayMissShaders,
      VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VMA_MEMORY_USAGE_CPU_ONLY, "Ray Miss SBT Buffer");

  raymissSBT_.sbtAddress.deviceAddress = raymissSBT_.buffer->vkDeviceAddress();
  raymissSBT_.sbtAddress.size = handleSizeAligned * numRayMissShaders;
  raymissSBT_.sbtAddress.stride = handleSizeAligned;

  raymissSBT_.buffer->copyDataToBuffer(
      shaderHandleStorage.data() + handleSizeAligned * numRayGenShaders,
      handleSize * numRayMissShaders);

  rayclosestHitSBT_.buffer = context_->createBuffer(
      context_->physicalDevice().rayTracingProperties().shaderGroupHandleSize *
          numRayClosestHitShaders,
      VK_BUFFER_USAGE_SHADER_BINDING_TABLE_BIT_KHR |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VMA_MEMORY_USAGE_CPU_ONLY, "Ray Closest Hit SBT Buffer");

  rayclosestHitSBT_.sbtAddress.deviceAddress =
      rayclosestHitSBT_.buffer->vkDeviceAddress();
  rayclosestHitSBT_.sbtAddress.size = handleSizeAligned * numRayClosestHitShaders;
  rayclosestHitSBT_.sbtAddress.stride = handleSizeAligned;

  rayclosestHitSBT_.buffer->copyDataToBuffer(shaderHandleStorage.data() +
                                                 handleSizeAligned * numRayGenShaders +
                                                 handleSizeAligned * numRayMissShaders,
                                             handleSize * numRayClosestHitShaders);
}

void RayTraceShadowPass::initRayTracedStorageImages() {
  auto swapchainFormat = VK_FORMAT_B8G8R8A8_UNORM;

  rayTracedImage_ = context_->createTexture(
      VK_IMAGE_TYPE_2D, swapchainFormat, 0,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
          VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      {
          .width = context_->swapchain()->extent().width,
          .height = context_->swapchain()->extent().height,
          .depth = 1,
      },
      1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
      "Ray traced image");
}

void RayTraceShadowPass::initBottomLevelAccelStruct(
    std::shared_ptr<EngineCore::Model> model,
    std::vector<std::shared_ptr<VulkanCore::Buffer>> buffers) {
  // buffers[0] - vertex, buffers[1] - index, buffers[3] - indirect, buffers[2] - material
  for (int meshIdx = 0; meshIdx < model->meshes.size(); ++meshIdx) {
    VkDeviceOrHostAddressConstKHR vertexBufferDeviceAddress{
        .deviceAddress =
            buffers[0]->vkDeviceAddress() +
            model->indirectDrawDataSet[meshIdx].vertexOffset * sizeof(EngineCore::Vertex),
    };
    VkDeviceOrHostAddressConstKHR indexBufferDeviceAddress{
        .deviceAddress =
            buffers[1]->vkDeviceAddress() +
            model->indirectDrawDataSet[meshIdx].firstIndex * sizeof(uint32_t),
    };

    uint32_t numTriangles = model->meshes[meshIdx].indices.size() / 3;

    uint32_t numVertices = model->meshes[meshIdx].vertices.size();

    // Build
    VkAccelerationStructureGeometryKHR accelerationStructureGeometry{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
        .geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR,
        .geometry =
            {
                .triangles =
                    {
                        .sType =
                            VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
                        .vertexFormat = VK_FORMAT_R32G32B32_SFLOAT,
                        .vertexData = vertexBufferDeviceAddress,
                        .vertexStride = sizeof(EngineCore::Vertex),
                        .maxVertex = numVertices,
                        .indexType = VK_INDEX_TYPE_UINT32,
                        .indexData = indexBufferDeviceAddress,
                        .transformData =
                            {
                                .hostAddress = nullptr,
                            },
                    },
            },
    };

    VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .geometryCount = 1,
        .pGeometries = &accelerationStructureGeometry,
    };

    VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
    };
    vkGetAccelerationStructureBuildSizesKHR(
        context_->device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
        &accelerationStructureBuildGeometryInfo, &numTriangles,
        &accelerationStructureBuildSizesInfo);

    bLAS_[meshIdx].buffer = context_->createBuffer(
        accelerationStructureBuildSizesInfo.accelerationStructureSize,
        VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
            VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY, "Bottom Level accel struct buffer");

    // Acceleration structure
    VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
        .buffer = bLAS_[meshIdx].buffer->vkBuffer(),
        .offset = 0,
        .size = accelerationStructureBuildSizesInfo.accelerationStructureSize,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,

    };

    VK_CHECK(vkCreateAccelerationStructureKHR(context_->device(),
                                              &accelerationStructureCreateInfo, nullptr,
                                              &bLAS_[meshIdx].handle));

    auto tempBuffer = context_->createBuffer(
        accelerationStructureBuildSizesInfo.buildScratchSize,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY, "Temporary buffer for bLAS");

    VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{
        .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
        .type = VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
        .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
        .mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
        .dstAccelerationStructure = bLAS_[meshIdx].handle,
        .geometryCount = 1,
        .pGeometries = &accelerationStructureGeometry,
        .scratchData =
            {
                .deviceAddress = tempBuffer->vkDeviceAddress(),
            },
    };

    VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{};
    accelerationStructureBuildRangeInfo.primitiveCount = numTriangles;
    accelerationStructureBuildRangeInfo.primitiveOffset = 0;
    accelerationStructureBuildRangeInfo.firstVertex = 0;
    accelerationStructureBuildRangeInfo.transformOffset = 0;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR*>
        accelerationBuildStructureRangeInfos = {&accelerationStructureBuildRangeInfo};

    auto commandQueueMgr = context_->createGraphicsCommandQueue(
        1, 1, "BLAS acceleration struct build command queue");

    const auto commandBuffer = commandQueueMgr.getCmdBufferToBegin();
    vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &accelerationBuildGeometryInfo,
                                        accelerationBuildStructureRangeInfos.data());
    commandQueueMgr.endCmdBuffer(commandBuffer);

    VkPipelineStageFlags flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
    const auto submitInfo =
        context_->swapchain()->createSubmitInfo(&commandBuffer, &flags, false, false);
    commandQueueMgr.submit(&submitInfo);
    commandQueueMgr.waitUntilSubmitIsComplete();
  }
}

void RayTraceShadowPass::initTopLevelAccelStruct(
    std::shared_ptr<EngineCore::Model> model,
    std::vector<std::shared_ptr<VulkanCore::Buffer>> buffers) {
  glm::mat4 identityMat = glm::identity<glm::mat4>();

  for (int meshIdx = 0; meshIdx < model->meshes.size(); ++meshIdx) {
    VkAccelerationStructureInstanceKHR instance{};

    for (int i = 0; i < 3; i++) {
      for (int j = 0; j < 4; j++) {
        instance.transform.matrix[i][j] = identityMat[j][i];
      }
    }

    instance.instanceCustomIndex =
        meshIdx;  // gl_InstanceCustomIndexEXT, we can also use gl_InstanceID

    instance.mask = 0xFF;

    instance.instanceShaderBindingTableRecordOffset =
        0;  // We will use the same hit group for all objects
    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR;
    instance.accelerationStructureReference = bLAS_[meshIdx].buffer->vkDeviceAddress();

    accelarationInstances_.push_back(instance);
  }

  auto instBuffer = context_->createBuffer(
      sizeof(VkAccelerationStructureInstanceKHR) * accelarationInstances_.size(),
      VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT |
          VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR,
      VMA_MEMORY_USAGE_CPU_ONLY, "Top level accel struct instance buffer");

  instBuffer->copyDataToBuffer(accelarationInstances_.data(), instBuffer->size());

  VkDeviceOrHostAddressConstKHR instanceDataDeviceAddress{
      .deviceAddress = instBuffer->vkDeviceAddress(),
  };

  VkAccelerationStructureGeometryKHR accelerationStructureGeometry{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
      .geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR,
      .geometry =
          {
              .instances =
                  {
                      .sType =
                          VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
                      .arrayOfPointers = VK_FALSE,
                      .data = instanceDataDeviceAddress,
                  },
          },
      .flags = VK_GEOMETRY_OPAQUE_BIT_KHR,
  };

  VkAccelerationStructureBuildGeometryInfoKHR accelerationStructureBuildGeometryInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
      .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
               VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
      .geometryCount = 1,
      .pGeometries = &accelerationStructureGeometry,
  };

  uint32_t primitiveCount = static_cast<uint32_t>(accelarationInstances_.size());

  VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
  };
  vkGetAccelerationStructureBuildSizesKHR(
      context_->device(), VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
      &accelerationStructureBuildGeometryInfo, &primitiveCount,
      &accelerationStructureBuildSizesInfo);

  tLAS_.buffer = context_->createBuffer(
      accelerationStructureBuildSizesInfo.accelerationStructureSize,
      VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_STORAGE_BIT_KHR |
          VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, "Top Level accel struct buffer");

  // Acceleration structure
  VkAccelerationStructureCreateInfoKHR accelerationStructureCreateInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
      .buffer = tLAS_.buffer->vkBuffer(),
      .offset = 0,
      .size = accelerationStructureBuildSizesInfo.accelerationStructureSize,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,

  };

  VK_CHECK(vkCreateAccelerationStructureKHR(
      context_->device(), &accelerationStructureCreateInfo, nullptr, &tLAS_.handle));

  auto tempBuffer = context_->createBuffer(
      accelerationStructureBuildSizesInfo.buildScratchSize,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, "Temporary buffer for bLAS");

  VkAccelerationStructureBuildGeometryInfoKHR accelerationBuildGeometryInfo{
      .sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
      .type = VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
      .flags = VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR |
               VK_BUILD_ACCELERATION_STRUCTURE_ALLOW_UPDATE_BIT_KHR,
      .mode =
          VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,  // use
                                                           // VK_BUILD_ACCELERATION_STRUCTURE_MODE_UPDATE_KHR
                                                           // when updating
      .srcAccelerationStructure = tLAS_.handle,
      .dstAccelerationStructure = tLAS_.handle,
      .geometryCount = 1,
      .pGeometries = &accelerationStructureGeometry,
      .scratchData =
          {
              .deviceAddress = tempBuffer->vkDeviceAddress(),
          },
  };

  VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{
      .primitiveCount = static_cast<uint32_t>(accelarationInstances_.size()),
      .primitiveOffset = 0,
      .firstVertex = 0,
      .transformOffset = 0,
  };
  std::vector<VkAccelerationStructureBuildRangeInfoKHR*>
      accelerationBuildStructureRangeInfos = {&accelerationStructureBuildRangeInfo};

  auto commandQueueMgr = context_->createGraphicsCommandQueue(
      1, 1, "TLAS acceleration struct build command queue");

  const auto commandBuffer = commandQueueMgr.getCmdBufferToBegin();
  vkCmdBuildAccelerationStructuresKHR(commandBuffer, 1, &accelerationBuildGeometryInfo,
                                      accelerationBuildStructureRangeInfos.data());
  commandQueueMgr.endCmdBuffer(commandBuffer);

  VkPipelineStageFlags flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
  const auto submitInfo =
      context_->swapchain()->createSubmitInfo(&commandBuffer, &flags, false, false);
  commandQueueMgr.submit(&submitInfo);
  commandQueueMgr.waitUntilSubmitIsComplete();
}

void RayTraceShadowPass::execute(VkCommandBuffer commandBuffer, uint32_t swapchainIndex,
                                 const LightData& data) {
  lightBuffer_->copyDataToBuffer(&data, sizeof(LightData));

  pipeline_->bind(commandBuffer);
  pipeline_->bindDescriptorSets(commandBuffer,
                                {
                                    {.set = MAIN_SET, .bindIdx = (uint32_t)0},
                                    {.set = GBUFFER_SET, .bindIdx = (uint32_t)0},
                                    {.set = LIGHT_DATA_SET, .bindIdx = (uint32_t)0},
                                });
  pipeline_->updateDescriptorSets();

  VkStridedDeviceAddressRegionKHR emptySbtEntry = {};
  vkCmdTraceRaysKHR(commandBuffer, &raygenSBT_.sbtAddress, &raymissSBT_.sbtAddress,
                    &rayclosestHitSBT_.sbtAddress, &emptySbtEntry,
                    static_cast<uint32_t>(rayTracedImage_->vkExtents().width),
                    static_cast<uint32_t>(rayTracedImage_->vkExtents().height), 1);
}
