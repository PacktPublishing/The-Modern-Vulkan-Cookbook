#pragma once
#include "enginecore/RingBuffer.hpp"
#include "vulkancore/Context.hpp"
#include "vulkancore/Pipeline.hpp"
#include "vulkancore/Texture.hpp"
// assumes we are using dynamic rendering

struct AtomicCounter {
  uint32_t counter;
};

class OitLinkedListPass {
 public:
  OitLinkedListPass();
  void init(VulkanCore::Context* context, const EngineCore::RingBuffer& cameraBuffer,
            EngineCore::RingBuffer& objectPropBuffer, size_t objectPropSize,
            uint32_t numMeshes, VkFormat colorTextureFormat, VkFormat depthTextureFormat,
            std::shared_ptr<VulkanCore::Texture> opaquePassDepth);

  void draw(VkCommandBuffer cmd, int index,
            const std::vector<std::shared_ptr<VulkanCore::Buffer>>& buffers,
            uint32_t numMeshes);

  std::shared_ptr<VulkanCore::Pipeline> pipeline() const { return pipeline_; }

  std::shared_ptr<VulkanCore::Texture> colorTexture() const { return colorTexture_; }

 private:
  void initCompositePipeline();

 private:
  VulkanCore::Context* context_ = nullptr;
  std::shared_ptr<VulkanCore::Texture> colorTexture_;
  std::shared_ptr<VulkanCore::Texture> depthTexture_;
  std::shared_ptr<VulkanCore::Buffer> atomicCounterBuffer_;
  std::shared_ptr<VulkanCore::Buffer> linkedListBuffer_;
  std::shared_ptr<VulkanCore::Texture> linkedListHeadPtrTexture_;  // per pixel headptr

  std::shared_ptr<VulkanCore::Sampler> sampler_;

  std::shared_ptr<VulkanCore::Pipeline> pipeline_;

  std::shared_ptr<VulkanCore::Pipeline> compositePipeline_;
};
