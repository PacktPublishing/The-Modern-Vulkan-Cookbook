#pragma once
#include "enginecore/Camera.hpp"
#include "vulkancore/Context.hpp"
#include "vulkancore/Pipeline.hpp"
#include "vulkancore/Texture.hpp"

class SSAOPass {
 public:
  SSAOPass();
  ~SSAOPass();
  void init(VulkanCore::Context* context,
            std::shared_ptr<VulkanCore::Texture> gBufferDepth);

  void run(VkCommandBuffer cmd);

  std::shared_ptr<VulkanCore::Texture> ssaoTexture() { return outSSAOTexture_; }

 private:
  VulkanCore::Context* context_ = nullptr;
  EngineCore::Camera* camera_ = nullptr;
  std::shared_ptr<VulkanCore::Pipeline> pipeline_;
  std::shared_ptr<VulkanCore::Texture> outSSAOTexture_;

  std::shared_ptr<VulkanCore::Texture> gBufferDepth_;
  std::shared_ptr<VulkanCore::Sampler> sampler_;
};
