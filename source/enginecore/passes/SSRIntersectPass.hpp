#pragma once
#include "enginecore/Camera.hpp"
#include "vulkancore/Context.hpp"
#include "vulkancore/Pipeline.hpp"
#include "vulkancore/Texture.hpp"

class SSRIntersectPass {
 public:
  SSRIntersectPass();
  ~SSRIntersectPass();
  void init(VulkanCore::Context* context, EngineCore::Camera* camera,
            std::shared_ptr<VulkanCore::Texture> gBufferNormal,
            std::shared_ptr<VulkanCore::Texture> gBufferSpecular,
            std::shared_ptr<VulkanCore::Texture> gBufferBaseColor,
            std::shared_ptr<VulkanCore::Texture> hierarchicalDepth,
            std::shared_ptr<VulkanCore::Texture> noiseTexture);

  void run(VkCommandBuffer cmd);

  std::shared_ptr<VulkanCore::Texture> insersectTexture() {
    return outSSRIntersectTexture_;
  }

 private:
  VulkanCore::Context* context_ = nullptr;
  EngineCore::Camera* camera_ = nullptr;
  std::shared_ptr<VulkanCore::Pipeline> pipeline_;
  std::shared_ptr<VulkanCore::Texture> outSSRIntersectTexture_;

  std::shared_ptr<VulkanCore::Texture> gBufferNormal_;
  std::shared_ptr<VulkanCore::Texture> gBufferSpecular_;
  std::shared_ptr<VulkanCore::Texture> gBufferBaseColor_;
  std::shared_ptr<VulkanCore::Texture> hierarchicalDepth_;
  std::shared_ptr<VulkanCore::Texture> noiseTexture_;
  std::shared_ptr<VulkanCore::Sampler> sampler_;
  std::shared_ptr<VulkanCore::Buffer> cameraBuffer_;

  uint32_t index_ = 0;
};
