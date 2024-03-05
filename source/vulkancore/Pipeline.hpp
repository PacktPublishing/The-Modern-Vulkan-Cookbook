#pragma once

#include <list>
#include <memory>
#include <mutex>
#include <span>
#include <unordered_map>

#include "Common.hpp"
#include "Utility.hpp"

namespace VulkanCore {

class Context;
class Buffer;
class RenderPass;
class Sampler;
class ShaderModule;
class Texture;

constexpr uint32_t MAX_DESC_BINDLESS = 1000;

class Pipeline final {
 public:
  struct SetDescriptor {
    uint32_t set_;
    std::vector<VkDescriptorSetLayoutBinding> bindings_;
  };

  struct ViewPort {
    ViewPort(const VkExtent2D& extents) { viewport_ = fromExtents(extents); }
    ViewPort() = default;
    ViewPort(const ViewPort&) = default;
    ViewPort& operator=(const ViewPort&) = default;

    ViewPort(const VkViewport& viewport) : viewport_(viewport) {}

    ViewPort& operator=(const VkViewport& viewport) {
      viewport_ = viewport;
      return *this;
    }

    ViewPort& operator=(const VkExtent2D& extents) {
      viewport_ = fromExtents(extents);
      return *this;
    }

    VkExtent2D toVkExtents() {
      return VkExtent2D{static_cast<uint32_t>(std::abs(viewport_.width)),
                        static_cast<uint32_t>(std::abs(viewport_.height))};
    }
    VkViewport toVkViewPort() { return viewport_; }

   private:
    VkViewport fromExtents(const ::VkExtent2D& extents) {
      VkViewport v;
      v.x = 0;
      v.y = 0;
      v.width = extents.width;
      v.height = extents.height;
      v.minDepth = 0.0;
      v.maxDepth = 1.0;
      return v;
    }

    VkViewport viewport_ = {};
  };

  struct GraphicsPipelineDescriptor {
    std::vector<SetDescriptor> sets_;
    std::weak_ptr<ShaderModule> vertexShader_;
    std::weak_ptr<ShaderModule> fragmentShader_;
    std::vector<VkPushConstantRange> pushConstants_;
    std::vector<VkDynamicState> dynamicStates_;
    bool useDynamicRendering_ = false;
    std::vector<VkFormat> colorTextureFormats;
    VkFormat depthTextureFormat = VK_FORMAT_UNDEFINED;
    VkFormat stencilTextureFormat = VK_FORMAT_UNDEFINED;

    VkPrimitiveTopology primitiveTopology =
        VkPrimitiveTopology::VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkSampleCountFlagBits sampleCount = VK_SAMPLE_COUNT_1_BIT;
    VkCullModeFlagBits cullMode = VK_CULL_MODE_BACK_BIT;
    VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    ViewPort viewport;
    bool blendEnable = false;
    uint32_t numberBlendAttachments = 0u;
    bool depthTestEnable = true;
    bool depthWriteEnable = true;
    VkCompareOp depthCompareOperation = VK_COMPARE_OP_LESS;

    VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = 0,
        .vertexAttributeDescriptionCount = 0,
    };
    std::vector<VkSpecializationMapEntry> vertexSpecConstants_;
    std::vector<VkSpecializationMapEntry> fragmentSpecConstants_;
    void* vertexSpecializationData = nullptr;
    void* fragmentSpecializationData = nullptr;

    std::vector<VkPipelineColorBlendAttachmentState> blendAttachmentStates_;
  };

  struct ComputePipelineDescriptor {
    std::vector<SetDescriptor> sets_;
    std::weak_ptr<ShaderModule> computeShader_;
    std::vector<VkPushConstantRange> pushConstants_;
    std::vector<VkSpecializationMapEntry> specializationConsts_;
    void* specializationData_ = nullptr;
  };

  struct RayTracingPipelineDescriptor {
    std::vector<SetDescriptor> sets_;
    std::weak_ptr<ShaderModule> rayGenShader_;
    std::vector<std::weak_ptr<ShaderModule>> rayMissShaders_;
    std::vector<std::weak_ptr<ShaderModule>> rayClosestHitShaders_;
    std::vector<VkPushConstantRange> pushConstants_;

    // Add specialization const, but they are needed per shaderModule?
  };

  explicit Pipeline(const Context* context, const GraphicsPipelineDescriptor& desc,
                    VkRenderPass renderPass, const std::string& name = "");

  explicit Pipeline(const Context* context, const ComputePipelineDescriptor& desc,
                    const std::string& name = "");

  explicit Pipeline(const Context* context, const RayTracingPipelineDescriptor& desc,
                    const std::string& name = "");

  ~Pipeline();

  bool valid() const { return vkPipeline_ == VK_NULL_HANDLE; }

  VkPipeline vkPipeline() const;

  VkPipelineLayout vkPipelineLayout() const;

  void updatePushConstant(VkCommandBuffer commandBuffer, VkShaderStageFlags flags,
                          uint32_t size, const void* data);

  void bind(VkCommandBuffer commandBuffer);

