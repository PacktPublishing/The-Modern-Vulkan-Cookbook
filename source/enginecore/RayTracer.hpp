#pragma once

#include <glm/glm.hpp>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "enginecore/Model.hpp"
#include "vulkancore/Buffer.hpp"
#include "vulkancore/CommandQueueManager.hpp"
#include "vulkancore/Common.hpp"
#include "vulkancore/Context.hpp"
#include "vulkancore/Texture.hpp"

namespace EngineCore {
class RayTracer {
 public:
  explicit RayTracer() = default;

  ~RayTracer();

  void init(VulkanCore::Context* context, std::shared_ptr<EngineCore::Model> model,
            std::vector<std::shared_ptr<VulkanCore::Buffer>> buffers,
            std::vector<std::shared_ptr<VulkanCore::Texture>> textures,
            std::vector<std::shared_ptr<VulkanCore::Sampler>> samplers);

  void initBottomLevelAccelStruct(
      std::shared_ptr<EngineCore::Model> model,
      std::vector<std::shared_ptr<VulkanCore::Buffer>> buffers);

  void initTopLevelAccelStruct(std::shared_ptr<EngineCore::Model> model,
                               std::vector<std::shared_ptr<VulkanCore::Buffer>> buffers);

  void execute(VkCommandBuffer commandBuffer, uint32_t swapchainIndex,
               const glm::mat4& viewMat, const glm::mat4& projMat, bool showAOImage);

  std::shared_ptr<VulkanCore::Texture> currentImage(int index) { return rayTracedImage_; }

 private:
  // Information about Shader binding table
  struct SBT {
    std::shared_ptr<VulkanCore::Buffer> buffer;
    VkStridedDeviceAddressRegionKHR sbtAddress;
  };

  // Holds information for a ray tracing acceleration structure
  struct AccelerationStructure {
    std::shared_ptr<VulkanCore::Buffer> buffer;
    VkAccelerationStructureKHR handle;
  };

  void createShaderBindingTable();

  void loadEnvMap();

  void initRayTracedStorageImages();

  VulkanCore::Context* context_ = nullptr;
  std::shared_ptr<VulkanCore::Pipeline> pipeline_;

  SBT raygenSBT_;
  SBT raymissSBT_;
  SBT rayclosestHitSBT_;

  std::shared_ptr<VulkanCore::Texture> envMap_;
  std::shared_ptr<VulkanCore::Buffer> envMapAccelBuffer_;

  std::unordered_map<uint32_t, AccelerationStructure> bLAS_;
  std::vector<VkAccelerationStructureInstanceKHR> accelarationInstances_;
  AccelerationStructure tLAS_;

  std::shared_ptr<VulkanCore::Texture> rayTracedImage_;
  std::shared_ptr<VulkanCore::Texture> rayTracedaccumImage_;

  std::shared_ptr<VulkanCore::Sampler> sampler_;

  std::shared_ptr<VulkanCore::Buffer> cameraMatBuffer_;

  glm::mat4 prevViewMat_;

  bool prevshowAOImage_ = false;

  unsigned int frameId_ = 0;
};
}  // namespace EngineCore
