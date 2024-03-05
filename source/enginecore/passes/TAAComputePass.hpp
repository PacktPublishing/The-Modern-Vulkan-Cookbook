#pragma once
#include "enginecore/Camera.hpp"
#include "vulkancore/Pipeline.hpp"

class TAAComputePass {
 public:
  struct TAAPushConstants {
    uint32_t isFirstFrame;
    uint32_t isCameraMoving;
  };

  TAAComputePass() = default;

  void init(VulkanCore::Context* context,
            std::shared_ptr<VulkanCore::Texture> depthTexture,
            std::shared_ptr<VulkanCore::Texture> velocityTexture,
            std::shared_ptr<VulkanCore::Texture> colorTexture);

  void doAA(VkCommandBuffer cmd, int frameIndex, int isCamMoving);

  std::shared_ptr<VulkanCore::Texture> colorTexture() const { return outColorTexture_; }

 private:
  void initSharpenPipeline();

  VulkanCore::Context* context_ = nullptr;
  std::shared_ptr<VulkanCore::Sampler> sampler_;
  std::shared_ptr<VulkanCore::Sampler> pointSampler_;

  std::shared_ptr<VulkanCore::Texture> depthTexture_;
  std::shared_ptr<VulkanCore::Texture> historyTexture_;
  std::shared_ptr<VulkanCore::Texture> velocityTexture_;
  std::shared_ptr<VulkanCore::Texture> colorTexture_;

  std::shared_ptr<VulkanCore::Texture> outColorTexture_;

  std::shared_ptr<VulkanCore::Pipeline> pipeline_;

  std::shared_ptr<VulkanCore::Pipeline> sharpenPipeline_;
};
