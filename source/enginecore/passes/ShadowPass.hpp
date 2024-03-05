#pragma once
#include "LightData.hpp"
#include "enginecore/Camera.hpp"
#include "enginecore/Model.hpp"
#include "enginecore/RingBuffer.hpp"
#include "vulkancore/Framebuffer.hpp"
#include "vulkancore/Pipeline.hpp"
#include "vulkancore/RenderPass.hpp"
#include "vulkancore/Texture.hpp"

class ShadowPass {
 public:
  ShadowPass();

  void init(VulkanCore::Context* context);

  void render(VkCommandBuffer cmd, int frameIndex,
              const std::vector<VulkanCore::Pipeline::SetAndBindingIndex>& sets,
              VkBuffer indexBuffer, VkBuffer indirectDrawBuffer, uint32_t numMeshes,
              uint32_t bufferSize);

  std::shared_ptr<VulkanCore::Pipeline> pipeline() const { return pipeline_; }

  std::shared_ptr<VulkanCore::Texture> shadowDepthTexture() const {
    return depthTexture_;
  }

 private:
  void initTextures(VulkanCore::Context* context);

 private:
  VulkanCore::Context* context_ = nullptr;
  std::shared_ptr<VulkanCore::Texture> depthTexture_;

  std::shared_ptr<VulkanCore::RenderPass> renderPass_;

  std::unique_ptr<VulkanCore::Framebuffer> frameBuffer_;

  std::shared_ptr<VulkanCore::Pipeline> pipeline_;
};
