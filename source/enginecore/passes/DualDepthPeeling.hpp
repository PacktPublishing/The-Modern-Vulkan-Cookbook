#pragma once
#include "LightData.hpp"
#include "enginecore/Camera.hpp"
#include "enginecore/Model.hpp"
#include "enginecore/RingBuffer.hpp"
#include "vulkancore/Pipeline.hpp"
#include "vulkancore/Sampler.hpp"
#include "vulkancore/Texture.hpp"

// assumes we are using dynamic rendering

class DualDepthPeeling final {
 public:
  explicit DualDepthPeeling(VulkanCore::Context* context) : context_{context} {}

  void init(VulkanCore::Context* context, const EngineCore::RingBuffer& cameraBuffer,
            EngineCore::RingBuffer& objectPropBuffer, size_t objectPropSize,
            uint32_t numMeshes, uint32_t numPeels, VkFormat colorTextureFormat,
            VkFormat depthTextureFormat,
            std::shared_ptr<VulkanCore::Texture> opaquePassDepth);

  void draw(VkCommandBuffer cmd, int index,
            const std::vector<std::shared_ptr<VulkanCore::Buffer>>& buffers,
            uint32_t numMeshes);

  std::shared_ptr<VulkanCore::Pipeline> pipeline() const { return pipeline_; }

  std::shared_ptr<VulkanCore::Texture> colorTexture() const { return colorTextures_[0]; }

 private:
  void init(uint32_t numMeshes, const VkVertexInputBindingDescription& vertexBindingDesc,
            const std::vector<VkVertexInputAttributeDescription>& vertexDescription);
  void initDepthTextures(VkFormat depthFormat);
  void initColorTextures(VkFormat colorTextureFormat);

 private:
  VulkanCore::Context* context_ = nullptr;
  uint32_t numPeels_ = 0;

  // Pipeline aux structures
  VkRect2D scissor_ = {};
  VkViewport viewport_ = {};

  // Color attachments (2x)
  std::array<std::shared_ptr<VulkanCore::Texture>, 2> colorTextures_;

  // Depth textures (2x)
  std::array<std::shared_ptr<VulkanCore::Texture>, 2> depthMinMaxTextures_;

  // Pipeline
  std::shared_ptr<VulkanCore::Pipeline> pipeline_;
  std::shared_ptr<VulkanCore::Pipeline> pipelineFinal_;

  // Sampler
  std::shared_ptr<VulkanCore::Sampler> sampler_;
};
