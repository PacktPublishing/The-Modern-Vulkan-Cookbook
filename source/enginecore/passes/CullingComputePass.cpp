#include "CullingComputePass.hpp"

#include <filesystem>

#include "vulkancore/Context.hpp"

constexpr uint32_t MESH_BBOX_SET = 0;
constexpr uint32_t INPUT_INDIRECT_BUFFER_SET = 1;
constexpr uint32_t OUTPUT_INDIRECT_BUFFER_SET = 2;
constexpr uint32_t OUTPUT_INDIRECT_COUNT_BUFFER_SET = 3;
constexpr uint32_t CAMERA_FRUSTUM_SET = 4;
constexpr uint32_t BINDING_0 = 0;

void CullingComputePass::init(
    VulkanCore::Context* context, EngineCore::Camera* camera,
    const EngineCore::Model& model,
    std::shared_ptr<VulkanCore::Buffer> inputIndirectBuffer) {
  context_ = context;
  camera_ = camera;
  inputIndirectDrawBuffer_ = inputIndirectBuffer;

  camFrustumBuffer_ = std::make_shared<EngineCore::RingBuffer>(
      context_->swapchain()->numberImages(), *context_, sizeof(ViewBuffer));

  for (auto& mesh : model.meshes) {
    meshesBBoxData_.emplace_back(MeshBBoxBuffer{
        .centerPos = glm::vec4(mesh.center, 1.0),
        .extents = glm::vec4(mesh.extents, 1.0),
    });
  }

  const auto totalSize = sizeof(MeshBBoxBuffer) * meshesBBoxData_.size();

  meshBboxBuffer_ = context_->createBuffer(
      totalSize,
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
          VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, "meshBBoxBuffer");

  outputIndirectDrawBuffer_ = context->createBuffer(
      inputIndirectDrawBuffer_->size(),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
          VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, "Output IndirectDrawBuffer");

  outputIndirectDrawCountBuffer_ = context->createBuffer(
      sizeof(IndirectDrawCount),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
          VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, "Output IndirectDrawBuffer");

  const auto resourcesFolder =
      std::filesystem::current_path() / "resources/shaders/";

  auto shader = context->createShaderModule(
      (resourcesFolder / "gpuculling.comp").string(),
      VK_SHADER_STAGE_COMPUTE_BIT, "compute");

  const std::vector<VulkanCore::Pipeline::SetDescriptor> setLayout = {
      {
          .set_ = MESH_BBOX_SET,
          .bindings_ =
              {
                  VkDescriptorSetLayoutBinding{
                      0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                      VK_SHADER_STAGE_COMPUTE_BIT},
              },
      },
      {
          .set_ = INPUT_INDIRECT_BUFFER_SET,
          .bindings_ =
              {
                  VkDescriptorSetLayoutBinding{
                      0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                      VK_SHADER_STAGE_COMPUTE_BIT},
              },
      },
      {
          .set_ = OUTPUT_INDIRECT_BUFFER_SET,
          .bindings_ =
              {
                  VkDescriptorSetLayoutBinding{
                      0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                      VK_SHADER_STAGE_COMPUTE_BIT},
              },
      },
      {
          .set_ = OUTPUT_INDIRECT_COUNT_BUFFER_SET,
          .bindings_ =
              {
                  VkDescriptorSetLayoutBinding{
                      0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                      VK_SHADER_STAGE_COMPUTE_BIT},
              },
      },
      {
          .set_ = CAMERA_FRUSTUM_SET,
          .bindings_ =
              {
                  VkDescriptorSetLayoutBinding{
                      0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                      VK_SHADER_STAGE_COMPUTE_BIT},
              },
      },
  };
  std::vector<VkPushConstantRange> pushConstants = {
      VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          .offset = 0,
          .size = sizeof(GPUCullingPassPushConstants),
      },
  };
  const VulkanCore::Pipeline::ComputePipelineDescriptor desc = {
      .sets_ = setLayout,
      .computeShader_ = shader,
      .pushConstants_ = pushConstants,
  };
  pipeline_ = context->createComputePipeline(desc, "main");

  pipeline_->allocateDescriptors({
      {.set_ = MESH_BBOX_SET, .count_ = 1},
      {.set_ = INPUT_INDIRECT_BUFFER_SET, .count_ = 1},
      {.set_ = OUTPUT_INDIRECT_BUFFER_SET, .count_ = 1},
      {.set_ = OUTPUT_INDIRECT_COUNT_BUFFER_SET, .count_ = 1},
      {.set_ = CAMERA_FRUSTUM_SET,
       .count_ = context_->swapchain()->numberImages()},
  });

  pipeline_->bindResource(MESH_BBOX_SET, BINDING_0, 0, meshBboxBuffer_, 0,
                          meshBboxBuffer_->size(),
                          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

  pipeline_->bindResource(
      INPUT_INDIRECT_BUFFER_SET, BINDING_0, 0, inputIndirectDrawBuffer_, 0,
      inputIndirectDrawBuffer_->size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

  pipeline_->bindResource(
      OUTPUT_INDIRECT_BUFFER_SET, BINDING_0, 0, outputIndirectDrawBuffer_, 0,
      outputIndirectDrawBuffer_->size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

  pipeline_->bindResource(OUTPUT_INDIRECT_COUNT_BUFFER_SET, BINDING_0, 0,
                          outputIndirectDrawCountBuffer_, 0,
                          outputIndirectDrawCountBuffer_->size(),
                          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

  pipeline_->bindResource(
      CAMERA_FRUSTUM_SET, BINDING_0, 0, camFrustumBuffer_->buffer(0), 0,
      camFrustumBuffer_->buffer(0)->size(), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

  pipeline_->bindResource(
      CAMERA_FRUSTUM_SET, BINDING_0, 1, camFrustumBuffer_->buffer(1), 0,
      camFrustumBuffer_->buffer(1)->size(), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

  pipeline_->bindResource(
      CAMERA_FRUSTUM_SET, BINDING_0, 2, camFrustumBuffer_->buffer(2), 0,
      camFrustumBuffer_->buffer(2)->size(), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

void CullingComputePass::upload(VulkanCore::CommandQueueManager& commandMgr) {
  const auto commandBuffer = commandMgr.getCmdBufferToBegin();
  context_->uploadToGPUBuffer(
      commandMgr, commandBuffer, meshBboxBuffer_.get(),
      reinterpret_cast<const void*>(meshesBBoxData_.data()),
      sizeof(MeshBBoxBuffer) * meshesBBoxData_.size());

  commandMgr.endCmdBuffer(commandBuffer);

  VkPipelineStageFlags flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
  const auto submitInfo = context_->swapchain()->createSubmitInfo(
      &commandBuffer, &flags, false, false);
  commandMgr.submit(&submitInfo);
  commandMgr.waitUntilSubmitIsComplete();
}

void CullingComputePass::cull(VkCommandBuffer cmd, int frameIndex) {
  context_->beginDebugUtilsLabel(cmd, "GPU Culling", {1.0f, 0.0f, 0.0f, 1.0f});

  GPUCullingPassPushConstants pushConst{
      .drawCount = uint32_t(meshesBBoxData_.size()),
  };

  pipeline_->bind(cmd);

  for (int i = 0; auto& plane : camera_->calculateFrustumPlanes()) {
    frustum_.frustumPlanes[i] = plane;
    ++i;
  }

  camFrustumBuffer_->buffer()->copyDataToBuffer(&frustum_, sizeof(ViewBuffer));

  pipeline_->updatePushConstant(cmd, VK_SHADER_STAGE_COMPUTE_BIT,
                                sizeof(GPUCullingPassPushConstants),
                                &pushConst);

  pipeline_->bindDescriptorSets(
      cmd, {
               {.set = MESH_BBOX_SET, .bindIdx = 0},
               {.set = INPUT_INDIRECT_BUFFER_SET, .bindIdx = 0},
               {.set = OUTPUT_INDIRECT_BUFFER_SET, .bindIdx = 0},
               {.set = OUTPUT_INDIRECT_COUNT_BUFFER_SET, .bindIdx = 0},
               {.set = CAMERA_FRUSTUM_SET, .bindIdx = uint32_t(frameIndex)},
           });
  pipeline_->updateDescriptorSets();

  vkCmdDispatch(cmd, (pushConst.drawCount / 256) + 1, 1, 1);

  context_->endDebugUtilsLabel(cmd);
  camFrustumBuffer_->moveToNextBuffer();
}

void CullingComputePass::addBarrierForCulledBuffers(
    VkCommandBuffer cmd, VkPipelineStageFlags dstStage,
    uint32_t computeFamilyIndex, uint32_t graphicsFamilyIndex) {
  std::array<VkBufferMemoryBarrier, 2> barriers{
      VkBufferMemoryBarrier{
          .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
          .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
          .dstAccessMask =
              VK_ACCESS_INDIRECT_COMMAND_READ_BIT,  // could be passed from
                                                    // outside as well
          .srcQueueFamilyIndex = computeFamilyIndex,
          .dstQueueFamilyIndex = graphicsFamilyIndex,
          .buffer = outputIndirectDrawBuffer_->vkBuffer(),
          .size = outputIndirectDrawBuffer_->size(),
      },
      VkBufferMemoryBarrier{
          .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
          .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
          .dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
          .srcQueueFamilyIndex = computeFamilyIndex,
          .dstQueueFamilyIndex = graphicsFamilyIndex,
          .buffer = outputIndirectDrawCountBuffer_->vkBuffer(),
          .size = outputIndirectDrawCountBuffer_->size(),
      },
  };

  vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, dstStage, 0,
                       0, nullptr, (uint32_t)barriers.size(), barriers.data(),
                       0, nullptr);
}
