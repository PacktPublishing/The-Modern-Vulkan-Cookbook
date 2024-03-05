#pragma once

#include <glm/glm.hpp>
#include <string>
#include <vector>

#include "vulkancore/CommandQueueManager.hpp"
#include "vulkancore/Common.hpp"
#include "vulkancore/Texture.hpp"

struct NVSDK_NGX_Handle;
struct NVSDK_NGX_Parameter;

namespace EngineCore {
class DLSS {
 public:
  DLSS(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device);

  void init(int currentWidth, int currentHeight, float upScaleFactor,
            VulkanCore::CommandQueueManager& commandQueueManager);

  void render(VkCommandBuffer commandBuffer, VulkanCore::Texture& inColorTexture,
              VulkanCore::Texture& inDepthTexture,
              VulkanCore::Texture& inMotionVectorTexture,
              VulkanCore::Texture& outColorTexture, glm::vec2 cameraJitter);

  ~DLSS();

  static void requiredExtensions(std::vector<std::string>& instanceExtensions,
                                 std::vector<std::string>& deviceExtensions);

  bool isSupported() const { return supported_; }

 private:
  bool supported_ = true;
  float upScaleFactor_ = 1.0;

  NVSDK_NGX_Parameter* paramsDLSS_ = nullptr;
  NVSDK_NGX_Handle* dlssFeatureHandle_ = nullptr;
};
}  // namespace EngineCore
