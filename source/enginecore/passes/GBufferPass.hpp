#pragma once
#include "enginecore/Camera.hpp"
#include "enginecore/Model.hpp"
#include "enginecore/RingBuffer.hpp"
#include "vulkancore/Framebuffer.hpp"
#include "vulkancore/Pipeline.hpp"
#include "vulkancore/RenderPass.hpp"
#include "vulkancore/Texture.hpp"

class GBufferPass {
 public:
  struct GBufferPushConstants {
    uint32_t applyJitter;
  };

  GBufferPass();

  void init(VulkanCore::Context* context, unsigned int width, unsigned int height);

  void render(VkCommandBuffer cmd, int frameIndex,
              const std::vector<VulkanCore::Pipeline::SetAndBindingIndex>& sets,
              VkBuffer indexBuffer, VkBuffer indirectDrawBuffer,
              VkBuffer indirectDrawCountBuffer, uint32_t numMeshes, uint32_t bufferSize,
              bool applyJitter = false);

  std::shared_ptr<VulkanCore::Pipeline> pipeline() const { return pipeline_; }

  std::shared_ptr<VulkanCore::Texture> baseColorTexture() const {
    return gBufferBaseColorTexture_;
  }

  std::shared_ptr<VulkanCore::Texture> positionTexture() const {
    return gBufferPositionTexture_;
  }

  std::shared_ptr<VulkanCore::Texture> normalTexture() const {
    return gBufferNormalTexture_;
  }

  std::shared_ptr<VulkanCore::Texture> emissiveTexture() const {
    return gBufferEmissiveTexture_;
  }

  std::shared_ptr<VulkanCore::Texture> specularTexture() const {
    return gBufferSpecularTexture_;
  }

  std::shared_ptr<VulkanCore::Texture> velocityTexture() const {
    return gBufferVelocityTexture_;
  }

  std::shared_ptr<VulkanCore::Texture> depthTexture() const { return depthTexture_; }

 private:
  void initTextures(VulkanCore::Context* context, unsigned int width,
                    unsigned int height);

 private:
  VulkanCore::Context* context_ = nullptr;
  std::shared_ptr<VulkanCore::Texture> gBufferBaseColorTexture_;
  std::shared_ptr<VulkanCore::Texture> gBufferNormalTexture_;
  std::shared_ptr<VulkanCore::Texture> gBufferEmissiveTexture_;
  std::shared_ptr<VulkanCore::Texture> gBufferSpecularTexture_;
  std::shared_ptr<VulkanCore::Texture> gBufferPositionTexture_;
  std::shared_ptr<VulkanCore::Texture> gBufferVelocityTexture_;
  std::shared_ptr<VulkanCore::Texture> depthTexture_;

  std::shared_ptr<VulkanCore::RenderPass> renderPass_;

  std::unique_ptr<VulkanCore::Framebuffer> frameBuffer_;

  std::shared_ptr<VulkanCore::Pipeline> pipeline_;
};
