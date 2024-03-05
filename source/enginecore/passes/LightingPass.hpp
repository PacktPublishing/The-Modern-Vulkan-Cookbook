#pragma once
#include <gli/gli.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/type_aligned.hpp>

#include "LightData.hpp"
#include "vulkancore/Context.hpp"
#include "vulkancore/Framebuffer.hpp"
#include "vulkancore/Pipeline.hpp"
#include "vulkancore/RenderPass.hpp"
#include "vulkancore/Texture.hpp"

class LightingPass {
 public:
  LightingPass();
  void init(VulkanCore::Context* context,
            std::shared_ptr<VulkanCore::Texture> gBufferNormal,
            std::shared_ptr<VulkanCore::Texture> gBufferSpecular,
            std::shared_ptr<VulkanCore::Texture> gBufferBaseColor,
            std::shared_ptr<VulkanCore::Texture> gBufferPosition,
            std::shared_ptr<VulkanCore::Texture> gBufferDepth,
            std::shared_ptr<VulkanCore::Texture> ambientOcclusion,
            std::shared_ptr<VulkanCore::Texture> shadowDepth);
  void render(VkCommandBuffer cmd, uint32_t index, const LightData& data,
              const glm::mat4& viewMat, const glm::mat4& projMat);
  std::shared_ptr<VulkanCore::Pipeline> pipeline() const { return pipeline_; }

  std::shared_ptr<VulkanCore::RenderPass> renderPass() const { return renderPass_; }

  std::shared_ptr<VulkanCore::Texture> lightTexture() const {
    return outLightingTexture_;
  }

 private:
  VulkanCore::Context* context_ = nullptr;
  std::shared_ptr<VulkanCore::RenderPass> renderPass_;
  std::shared_ptr<VulkanCore::Pipeline> pipeline_;
  std::unique_ptr<VulkanCore::Framebuffer> frameBuffer_;

  std::shared_ptr<VulkanCore::Texture> outLightingTexture_;

  std::shared_ptr<VulkanCore::Texture> gBufferNormal_;
  std::shared_ptr<VulkanCore::Texture> gBufferSpecular_;
  std::shared_ptr<VulkanCore::Texture> gBufferBaseColor_;
  std::shared_ptr<VulkanCore::Texture> gBufferDepth_;
  std::shared_ptr<VulkanCore::Texture> gBufferPosition_;
  std::shared_ptr<VulkanCore::Texture> ambientOcclusion_;
  std::shared_ptr<VulkanCore::Texture> shadowDepth_;
  std::shared_ptr<VulkanCore::Sampler> sampler_;
  std::shared_ptr<VulkanCore::Sampler> samplerShadowMap_;

  std::shared_ptr<VulkanCore::Buffer> cameraBuffer_;
  std::shared_ptr<VulkanCore::Buffer> lightBuffer_;

  uint32_t width_ = 0;
  uint32_t height_ = 0;
};
