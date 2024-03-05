#pragma once
#include "vulkancore/Context.hpp"
#include "vulkancore/Framebuffer.hpp"
#include "vulkancore/Pipeline.hpp"
#include "vulkancore/RenderPass.hpp"
#include "vulkancore/Texture.hpp"

namespace EngineCore {
namespace GUI {
class ImguiManager;
}
}  // namespace EngineCore

class FullScreenColorPass {
 public:
  explicit FullScreenColorPass() = default;
  void init(VulkanCore::Context* context, std::vector<VkFormat> colorTextureFormats);
  void render(VkCommandBuffer cmd, uint32_t index, const std::vector<glm::vec4>& color);
  std::shared_ptr<VulkanCore::Pipeline> pipeline() const { return pipeline_; }

  // std::shared_ptr<VulkanCore::RenderPass> renderPass() const { return renderPass_; }

  VkFramebuffer framebuffer(int index) const {
    return frameBuffers_[index]->vkFramebuffer();
  }

 private:
  VulkanCore::Context* context_ = nullptr;
  // std::shared_ptr<VulkanCore::RenderPass> renderPass_;
  std::shared_ptr<VulkanCore::Pipeline> pipeline_;

  std::vector<std::unique_ptr<VulkanCore::Framebuffer>> frameBuffers_;

  uint32_t width_ = 0;
  uint32_t height_ = 0;

  bool useDynamicRendering_ = false;
};
