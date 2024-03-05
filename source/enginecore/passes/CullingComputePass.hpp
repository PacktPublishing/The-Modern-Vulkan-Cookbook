#pragma once
#include "enginecore/Camera.hpp"
#include "enginecore/Model.hpp"
#include "enginecore/RingBuffer.hpp"
#include "vulkancore/Pipeline.hpp"

class CullingComputePass {
 public:
  struct MeshBBoxBuffer {
    glm::vec4 centerPos;
    glm::vec4 extents;
  };

  struct GPUCullingPassPushConstants {
    uint32_t drawCount;
  };

  struct ViewBuffer {
    alignas(16) glm::vec4 frustumPlanes[6];
  };

  struct IndirectDrawCount {
    uint32_t count;
  };

  CullingComputePass() = default;

  void init(VulkanCore::Context* context, EngineCore::Camera* camera,
            const EngineCore::Model& model,
            std::shared_ptr<VulkanCore::Buffer> inputIndirectBuffer);

  void upload(VulkanCore::CommandQueueManager& queueMgr);

  void cull(VkCommandBuffer cmd, int frameIndex);

  void addBarrierForCulledBuffers(VkCommandBuffer cmd,
                                  VkPipelineStageFlags dstStage,
                                  uint32_t computeFamilyIndex,
                                  uint32_t graphicsFamilyIndex);

  std::shared_ptr<VulkanCore::Buffer> culledIndirectDrawBuffer() {
    return outputIndirectDrawBuffer_;
  }

  std::shared_ptr<VulkanCore::Buffer> culledIndirectDrawCountBuffer() {
    return outputIndirectDrawCountBuffer_;
  }

 private:
  VulkanCore::Context* context_ = nullptr;
  EngineCore::Camera* camera_ = nullptr;
  std::shared_ptr<VulkanCore::Pipeline> pipeline_;
  std::shared_ptr<EngineCore::RingBuffer> camFrustumBuffer_;
  std::shared_ptr<VulkanCore::Buffer> meshBboxBuffer_;
  std::shared_ptr<VulkanCore::Buffer> inputIndirectDrawBuffer_;
  std::shared_ptr<VulkanCore::Buffer> outputIndirectDrawBuffer_;
  std::shared_ptr<VulkanCore::Buffer> outputIndirectDrawCountBuffer_;

  std::vector<MeshBBoxBuffer> meshesBBoxData_;
  ViewBuffer frustum_;
};
