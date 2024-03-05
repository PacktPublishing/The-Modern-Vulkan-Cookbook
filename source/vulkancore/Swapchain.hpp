#pragma once

#include <memory>
#include <vector>

#include "Common.hpp"
#include "Utility.hpp"

namespace VulkanCore {

class Context;
class Framebuffer;
class PhysicalDevice;
class Texture;

class Swapchain final {
 public:
  explicit Swapchain() = default;
  explicit Swapchain(const Context& context,
                     const PhysicalDevice& physicalDevice, VkSurfaceKHR surface,
                     VkQueue presentQueue, VkFormat imageFormat,
                     VkColorSpaceKHR imageClorSpace,
                     VkPresentModeKHR presentMode, VkExtent2D extent,
                     const std::string& name = "");

  ~Swapchain();

  uint32_t numberImages() const {
    return static_cast<uint32_t>(images_.size());
  }

  size_t currentImageIndex() const { return imageIndex_; }

  std::shared_ptr<Texture> acquireImage();

  VkFormat imageFormat() const { return imageFormat_; }

  VkExtent2D extent() const { return extent_; }

  void present() const;

  VkSubmitInfo createSubmitInfo(const VkCommandBuffer* buffer,
                                const VkPipelineStageFlags* submitStageMask,
                                bool waitForImageAvailable = true,
                                bool signalImagePresented = true) const;

  std::shared_ptr<Texture> texture(uint32_t index) const {
    ASSERT(index < images_.size(),
           "Index is greater than number of images in the swapchain");
    return images_[index];
  }

 private:
  void createTextures(const Context& context, VkFormat imageFormat,
                      const VkExtent2D& extent);

  void createSemaphores(const Context& context);

 private:
  VkDevice device_ = VK_NULL_HANDLE;
  VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
  VkQueue presentQueue_ = VK_NULL_HANDLE;
  std::vector<std::shared_ptr<Texture>> images_;
  VkSemaphore imageAvailable_ = VK_NULL_HANDLE;
  VkSemaphore imageRendered_ = VK_NULL_HANDLE;
  uint32_t imageIndex_ = 0;
  VkExtent2D extent_;
  VkFormat imageFormat_;
  VkFence acquireFence_ = VK_NULL_HANDLE;
};

}  // namespace VulkanCore
