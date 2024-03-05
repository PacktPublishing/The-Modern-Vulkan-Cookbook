#pragma once
#include "vulkancore/Context.hpp"
#include "vulkancore/Pipeline.hpp"
#include "vulkancore/Texture.hpp"

class HierarchicalDepthBufferPass {
 public:
  HierarchicalDepthBufferPass();
  ~HierarchicalDepthBufferPass();
  void init(VulkanCore::Context* context,
            std::shared_ptr<VulkanCore::Texture> depthTexture);

  void generateHierarchicalDepthBuffer(VkCommandBuffer cmd);

  std::shared_ptr<VulkanCore::Texture> hierarchicalDepthTexture() {
    return outHierarchicalDepthTexture_;
  }

 private:
  VulkanCore::Context* context_ = nullptr;
  std::shared_ptr<VulkanCore::Pipeline> pipeline_;
  std::shared_ptr<VulkanCore::Texture> outHierarchicalDepthTexture_;
  std::vector<std::shared_ptr<VkImageView>> hierarchicalDepthTexturePerMipImageViews_;
  std::shared_ptr<VulkanCore::Texture> depthTexture_;
  std::shared_ptr<VulkanCore::Sampler> sampler_;
};
