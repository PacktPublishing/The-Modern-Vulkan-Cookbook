#include "OXRSwapchain.h"

namespace OXR {

    OXRSwapchain::OXRSwapchain(VulkanCore::Context &ctx,
                               const XrSession &session,
                               const XrViewConfigurationView &viewport,
                               uint32_t numViews) : ctx_(ctx), session_(session),
                                                    viewport_(viewport),
                                                    numViews_(numViews) {
      vulkanTextures_.resize(2);
    }

    OXRSwapchain::~OXRSwapchain() {
      xrDestroySwapchain(colorSwapchain_);
      xrDestroySwapchain(depthSwapchain_);
    }

    bool OXRSwapchain::initialize() {
      uint32_t numSwapchainFormats = 0;
      XR_CHECK(xrEnumerateSwapchainFormats(session_, 0, &numSwapchainFormats, nullptr));
      LOGI("Number of XrSwapchain formats supported is %u", numSwapchainFormats);

      std::vector<int64_t> swapchainFormats(numSwapchainFormats);
      XR_CHECK(xrEnumerateSwapchainFormats(
          session_, numSwapchainFormats, &numSwapchainFormats, swapchainFormats.data()));

      LOGI("XrSwapchain formats supported:");
      for (uint32_t i = 0; i < swapchainFormats.size(); ++i) {
        LOGI("\t%zd", swapchainFormats[i]);
      }

      const auto colorFormat = VK_FORMAT_R8G8B8A8_UNORM;
      if (std::any_of(std::begin(swapchainFormats),
                      std::end(swapchainFormats),
                      [&](const auto &format) { return format == colorFormat; })) {
        selectedColorFormat_ = colorFormat;
      }

      colorSwapchain_ =
          createXrSwapchain(XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT, selectedColorFormat_);

      const auto depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;
      if (std::any_of(std::begin(swapchainFormats),
                      std::end(swapchainFormats),
                      [&](const auto &format) { return format == depthFormat; })) {
        selectedDepthFormat_ = depthFormat;
      }

      depthSwapchain_ =
          createXrSwapchain(XR_SWAPCHAIN_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, selectedDepthFormat_);

      enumerateImages(colorSwapchain_,
                      depthSwapchain_,
                      selectedColorFormat_,
                      selectedDepthFormat_,
                      viewport_,
                      numViews_);

      return true;
    }

    XrSwapchain OXRSwapchain::createXrSwapchain(XrSwapchainUsageFlags usageFlags, int64_t format) {

      const XrVulkanSwapchainCreateInfoMETA vulkanImageAdditionalFlags{
          .type = XR_TYPE_VULKAN_SWAPCHAIN_CREATE_INFO_META,
          .next = nullptr,
          .additionalCreateFlags = VK_IMAGE_CREATE_FRAGMENT_DENSITY_MAP_OFFSET_BIT_QCOM,
      };

      const XrSwapchainCreateInfo swapChainCreateInfo = {
          .type = XR_TYPE_SWAPCHAIN_CREATE_INFO,
          .next = &vulkanImageAdditionalFlags,
          .usageFlags = XR_SWAPCHAIN_USAGE_SAMPLED_BIT | usageFlags,
          .format = format,
          .sampleCount = 1,
          .width = viewport_.recommendedImageRectWidth,
          .height = viewport_.recommendedImageRectHeight,
          .faceCount = 1,
          .arraySize = numViews_,
          .mipCount = 1,};

      XrSwapchain swapchain;
      XR_CHECK(xrCreateSwapchain(session_, &swapChainCreateInfo, &swapchain));

      return swapchain;
    }

    void OXRSwapchain::enumerateImages(XrSwapchain colorSwapchain,
                                       XrSwapchain depthSwapchain,
                                       int64_t selectedColorFormat,
                                       int64_t selectedDepthFormat,
                                       const XrViewConfigurationView &viewport,
                                       uint32_t numViews) {
      enumerateSwapchainImages(colorSwapchain,
                               selectedColorFormat,
                               viewport,
                               numViews,
                               VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                               numViews,
                               VK_IMAGE_ASPECT_COLOR_BIT,
                               SwapChainType::Color);

      auto vkDepthFormat = static_cast<VkFormat>(selectedDepthFormat);
      VkImageAspectFlags depthAspectFlags = 0;
      if ((vkDepthFormat == VK_FORMAT_D16_UNORM || vkDepthFormat == VK_FORMAT_D16_UNORM_S8_UINT ||
           vkDepthFormat == VK_FORMAT_D24_UNORM_S8_UINT || vkDepthFormat == VK_FORMAT_D32_SFLOAT ||
           vkDepthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT ||
           vkDepthFormat == VK_FORMAT_X8_D24_UNORM_PACK32)) {
        depthAspectFlags |= VK_IMAGE_ASPECT_DEPTH_BIT;
      }
      if (vkDepthFormat == VK_FORMAT_S8_UINT || vkDepthFormat == VK_FORMAT_D16_UNORM_S8_UINT ||
          vkDepthFormat == VK_FORMAT_D24_UNORM_S8_UINT ||
          vkDepthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT) {
        depthAspectFlags |= VK_IMAGE_ASPECT_STENCIL_BIT;
      }
      enumerateSwapchainImages(depthSwapchain,
                               selectedDepthFormat,
                               viewport,
                               numViews,
                               VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                               numViews,
                               depthAspectFlags,
                               SwapChainType::Depth);
    }

