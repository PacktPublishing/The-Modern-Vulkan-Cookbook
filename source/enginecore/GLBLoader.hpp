#pragma once

#include <GLTFSDK/Deserialize.h>
#include <GLTFSDK/GLBResourceReader.h>
#include <GLTFSDK/GLTF.h>
#include <GLTFSDK/GLTFResourceReader.h>

#include "BS_thread_pool.hpp"
#include "Model.hpp"
#include "vulkancore/Common.hpp"

namespace VulkanCore {
class Buffer;
class Context;
class Texture;
class Sampler;
class CommandQueueManager;
}  // namespace VulkanCore

namespace EngineCore {

struct IndirectDrawCommandAndMeshData {
  VkDrawIndexedIndirectCommand command;

  uint32_t meshId;
  uint32_t materialIndex;
};

class GLBLoader {
 public:
  std::shared_ptr<Model> load(const std::vector<char>& buffer);
  std::shared_ptr<Model> load(const std::string& filePath);
  std::shared_ptr<Model> load(const std::string& filePath, BS::thread_pool& pool,
                              std::function<void(int, int)> callback);

 public:
  std::vector<std::future<int>> results_;

 private:
  void updateMeshData(std::shared_ptr<Microsoft::glTF::GLBResourceReader> resourceReader,
                      std::shared_ptr<Microsoft::glTF::Document> document,
                      Model& outputModel);
  void updateMaterials(std::shared_ptr<Microsoft::glTF::Document> document,
                       Model& outputModel);
};

/// @brief Produces one vertex and one index buffer for each mesh plus a buffer for the
/// scene materials. The buffers are interleaved: V0 I0 V1 I1 ... Vn In
void convertModel2OneMeshPerBuffer(
    const VulkanCore::Context& context, VulkanCore::CommandQueueManager& queueMgr,
    VkCommandBuffer commandBuffer, const Model& model,
    std::vector<std::shared_ptr<VulkanCore::Buffer>>& buffers,
    std::vector<std::shared_ptr<VulkanCore::Texture>>& textures,
    std::vector<std::shared_ptr<VulkanCore::Sampler>>& samplers,
    bool makeBuffersSuitableForAccelStruct = false);

void convertModel2OneMeshPerBuffer(
    const VulkanCore::Context& context, VulkanCore::CommandQueueManager& queueMgr,
    VkCommandBuffer commandBuffer, const Model& model,
    std::vector<std::shared_ptr<VulkanCore::Buffer>>& buffers,
    std::vector<std::shared_ptr<VulkanCore::Sampler>>& samplers, bool makeBuffersSuitableForAccelStruct = false);

/// @brief Produces 4 buffers:
///   [0] vertex buffer
///   [1] index buffer
///   [2] material buffer
///   [3] indirect draw command buffer
void convertModel2OneBuffer(const VulkanCore::Context& context,
                            VulkanCore::CommandQueueManager& queueMgr,
                            VkCommandBuffer commandBuffer, const Model& model,
                            std::vector<std::shared_ptr<VulkanCore::Buffer>>& buffers,
                            std::vector<std::shared_ptr<VulkanCore::Texture>>& textures,
                            std::vector<std::shared_ptr<VulkanCore::Sampler>>& samplers,
                            bool useHalfFloatVertices = false, bool makeBuffersSuitableForAccelStruct = false);

void convertModel2OneBuffer(const VulkanCore::Context& context,
                            VulkanCore::CommandQueueManager& queueMgr,
                            VkCommandBuffer commandBuffer, const Model& model,
                            std::vector<std::shared_ptr<VulkanCore::Buffer>>& buffers,
                            std::vector<std::shared_ptr<VulkanCore::Sampler>>& samplers,
                            bool useHalfFloatVertices = false,
                            bool makeBuffersSuitableForAccelStruct = false);

/// @brief Produces 3 buffers:
///   [0] (optimized) vertex buffer
///   [1] (optimized) index buffer
///   [2] material buffer
void convertModel2OneBufferOptimized(
    const VulkanCore::Context& context, VulkanCore::CommandQueueManager& queueMgr,
    VkCommandBuffer commandBuffer, const Model& model,
    std::vector<std::shared_ptr<VulkanCore::Buffer>>& buffers,
    std::vector<std::shared_ptr<VulkanCore::Texture>>& textures,
    std::vector<std::shared_ptr<VulkanCore::Sampler>>& samplers,
    bool makeBuffersSuitableForAccelStruct = false);
}  // namespace EngineCore
