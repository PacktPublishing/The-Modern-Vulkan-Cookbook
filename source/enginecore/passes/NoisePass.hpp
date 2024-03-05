#pragma once
#include "vulkancore/Context.hpp"
#include "vulkancore/Pipeline.hpp"
#include "vulkancore/Texture.hpp"

class NoisePass {
 public:
  NoisePass();
  void init(VulkanCore::Context* context);
  void upload(VulkanCore::CommandQueueManager& queueMgr);

  void generateNoise(VkCommandBuffer cmd);

  std::shared_ptr<VulkanCore::Texture> noiseTexture() { return outNoiseTexture_; }

 private:
  VulkanCore::Context* context_ = nullptr;
  std::shared_ptr<VulkanCore::Pipeline> pipeline_;
  std::shared_ptr<VulkanCore::Texture> outNoiseTexture_;
  std::shared_ptr<VulkanCore::Buffer> sobolBuffer_;
  std::shared_ptr<VulkanCore::Buffer> rankingTile_;
  std::shared_ptr<VulkanCore::Buffer> scramblingTileBuffer_;

  uint32_t index_ = 0;
};
