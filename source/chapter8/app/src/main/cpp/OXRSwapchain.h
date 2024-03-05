#ifndef OPENXR_SAMPLE_OXRSWAPCHAIN_H
#define OPENXR_SAMPLE_OXRSWAPCHAIN_H

#include <jni.h>

#include <array>
#include <vector>
#include <string>

#include <vulkancore/Context.hpp>
#include <vulkancore/Texture.hpp>

//#include <vulkan/vulkan.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <glm/glm.hpp>

#include "Common.h"

namespace OXR {

    class OXRSwapchain {
    public:
        OXRSwapchain(VulkanCore::Context &ctx,
                     const XrSession &session,
                     const XrViewConfigurationView &viewport,
                     uint32_t numViews);

        ~OXRSwapchain();

        bool initialize();

        XrSwapchain createXrSwapchain(XrSwapchainUsageFlags usageFlags, int64_t format);

        enum class SwapChainType {
            Color = 0, Depth = 1
        };

        void enumerateImages(XrSwapchain colorSwapchain,
                             XrSwapchain depthSwapchain,
                             int64_t selectedColorFormat,
                             int64_t selectedDepthFormat,
                             const XrViewConfigurationView &viewport,
                             uint32_t numViews);

        void enumerateSwapchainImages(XrSwapchain colorSwapchain,
                                      int64_t selectedColorFormat,
                                      const XrViewConfigurationView &viewport,
                                      uint32_t numViews, VkImageUsageFlags usageFlags, uint32_t numviews,
                                      VkImageAspectFlags aspectMask, SwapChainType swapChainType);

        struct SwapchainTextures {
            std::shared_ptr<VulkanCore::Texture> color_;
            std::shared_ptr<VulkanCore::Texture> depth_;
        };

        SwapchainTextures getSurfaceTextures();

        std::shared_ptr<VulkanCore::Texture> getSurfaceTexture(
            const XrSwapchain &swapchain,
            const XrViewConfigurationView &viewport,
            uint32_t numViews,
            int64_t externalTextureFormat,
            SwapChainType swapchainType);

        uint32_t numImages() const { return numImages_; }

        uint32_t currentImageIndex() const { return currentImageIndex_; }

        const std::shared_ptr<VulkanCore::Texture> colorTexture(uint32_t index) const;

        const std::shared_ptr<VulkanCore::Texture> depthTexture(uint32_t index) const;

        void releaseSwapchainImages() const;

        const XrViewConfigurationView& viewport() const {
          return viewport_;
        }

    public:
        XrSwapchain colorSwapchain_ = XR_NULL_HANDLE;
        XrSwapchain depthSwapchain_ = XR_NULL_HANDLE;

    private:
        VulkanCore::Context &ctx_;
        const XrSession &session_;
        const XrViewConfigurationView &viewport_;
        int64_t selectedColorFormat_;
        int64_t selectedDepthFormat_;
        mutable uint32_t currentImageIndex_ = 0;
        const uint32_t numViews_ = 1;
        uint32_t numImages_ = 0;

        // Color index = 0, Depth index = 1
        std::vector<std::vector<std::shared_ptr<VulkanCore::Texture>>> vulkanTextures_;
    };

} // namespace OXR

#endif //OPENXR_SAMPLE_OXRSWAPCHAIN_H