  void bindVertexBuffer(VkCommandBuffer commandBuffer, VkBuffer vertexBuffer);

  void bindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer indexBuffer);

  struct SetAndCount {
    uint32_t set_;
    uint32_t count_;
    std::string name_;
  };
  void allocateDescriptors(const std::vector<SetAndCount>& setAndCount);

  struct SetAndBindingIndex {
    uint32_t set;
    uint32_t bindIdx;
  };
  void bindDescriptorSets(VkCommandBuffer commandBuffer,
                          const std::vector<SetAndBindingIndex>& sets);

  struct SetBindings {
    uint32_t set_ = 0;
    uint32_t binding_ = 0;
    std::span<std::shared_ptr<VulkanCore::Texture>> textures_;
    std::span<std::shared_ptr<VulkanCore::Sampler>> samplers_;
    std::shared_ptr<VulkanCore::Buffer> buffer;
    uint32_t index_ = 0;
    uint32_t offset_ = 0;
    VkDeviceSize bufferBytes = 0;
  };

  // void updateDescriptorSets(uint32_t set, uint32_t index,
  //                           const std::vector<SetBindings>& bindings);
  void updateSamplersDescriptorSets(uint32_t set, uint32_t index,
                                    const std::vector<SetBindings>& bindings);
  void updateTexturesDescriptorSets(uint32_t set, uint32_t index,
                                    const std::vector<SetBindings>& bindings);
  void updateBuffersDescriptorSets(uint32_t set, uint32_t index, VkDescriptorType type,
                                   const std::vector<SetBindings>& bindings);
  void updateDescriptorSets();

  /// @brief Assigns the resource to a position in the resource array specific
  /// to te resource's type
  void bindResource(uint32_t set, uint32_t binding, uint32_t index,
                    std::shared_ptr<Buffer> buffer, uint32_t offset, uint32_t size,
                    VkDescriptorType type, VkFormat format = VK_FORMAT_UNDEFINED);
  // if sampler is passed, it applies to all textures
  void bindResource(uint32_t set, uint32_t binding, uint32_t index,
                    std::span<std::shared_ptr<Texture>> textures,
                    std::shared_ptr<Sampler> sampler = nullptr,
                    uint32_t dstArrayElement = 0);
  void bindResource(uint32_t set, uint32_t binding, uint32_t index,
                    std::span<std::shared_ptr<Sampler>> samplers);

  void bindResource(uint32_t set, uint32_t binding, uint32_t index,
                    std::span<std::shared_ptr<VkImageView>> imageViews,
                    VkDescriptorType type);

  void bindResource(uint32_t set, uint32_t binding, uint32_t index,
                    std::vector<std::shared_ptr<Buffer>> buffers, VkDescriptorType type);

  void bindResource(uint32_t set, uint32_t binding, uint32_t index,
                    std::shared_ptr<Texture> texture, VkDescriptorType type);

  void bindResource(uint32_t set, uint32_t binding, uint32_t index,
                    std::shared_ptr<Texture> texture, std::shared_ptr<Sampler> sampler,
                    VkDescriptorType type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER);

  void bindResource(uint32_t set, uint32_t binding, uint32_t index,
                    VkAccelerationStructureKHR* accelStructHandle);

 private:
  void createGraphicsPipeline();

  void createComputePipeline();

  void createRayTracingPipeline();

  VkPipelineLayout createPipelineLayout(
      const std::vector<VkDescriptorSetLayout>& descLayouts,
      const std::vector<VkPushConstantRange>& pushConsts) const;

  void initDescriptorPool();
  void initDescriptorLayout();

 private:
  const Context* context_ = nullptr;
  std::string name_;
  GraphicsPipelineDescriptor graphicsPipelineDesc_;
  ComputePipelineDescriptor computePipelineDesc_;
  RayTracingPipelineDescriptor rayTracingPipelineDesc_;
  VkPipelineBindPoint bindPoint_ = VK_PIPELINE_BIND_POINT_GRAPHICS;
  VkPipeline vkPipeline_ = VK_NULL_HANDLE;
  VkPipelineLayout vkPipelineLayout_ = VK_NULL_HANDLE;
  VkRenderPass vkRenderPass_ = VK_NULL_HANDLE;

  struct DescriptorSet {
    std::vector<VkDescriptorSet> vkSets_;
    VkDescriptorSetLayout vkLayout_ = VK_NULL_HANDLE;
  };
  std::unordered_map<uint32_t, DescriptorSet> descriptorSets_;
  VkDescriptorPool vkDescriptorPool_ = VK_NULL_HANDLE;
  std::vector<VkPushConstantRange> pushConsts_;  // IDK

  std::list<std::vector<VkDescriptorBufferInfo>> bufferInfo_;
  std::list<VkBufferView> bufferViewInfo_;
  std::list<std::vector<VkDescriptorImageInfo>> imageInfo_;
  std::vector<VkWriteDescriptorSetAccelerationStructureKHR> accelerationStructInfo_;
  std::vector<VkWriteDescriptorSet> writeDescSets_;
  std::mutex mutex_;
};

}  // namespace VulkanCore
