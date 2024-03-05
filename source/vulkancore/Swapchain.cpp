#include "Swapchain.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <tracy/Tracy.hpp>

#include "Context.hpp"
#include "Framebuffer.hpp"
#include "PhysicalDevice.hpp"
#include "Texture.hpp"

namespace VulkanCore {

Swapchain::Swapchain(const Context& context, const PhysicalDevice& physicalDevice,
                     VkSurfaceKHR surface, VkQueue presentQueue, VkFormat imageFormat,
                     VkColorSpaceKHR imageClorSpace, VkPresentModeKHR presentMode,
                     VkExtent2D extent, const std::string& name)
    : device_{context.device()}, presentQueue_{presentQueue}, extent_{extent} {
  const uint32_t numImages =
      std::clamp(physicalDevice.surfaceCapabilities().minImageCount + 1,
                 physicalDevice.surfaceCapabilities().minImageCount,
                 physicalDevice.surfaceCapabilities().maxImageCount);

  const auto presentationFamilyIndex = physicalDevice.presentationFamilyIndex();
  ASSERT(presentationFamilyIndex.has_value(),
         "There are no presentation queues available for the swapchain");

  const bool presentationQueueIsShared =
      physicalDevice.graphicsFamilyIndex().value() == presentationFamilyIndex.value();

  std::array<uint32_t, 2> familyIndices{physicalDevice.graphicsFamilyIndex().value(),
                                        presentationFamilyIndex.value()};
  const VkSwapchainCreateInfoKHR swapchainInfo = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = surface,
      .minImageCount = numImages,
      .imageFormat = imageFormat,
      .imageColorSpace = imageClorSpace,
      .imageExtent = extent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
      .imageSharingMode = presentationQueueIsShared ? VK_SHARING_MODE_EXCLUSIVE
                                                    : VK_SHARING_MODE_CONCURRENT,
      .queueFamilyIndexCount = presentationQueueIsShared ? 0u : 2u,
      .pQueueFamilyIndices = presentationQueueIsShared ? nullptr : familyIndices.data(),
      .preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .presentMode = presentMode,
      .clipped = VK_TRUE,
      .oldSwapchain = VK_NULL_HANDLE,
  };
  VK_CHECK(vkCreateSwapchainKHR(device_, &swapchainInfo, nullptr, &swapchain_));
  context.setVkObjectname(swapchain_, VK_OBJECT_TYPE_SWAPCHAIN_KHR, "Swapchain: " + name);

  createTextures(context, imageFormat, extent);

  createSemaphores(context);

  const VkFenceCreateInfo fenceci = {
      .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
      .flags = VK_FENCE_CREATE_SIGNALED_BIT,
  };
  VK_CHECK(vkCreateFence(device_, &fenceci, nullptr, &acquireFence_));
}

Swapchain::~Swapchain() {
  VK_CHECK(vkWaitForFences(device_, 1, &acquireFence_, VK_TRUE, UINT64_MAX));
  vkDestroyFence(device_, acquireFence_, nullptr);
  vkDestroySemaphore(device_, imageRendered_, nullptr);
  vkDestroySemaphore(device_, imageAvailable_, nullptr);
  vkDestroySwapchainKHR(device_, swapchain_, nullptr);
}

std::shared_ptr<Texture> Swapchain::acquireImage() {
  ZoneScopedN("Swapchain: acquireImage");

  VK_CHECK(vkWaitForFences(device_, 1, &acquireFence_, VK_TRUE, UINT64_MAX));
  VK_CHECK(vkResetFences(device_, 1, &acquireFence_));

  VK_CHECK(vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, imageAvailable_,
                                 acquireFence_, &imageIndex_));
  return images_[imageIndex_];
}

VkSubmitInfo Swapchain::createSubmitInfo(const VkCommandBuffer* buffer,
                                         const VkPipelineStageFlags* submitStageMask,
                                         bool waitForImageAvailable,
                                         bool signalImagePresented) const {
  const VkSubmitInfo si = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = waitForImageAvailable ? (imageAvailable_ ? 1u : 0) : 0,
      .pWaitSemaphores = waitForImageAvailable ? &imageAvailable_ : VK_NULL_HANDLE,
      .pWaitDstStageMask = submitStageMask,
      .commandBufferCount = 1,
      .pCommandBuffers = buffer,
      .signalSemaphoreCount = signalImagePresented ? (imageRendered_ ? 1u : 0) : 0,
      .pSignalSemaphores = signalImagePresented ? &imageRendered_ : VK_NULL_HANDLE,
  };
  return si;
}

void Swapchain::present() const {
  ZoneScopedN("Swapchain: present");
  const VkPresentInfoKHR presentInfo{
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &imageRendered_,
      .swapchainCount = 1,
      .pSwapchains = &swapchain_,
      .pImageIndices = &imageIndex_,
  };
  VK_CHECK(vkQueuePresentKHR(presentQueue_, &presentInfo));
}

void Swapchain::createTextures(const Context& context, VkFormat imageFormat,
                               const VkExtent2D& extent) {
  uint32_t imageCount{0};
  VK_CHECK(vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr));
  std::vector<VkImage> images(imageCount);
  VK_CHECK(vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, images.data()));

  images_.reserve(imageCount);
  for (size_t index = 0; index < imageCount; ++index) {
    images_.emplace_back(
        std::make_shared<Texture>(context, device_, images[index], imageFormat,
                                  VkExtent3D{
                                      .width = extent.width,
                                      .height = extent.height,
                                      .depth = 1,
                                  },
                                  1,  // number of layers
                                  false, "Swapchain image " + std::to_string(index)));
  }
}

void Swapchain::createSemaphores(const Context& context) {
  const VkSemaphoreCreateInfo semaphoreInfo{
      .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
  };
  VK_CHECK(vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailable_));
  context.setVkObjectname(imageAvailable_, VK_OBJECT_TYPE_SEMAPHORE,
                          "Semaphore: swapchain image available semaphore");

  VK_CHECK(vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageRendered_));
  context.setVkObjectname(imageRendered_, VK_OBJECT_TYPE_SEMAPHORE,
                          "Semaphore: swapchain image presented semaphore");
}

}  // namespace VulkanCore