    void OXRSwapchain::enumerateSwapchainImages(
        XrSwapchain swapchain,
        int64_t format,
        const XrViewConfigurationView &viewport,
        uint32_t numViews,
        VkImageUsageFlags usageFlags,
        uint32_t numviews,
        VkImageAspectFlags aspectMask,
        SwapChainType swapChainType) {
      XR_CHECK(xrEnumerateSwapchainImages(swapchain, 0, &numImages_, NULL));

      LOGI("xrEnumerateSwapchainImages numImages_: %d", numImages_);

      std::vector<XrSwapchainImageVulkanKHR> images(
          numImages_, {.type = XR_TYPE_SWAPCHAIN_IMAGE_VULKAN_KHR, .next = nullptr});
      XR_CHECK(xrEnumerateSwapchainImages(
          swapchain, numImages_, &numImages_, (XrSwapchainImageBaseHeader *) images.data()));

      uint32_t swapchainTypeIndex = static_cast<uint32_t>(swapChainType);
      const std::string type = swapChainType == SwapChainType::Color ? "color" : "depth";
      for (uint32_t i = 0; i < numImages_; i++) {
        const std::string debugName = type + " swapchain " + std::to_string(i);
        LOGI("Creating swapchain image %s (%p)", debugName.c_str(), images[i].image);
        LOGI("vulkanTextures_ size is %zu", vulkanTextures_[swapchainTypeIndex].size());
        vulkanTextures_[swapchainTypeIndex].push_back(
            std::make_shared<VulkanCore::Texture>(ctx_, ctx_.device(), images[i].image,
                                                  static_cast<VkFormat>(format),
                                                  VkExtent3D{viewport.recommendedImageRectWidth,
                                                             viewport.recommendedImageRectHeight,
                                                             1},
                                                  numviews,
                                                  numviews > 1,
                                                  debugName));
        LOGI("Created swapchain image %s", debugName.c_str());
      }
    }

    OXRSwapchain::SwapchainTextures OXRSwapchain::getSurfaceTextures() {
      auto colorTexture = getSurfaceTexture(colorSwapchain_,
                                            viewport_,
                                            numViews_,
                                            selectedColorFormat_, SwapChainType::Color);
      auto depthTexture = getSurfaceTexture(depthSwapchain_,
                                            viewport_,
                                            numViews_,
                                            selectedDepthFormat_, SwapChainType::Depth);

      return {colorTexture, depthTexture};
    }

    std::shared_ptr<VulkanCore::Texture> OXRSwapchain::getSurfaceTexture(
        const XrSwapchain &swapchain,
        const XrViewConfigurationView &viewport,
        uint32_t numViews,
        int64_t externalTextureFormat,
        SwapChainType swapchainType) {
      XrSwapchainImageAcquireInfo acquireInfo{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};
      XR_CHECK(xrAcquireSwapchainImage(swapchain, &acquireInfo, &currentImageIndex_));

      XrSwapchainImageWaitInfo waitInfo{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
      waitInfo.timeout = XR_INFINITE_DURATION;
      XR_CHECK(xrWaitSwapchainImage(swapchain, &waitInfo));

      uint32_t swapchainTypeIndex = static_cast<uint32_t>(swapchainType);
      auto &textureVector = vulkanTextures_[swapchainTypeIndex];
      // auto vulkanTexture = textureVector[currentImageIndex_];

      return textureVector[currentImageIndex_];
    }

    const std::shared_ptr<VulkanCore::Texture> OXRSwapchain::colorTexture(uint32_t index) const {
      assert(index < vulkanTextures_[static_cast<uint32_t>(SwapChainType::Color)].size());
      return vulkanTextures_[static_cast<uint32_t>(SwapChainType::Color)][index];
    }

    const std::shared_ptr<VulkanCore::Texture> OXRSwapchain::depthTexture(uint32_t index) const {
      assert(index < vulkanTextures_[static_cast<uint32_t>(SwapChainType::Depth)].size());
      return vulkanTextures_[static_cast<uint32_t>(SwapChainType::Depth)][index];
    }

    void OXRSwapchain::releaseSwapchainImages() const {
      XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};
      XR_CHECK(xrReleaseSwapchainImage(colorSwapchain_, &releaseInfo));
      XR_CHECK(xrReleaseSwapchainImage(depthSwapchain_, &releaseInfo));
    }

} // namespace OXR