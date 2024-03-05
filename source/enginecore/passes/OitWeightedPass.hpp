#pragma once
#include "enginecore/RingBuffer.hpp"
#include "vulkancore/Context.hpp"
#include "vulkancore/Pipeline.hpp"
#include "vulkancore/Texture.hpp"
// assumes we are using dynamic rendering

class OitWeightedPass {
 public:
  OitWeightedPass();
  void init(VulkanCore::Context* context, const EngineCore::RingBuffer& cameraBuffer,
            EngineCore::RingBuffer& objectPropBuffer, size_t objectPropSize,
            uint32_t numMeshes, VkFormat colorTextureFormat, VkFormat depthTextureFormat,
            std::shared_ptr<VulkanCore::Texture> opaquePassDepth);

  void draw(VkCommandBuffer cmd, int index,
            const std::vector<std::shared_ptr<VulkanCore::Buffer>>& buffers,
            uint32_t numMeshes);

  std::shared_ptr<VulkanCore::Pipeline> pipeline() const { return pipeline_; }

  std::shared_ptr<VulkanCore::Texture> colorTexture() const {
    return compositeColorTexture_;
  }

 private:
  void initCompositePipeline(VkFormat colorTextureFormat);

 private:
  VulkanCore::Context* context_ = nullptr;
  std::shared_ptr<VulkanCore::Texture> colorTexture_;  // VK_FORMAT_R16G16B16A16_SFLOAT
  std::shared_ptr<VulkanCore::Texture> alphaTexture_;  // VK_FORMAT_R16_SFLOAT
  std::shared_ptr<VulkanCore::Texture> depthTexture_;

  std::shared_ptr<VulkanCore::Sampler> sampler_;

  std::shared_ptr<VulkanCore::Pipeline> pipeline_;

  std::shared_ptr<VulkanCore::Texture> compositeColorTexture_;
  std::shared_ptr<VulkanCore::Pipeline> compositePipeline_;
};
