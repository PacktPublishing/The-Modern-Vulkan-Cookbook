#include "Pipeline.hpp"

#include "Buffer.hpp"
#include "Context.hpp"
#include "RenderPass.hpp"
#include "Sampler.hpp"
#include "Texture.hpp"

namespace VulkanCore {

static constexpr int MAX_DESCRIPTOR_SETS = 4096 * 3;

Pipeline::Pipeline(const Context* context, const GraphicsPipelineDescriptor& desc,
                   VkRenderPass renderPass, const std::string& name)
    : context_(context),
      graphicsPipelineDesc_(desc),
      bindPoint_(VK_PIPELINE_BIND_POINT_GRAPHICS),
      vkRenderPass_(renderPass),
      name_{name} {
  createGraphicsPipeline();
}

Pipeline::Pipeline(const Context* context, const ComputePipelineDescriptor& desc,
                   const std::string& name /*= ""*/)
    : context_(context),
      computePipelineDesc_(desc),
      bindPoint_(VK_PIPELINE_BIND_POINT_COMPUTE),
      name_{name} {
  createComputePipeline();
}

Pipeline::Pipeline(const Context* context, const RayTracingPipelineDescriptor& desc,
                   const std::string& name /*= ""*/)
    : context_(context),
      rayTracingPipelineDesc_(desc),
      bindPoint_(VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR),
      name_{name} {
  createRayTracingPipeline();
}

Pipeline::~Pipeline() {
  const auto device = context_->device();

  vkDestroyPipeline(device, vkPipeline_, nullptr);
  vkDestroyPipelineLayout(device, vkPipelineLayout_, nullptr);
  vkDestroyDescriptorPool(device, vkDescriptorPool_, nullptr);

  for (const auto& set : descriptorSets_) {
    vkDestroyDescriptorSetLayout(device, set.second.vkLayout_, nullptr);
  }
}

VkPipeline Pipeline::vkPipeline() const { return vkPipeline_; }

VkPipelineLayout Pipeline::vkPipelineLayout() const { return vkPipelineLayout_; }

void Pipeline::updatePushConstant(VkCommandBuffer commandBuffer, VkShaderStageFlags flags,
                                  uint32_t size, const void* data) {
  vkCmdPushConstants(commandBuffer, vkPipelineLayout_, flags, 0, size, data);
}

void Pipeline::bind(VkCommandBuffer commandBuffer) {
  vkCmdBindPipeline(commandBuffer, bindPoint_, vkPipeline_);

  updateDescriptorSets();
}

void Pipeline::allocateDescriptors(const std::vector<SetAndCount>& setAndCount) {
  if (vkDescriptorPool_ == VK_NULL_HANDLE) {
    initDescriptorPool();
  }

  for (auto set : setAndCount) {
    ASSERT(descriptorSets_.contains(set.set_),
           "This pipeline doesn't have a set with index " + std::to_string(set.set_));

    const VkDescriptorSetAllocateInfo allocInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = vkDescriptorPool_,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptorSets_[set.set_].vkLayout_,
    };

    for (size_t i = 0; i < set.count_; ++i) {
      VkDescriptorSet descriptorSet{VK_NULL_HANDLE};
      VK_CHECK(vkAllocateDescriptorSets(context_->device(), &allocInfo, &descriptorSet));
      descriptorSets_[set.set_].vkSets_.push_back(descriptorSet);

      context_->setVkObjectname(descriptorSet, VK_OBJECT_TYPE_DESCRIPTOR_SET,
                                "Descriptor set: " + set.name_ + " " + std::to_string(i));
    }
  }
}

void Pipeline::bindDescriptorSets(VkCommandBuffer commandBuffer,
                                  const std::vector<SetAndBindingIndex>& sets) {
  for (const auto& set : sets) {
    vkCmdBindDescriptorSets(commandBuffer, bindPoint_, vkPipelineLayout_, set.set, 1u,
                            &descriptorSets_[set.set].vkSets_[set.bindIdx], 0, nullptr);
  }
}

void Pipeline::updateSamplersDescriptorSets(uint32_t set, uint32_t index,
                                            const std::vector<SetBindings>& bindings) {
  ASSERT(!bindings.empty(), "bindings are empty");
  std::vector<std::vector<VkDescriptorImageInfo>> samplerInfo(bindings.size());

  std::vector<VkWriteDescriptorSet> writeDescSets;
  writeDescSets.reserve(bindings.size());

  for (size_t idx = 0; auto& binding : bindings) {
    samplerInfo[idx].reserve(binding.samplers_.size());
    for (const auto& sampler : binding.samplers_) {
      samplerInfo[idx].emplace_back(VkDescriptorImageInfo{
          .sampler = sampler->vkSampler(),
      });
    }
    const VkWriteDescriptorSet writeDescSet = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSets_[set].vkSets_[index],
        .dstBinding = binding.binding_,
        .dstArrayElement = 0,
        .descriptorCount = static_cast<uint32_t>(samplerInfo[idx].size()),
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .pImageInfo = samplerInfo[idx].data(),
        .pBufferInfo = nullptr,
    };

    writeDescSets.emplace_back(std::move(writeDescSet));
    ++idx;
  }

  vkUpdateDescriptorSets(context_->device(), writeDescSets.size(), writeDescSets.data(),
                         0, nullptr);
}

void Pipeline::updateTexturesDescriptorSets(uint32_t set, uint32_t index,
                                            const std::vector<SetBindings>& bindings) {
  ASSERT(!bindings.empty(), "bindings are empty");
  std::vector<std::vector<VkDescriptorImageInfo>> imageInfo(bindings.size());

  std::vector<VkWriteDescriptorSet> writeDescSets;
  writeDescSets.reserve(bindings.size());

  for (size_t idx = 0; auto& binding : bindings) {
    imageInfo[idx].reserve(binding.textures_.size());
    for (const auto& texture : binding.textures_) {
      imageInfo[idx].emplace_back(VkDescriptorImageInfo{
          .imageView = texture->vkImageView(),
          .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      });
    }
    const VkWriteDescriptorSet writeDescSet = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSets_[set].vkSets_[index],
        .dstBinding = binding.binding_,
        .dstArrayElement = 0,
        .descriptorCount = static_cast<uint32_t>(imageInfo[idx].size()),
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
        .pImageInfo = imageInfo[idx].data(),
        .pBufferInfo = nullptr,
    };

    writeDescSets.emplace_back(std::move(writeDescSet));
    ++idx;
  }

  vkUpdateDescriptorSets(context_->device(), writeDescSets.size(), writeDescSets.data(),
                         0, nullptr);
}

void Pipeline::updateBuffersDescriptorSets(uint32_t set, uint32_t index,
                                           VkDescriptorType type,
                                           const std::vector<SetBindings>& bindings) {
  ASSERT(!bindings.empty(), "bindings are empty");
  std::vector<VkDescriptorBufferInfo> bufferInfo;
  bufferInfo.reserve(bindings.size());
  std::vector<VkWriteDescriptorSet> writeDescSets;
  writeDescSets.reserve(bindings.size());

  for (auto& binding : bindings) {
    bufferInfo.emplace_back(VkDescriptorBufferInfo{
        .buffer = binding.buffer->vkBuffer(), .offset = 0, .range = binding.bufferBytes});

    const VkWriteDescriptorSet writeDescSet = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptorSets_[set].vkSets_[index],
        .dstBinding = binding.binding_,
        .dstArrayElement = 0,
        .descriptorCount = 1,
        .descriptorType = type,
        .pImageInfo = nullptr,
        .pBufferInfo = &bufferInfo.back(),
    };

    writeDescSets.emplace_back(writeDescSet);
  }

  vkUpdateDescriptorSets(context_->device(), writeDescSets.size(), writeDescSets.data(),
                         0, nullptr);
}

void Pipeline::updateDescriptorSets() {
  if (!writeDescSets_.empty()) {
    std::unique_lock<std::mutex> mlock(mutex_);
    vkUpdateDescriptorSets(context_->device(), writeDescSets_.size(),
                           writeDescSets_.data(), 0, nullptr);
    writeDescSets_.clear();
    bufferInfo_.clear();

    bufferViewInfo_.clear();
    imageInfo_.clear();
    accelerationStructInfo_.clear();
  }
}

void Pipeline::bindResource(uint32_t set, uint32_t binding, uint32_t index,
                            std::shared_ptr<Buffer> buffer, uint32_t offset,
                            uint32_t size, VkDescriptorType type, VkFormat format) {
  bufferInfo_.emplace_back(std::vector<VkDescriptorBufferInfo>{VkDescriptorBufferInfo{
      .buffer = buffer->vkBuffer(), .offset = offset, .range = size}});

  if (type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
      type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER) {
    ASSERT(format != VK_FORMAT_UNDEFINED, "format must be specified");
    bufferViewInfo_.emplace_back(buffer->requestBufferView(format));
  }

  ASSERT(descriptorSets_[set].vkSets_[index] != VK_NULL_HANDLE,
         "Did you allocate the descriptor set before binding to it?");

  const VkWriteDescriptorSet writeDescSet = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptorSets_[set].vkSets_[index],
      .dstBinding = binding,
      .dstArrayElement = 0,
      .descriptorCount = 1,
      .descriptorType = type,
      .pImageInfo = nullptr,
      .pBufferInfo = (type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
                      type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
                         ? VK_NULL_HANDLE
                         : bufferInfo_.back().data(),
      .pTexelBufferView = (type == VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER ||
                           type == VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER)
                              ? &bufferViewInfo_.back()
                              : VK_NULL_HANDLE,
  };

  writeDescSets_.emplace_back(std::move(writeDescSet));
}

void Pipeline::bindResource(uint32_t set, uint32_t binding, uint32_t index,
                            std::span<std::shared_ptr<Texture>> textures,
                            std::shared_ptr<Sampler> sampler, uint32_t dstArrayElement) {
  if (textures.size() == 0) {
    return;
  }

  std::unique_lock<std::mutex> mlock(mutex_);

  imageInfo_.push_back(std::vector<VkDescriptorImageInfo>());
  imageInfo_.back().reserve(textures.size());
  for (const auto& texture : textures) {
    if (texture) {
      imageInfo_.back().emplace_back(VkDescriptorImageInfo{
          .sampler = sampler ? sampler->vkSampler() : VK_NULL_HANDLE,
          .imageView = texture->vkImageView(),
          .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
      });
    }
  }

  if (imageInfo_.back().size() == 0) {
    return;
  }

  ASSERT(descriptorSets_[set].vkSets_[index] != VK_NULL_HANDLE,
         "Did you allocate the descriptor set before binding to it?");

  const VkWriteDescriptorSet writeDescSet = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptorSets_[set].vkSets_[index],
      .dstBinding = binding,
      .dstArrayElement = dstArrayElement,
      .descriptorCount = static_cast<uint32_t>(imageInfo_.back().size()),
      .descriptorType = sampler ? VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER
                                : VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,
      .pImageInfo = imageInfo_.back().data(),
      .pBufferInfo = nullptr,
  };

  writeDescSets_.emplace_back(std::move(writeDescSet));
}

void Pipeline::bindResource(uint32_t set, uint32_t binding, uint32_t index,
                            std::span<std::shared_ptr<Sampler>> samplers) {
  imageInfo_.push_back(std::vector<VkDescriptorImageInfo>());
  imageInfo_.back().reserve(samplers.size());
  for (const auto& sampler : samplers) {
    imageInfo_.back().emplace_back(VkDescriptorImageInfo{
        .sampler = sampler->vkSampler(),
    });
  }

  ASSERT(descriptorSets_[set].vkSets_[index] != VK_NULL_HANDLE,
         "Did you allocate the descriptor set before binding to it?");

  const VkWriteDescriptorSet writeDescSet = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptorSets_[set].vkSets_[index],
      .dstBinding = binding,
      .dstArrayElement = 0,
      .descriptorCount = static_cast<uint32_t>(imageInfo_.back().size()),
      .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
      .pImageInfo = imageInfo_.back().data(),
      .pBufferInfo = nullptr,
  };

  writeDescSets_.emplace_back(writeDescSet);
}

void Pipeline::bindResource(uint32_t set, uint32_t binding, uint32_t index,
                            std::vector<std::shared_ptr<Buffer>> buffers,
                            VkDescriptorType type) {
  std::vector<VkDescriptorBufferInfo> bufferInfos;

  for (auto& buffer : buffers) {
    bufferInfos.emplace_back(VkDescriptorBufferInfo{
        .buffer = buffer->vkBuffer(),
        .offset = 0,
        .range = buffer->size(),
    });
  }

  bufferInfo_.emplace_back(bufferInfos);

  ASSERT(descriptorSets_[set].vkSets_[index] != VK_NULL_HANDLE,
         "Did you allocate the descriptor set before binding to it?");

  const VkWriteDescriptorSet writeDescSet = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptorSets_[set].vkSets_[index],
      .dstBinding = binding,
      .dstArrayElement = 0,
      .descriptorCount = uint32_t(bufferInfos.size()),
      .descriptorType = type,
      .pImageInfo = nullptr,
      .pBufferInfo = bufferInfo_.back().data(),
  };

  writeDescSets_.emplace_back(std::move(writeDescSet));
}

void Pipeline::bindResource(uint32_t set, uint32_t binding, uint32_t index,
                            std::shared_ptr<Texture> texture, VkDescriptorType type) {
  imageInfo_.push_back(std::vector<VkDescriptorImageInfo>());
  imageInfo_.back().push_back(VkDescriptorImageInfo{
      .imageView = texture->vkImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
  });

  ASSERT(descriptorSets_[set].vkSets_[index] != VK_NULL_HANDLE,
         "Did you allocate the descriptor set before binding to it?");

  const VkWriteDescriptorSet writeDescSet = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptorSets_[set].vkSets_[index],
      .dstBinding = binding,
      .dstArrayElement = 0,
      .descriptorCount = static_cast<uint32_t>(imageInfo_.back().size()),
      .descriptorType = type,
      .pImageInfo = imageInfo_.back().data(),
      .pBufferInfo = nullptr,
  };

  writeDescSets_.emplace_back(writeDescSet);
}

void Pipeline::bindResource(uint32_t set, uint32_t binding, uint32_t index,
                            std::span<std::shared_ptr<VkImageView>> imageViews,
                            VkDescriptorType type) {
  imageInfo_.push_back(std::vector<VkDescriptorImageInfo>());
  imageInfo_.back().reserve(imageViews.size());
  for (const auto& imview : imageViews) {
    imageInfo_.back().emplace_back(VkDescriptorImageInfo{
        .imageView = *imview,
        .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
    });
  }

  ASSERT(descriptorSets_[set].vkSets_[index] != VK_NULL_HANDLE,
         "Did you allocate the descriptor set before binding to it?");

  const VkWriteDescriptorSet writeDescSet = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptorSets_[set].vkSets_[index],
      .dstBinding = binding,
      .dstArrayElement = 0,
      .descriptorCount = static_cast<uint32_t>(imageInfo_.back().size()),
      .descriptorType = type,
      .pImageInfo = imageInfo_.back().data(),
      .pBufferInfo = nullptr,
  };

  writeDescSets_.emplace_back(writeDescSet);
}

void Pipeline::bindResource(uint32_t set, uint32_t binding, uint32_t index,
                            std::shared_ptr<Texture> texture,
                            std::shared_ptr<Sampler> sampler, VkDescriptorType type) {
  imageInfo_.push_back(std::vector<VkDescriptorImageInfo>());
  imageInfo_.back().push_back(VkDescriptorImageInfo{
      .sampler = sampler->vkSampler(),
      .imageView = texture->vkImageView(),
      .imageLayout = VK_IMAGE_LAYOUT_GENERAL,
  });

  ASSERT(descriptorSets_[set].vkSets_[index] != VK_NULL_HANDLE,
         "Did you allocate the descriptor set before binding to it?");

  const VkWriteDescriptorSet writeDescSet = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .dstSet = descriptorSets_[set].vkSets_[index],
      .dstBinding = binding,
      .dstArrayElement = 0,
      .descriptorCount = static_cast<uint32_t>(imageInfo_.back().size()),
      .descriptorType = type,
      .pImageInfo = imageInfo_.back().data(),
      .pBufferInfo = nullptr,
  };

  writeDescSets_.emplace_back(writeDescSet);
}

void Pipeline::bindResource(uint32_t set, uint32_t binding, uint32_t index,
                            VkAccelerationStructureKHR* accelStructHandle) {
  accelerationStructInfo_.push_back(VkWriteDescriptorSetAccelerationStructureKHR{
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET_ACCELERATION_STRUCTURE_KHR,
      .accelerationStructureCount = 1,
      .pAccelerationStructures = accelStructHandle,
  });

  ASSERT(descriptorSets_[set].vkSets_[index] != VK_NULL_HANDLE,
         "Did you allocate the descriptor set before binding to it?");

  const VkWriteDescriptorSet writeDescSet = {
      .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
      .pNext = &accelerationStructInfo_.back(),
      .dstSet = descriptorSets_[set].vkSets_[index],
      .dstBinding = binding,
      .dstArrayElement = 0,
      .descriptorCount = 1u,
      .descriptorType = VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR,
  };

  writeDescSets_.emplace_back(writeDescSet);
}

void Pipeline::bindVertexBuffer(VkCommandBuffer commandBuffer, VkBuffer vertexBuffer) {
  VkBuffer vertexBuffers[1] = {vertexBuffer};
  VkDeviceSize vertexOffset[1] = {0};
  vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, vertexOffset);
}

void Pipeline::bindIndexBuffer(VkCommandBuffer commandBuffer, VkBuffer indexBuffer) {
  vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
}

void Pipeline::createGraphicsPipeline() {
  const VkSpecializationInfo vertexSpecializationInfo{
      .mapEntryCount =
          static_cast<uint32_t>(graphicsPipelineDesc_.vertexSpecConstants_.size()),
      .pMapEntries = graphicsPipelineDesc_.vertexSpecConstants_.data(),
      .dataSize = !graphicsPipelineDesc_.vertexSpecConstants_.empty()
                      ? graphicsPipelineDesc_.vertexSpecConstants_.back().offset +
                            graphicsPipelineDesc_.vertexSpecConstants_.back().size
                      : 0,
      .pData = graphicsPipelineDesc_.vertexSpecializationData,
  };

  const VkSpecializationInfo fragmentSpecializationInfo{
      .mapEntryCount =
          static_cast<uint32_t>(graphicsPipelineDesc_.fragmentSpecConstants_.size()),
      .pMapEntries = graphicsPipelineDesc_.fragmentSpecConstants_.data(),
      .dataSize = !graphicsPipelineDesc_.fragmentSpecConstants_.empty()
                      ? graphicsPipelineDesc_.fragmentSpecConstants_.back().offset +
                            graphicsPipelineDesc_.fragmentSpecConstants_.back().size
                      : 0,
      .pData = graphicsPipelineDesc_.fragmentSpecializationData,
  };

  const auto vertShader = graphicsPipelineDesc_.vertexShader_.lock();
  ASSERT(vertShader,
         "Vertex's ShaderModule has been destroyed before being used to create "
         "a pipeline");
  const auto fragShader = graphicsPipelineDesc_.fragmentShader_.lock();
  ASSERT(fragShader,
         "Vertex's ShaderModule has been destroyed before being used to create "
         "a pipeline");
  std::array<VkPipelineShaderStageCreateInfo, 2> shaderStages = {

      VkPipelineShaderStageCreateInfo{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = vertShader->vkShaderStageFlags(),
          .module = vertShader->vkShaderModule(),
          .pName = vertShader->entryPoint().c_str(),
          .pSpecializationInfo = !graphicsPipelineDesc_.vertexSpecConstants_.empty()
                                     ? &vertexSpecializationInfo
                                     : nullptr,
      },
      VkPipelineShaderStageCreateInfo{
          .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
          .stage = fragShader->vkShaderStageFlags(),
          .module = fragShader->vkShaderModule(),
          .pName = fragShader->entryPoint().c_str(),
          .pSpecializationInfo = !graphicsPipelineDesc_.fragmentSpecConstants_.empty()
                                     ? &fragmentSpecializationInfo
                                     : nullptr,
      },
  };

  const VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 0,
      .vertexAttributeDescriptionCount = 0,
  };

  const VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = graphicsPipelineDesc_.primitiveTopology,
      .primitiveRestartEnable = VK_FALSE,
  };

  // viewport & scissor stuff
  const VkViewport viewport = graphicsPipelineDesc_.viewport.toVkViewPort();

  const VkRect2D scissor = {
      .offset =
          {
              .x = 0,
              .y = 0,
          },
      .extent = graphicsPipelineDesc_.viewport.toVkExtents(),
  };

  const VkPipelineViewportStateCreateInfo viewportState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = &viewport,
      .scissorCount = 1,
      .pScissors = &scissor,
  };

  // fixed function stuff
  const VkPipelineRasterizationStateCreateInfo rasterizer = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .cullMode = VkCullModeFlags(graphicsPipelineDesc_.cullMode),
      .frontFace = graphicsPipelineDesc_.frontFace,
      .depthBiasEnable = VK_FALSE,
      .depthBiasConstantFactor = 0.0f,  // Optional
      .depthBiasClamp = 0.0f,           // Optional
      .depthBiasSlopeFactor = 0.0f,     // Optional
      .lineWidth = 1.0f,
  };

  const VkPipelineMultisampleStateCreateInfo multisampling = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .rasterizationSamples = graphicsPipelineDesc_.sampleCount,
      .sampleShadingEnable = VK_FALSE,
      .minSampleShading = 1.0f,           // Optional
      .pSampleMask = nullptr,             // Optional
      .alphaToCoverageEnable = VK_FALSE,  // Optional
      .alphaToOneEnable = VK_FALSE,       // Optional
  };

  std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;

  if (graphicsPipelineDesc_.blendAttachmentStates_.size() > 0) {
    ASSERT(graphicsPipelineDesc_.blendAttachmentStates_.size() ==
               graphicsPipelineDesc_.colorTextureFormats.size(),
           "Blend states need to be provided for all color textures");
    colorBlendAttachments = graphicsPipelineDesc_.blendAttachmentStates_;
  } else {
    colorBlendAttachments = std::vector<VkPipelineColorBlendAttachmentState>(
        graphicsPipelineDesc_.colorTextureFormats.size(),
        {
            .blendEnable = graphicsPipelineDesc_.blendEnable,
            .srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,            // Optional
            .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,  // Optional
            .colorBlendOp = VK_BLEND_OP_ADD,                             // Optional
            .srcAlphaBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,            // Optional
            .dstAlphaBlendFactor = VK_BLEND_FACTOR_DST_ALPHA,            // Optional
            .alphaBlendOp = VK_BLEND_OP_ADD,                             // Optional
            .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
        });
  }

  const VkPipelineColorBlendStateCreateInfo colorBlending = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY,  // Optional
      .attachmentCount = uint32_t(colorBlendAttachments.size()),
      .pAttachments = colorBlendAttachments.data(),
      .blendConstants = {0.0f, 0.0f, 0.0f, 0.0f},  // Optional
  };

  // Descriptor Set
  initDescriptorLayout();

  // End of descriptor set layout
  // TODO: does order matter in descSetLayouts? YES!
  std::vector<VkDescriptorSetLayout> descSetLayouts(descriptorSets_.size());
  for (const auto& set : descriptorSets_) {
    descSetLayouts[set.first] = set.second.vkLayout_;
  }

  vkPipelineLayout_ =
      createPipelineLayout(descSetLayouts, graphicsPipelineDesc_.pushConstants_);

  const VkPipelineDepthStencilStateCreateInfo depthStencilState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
      .depthTestEnable = graphicsPipelineDesc_.depthTestEnable,
      .depthWriteEnable = graphicsPipelineDesc_.depthWriteEnable,
      .depthCompareOp = graphicsPipelineDesc_.depthCompareOperation,
      .depthBoundsTestEnable = VK_FALSE,
      .stencilTestEnable = VK_FALSE,
      .front = {},
      .back = {},
      .minDepthBounds = 0.0f,
      .maxDepthBounds = 1.0f,
  };

  const VkPipelineDynamicStateCreateInfo dynamicState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
      .dynamicStateCount =
          static_cast<uint32_t>(graphicsPipelineDesc_.dynamicStates_.size()),
      .pDynamicStates = graphicsPipelineDesc_.dynamicStates_.data(),
  };

  // only used for dynamic rendering
  const VkPipelineRenderingCreateInfo pipelineRenderingCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
      .colorAttachmentCount = uint32_t(graphicsPipelineDesc_.colorTextureFormats.size()),
      .pColorAttachmentFormats = graphicsPipelineDesc_.colorTextureFormats.data(),
      .depthAttachmentFormat = graphicsPipelineDesc_.depthTextureFormat,
      .stencilAttachmentFormat = graphicsPipelineDesc_.stencilTextureFormat,
  };

  const VkGraphicsPipelineCreateInfo pipelineInfo = {
      .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
      .pNext = graphicsPipelineDesc_.useDynamicRendering_ ? &pipelineRenderingCreateInfo
                                                          : nullptr,
      .stageCount = uint32_t(shaderStages.size()),
      .pStages = shaderStages.data(),
      .pVertexInputState = &graphicsPipelineDesc_.vertexInputCreateInfo,
      .pInputAssemblyState = &inputAssembly,
      .pViewportState = &viewportState,
      .pRasterizationState = &rasterizer,
      .pMultisampleState = &multisampling,
      .pDepthStencilState = &depthStencilState,  // Optional
      .pColorBlendState = &colorBlending,
      .pDynamicState = &dynamicState,
      .layout = vkPipelineLayout_,
      .renderPass = vkRenderPass_,
      .basePipelineHandle = VK_NULL_HANDLE,  // Optional
      .basePipelineIndex = -1,               // Optional
  };

  VK_CHECK(vkCreateGraphicsPipelines(context_->device(), VK_NULL_HANDLE, 1, &pipelineInfo,
                                     nullptr, &vkPipeline_));

  context_->setVkObjectname(vkPipeline_, VK_OBJECT_TYPE_PIPELINE,
                            "Graphics pipeline: " + name_);
}  // namespace VulkanCore

void Pipeline::createComputePipeline() {
  const auto computeShader = computePipelineDesc_.computeShader_.lock();
  ASSERT(computeShader,
         "Compute's ShaderModule has been destroyed before being used to create "
         "a pipeline");

  const VkSpecializationInfo specializationInfo{
      .mapEntryCount =
          static_cast<uint32_t>(computePipelineDesc_.specializationConsts_.size()),
      .pMapEntries = computePipelineDesc_.specializationConsts_.data(),
      .dataSize = !computePipelineDesc_.specializationConsts_.empty()
                      ? computePipelineDesc_.specializationConsts_.back().offset +
                            computePipelineDesc_.specializationConsts_.back().size
                      : 0,
      .pData = computePipelineDesc_.specializationData_,
  };

  initDescriptorLayout();

  std::vector<VkDescriptorSetLayout> descSetLayouts;
  for (const auto& set : descriptorSets_) {
    descSetLayouts.push_back(set.second.vkLayout_);
  }

  vkPipelineLayout_ =
      createPipelineLayout(descSetLayouts, computePipelineDesc_.pushConstants_);

  VkPipelineShaderStageCreateInfo shaderStage{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = computeShader->vkShaderStageFlags(),
      .module = computeShader->vkShaderModule(),
      .pName = computeShader->entryPoint().c_str(),
  };

  VkComputePipelineCreateInfo computePipelineCreateInfo{
      .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
      .flags = 0,
      .stage = shaderStage,
      .layout = vkPipelineLayout_,
  };
  VK_CHECK(vkCreateComputePipelines(context_->device(), VK_NULL_HANDLE, 1,
                                    &computePipelineCreateInfo, VK_NULL_HANDLE,
                                    &vkPipeline_));
  context_->setVkObjectname(vkPipeline_, VK_OBJECT_TYPE_PIPELINE,
                            "Compute pipeline: " + name_);
}

void Pipeline::createRayTracingPipeline() {
  initDescriptorLayout();

  std::vector<VkDescriptorSetLayout> descSetLayouts;
  for (const auto& set : descriptorSets_) {
    descSetLayouts.push_back(set.second.vkLayout_);
  }

  vkPipelineLayout_ =
      createPipelineLayout(descSetLayouts, rayTracingPipelineDesc_.pushConstants_);

  std::vector<VkPipelineShaderStageCreateInfo> shaderStages;

  std::vector<VkRayTracingShaderGroupCreateInfoKHR> shaderGroups;

  const auto rayGenShader = rayTracingPipelineDesc_.rayGenShader_.lock();

  VkPipelineShaderStageCreateInfo rayGenShaderInfo{
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = rayGenShader->vkShaderStageFlags(),
      .module = rayGenShader->vkShaderModule(),
      .pName = rayGenShader->entryPoint().c_str(),
  };

  shaderStages.push_back(rayGenShaderInfo);

  VkRayTracingShaderGroupCreateInfoKHR shaderGroup{
      .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
      .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
      .generalShader = static_cast<uint32_t>(shaderStages.size()) - 1,
      .closestHitShader = VK_SHADER_UNUSED_KHR,
      .anyHitShader = VK_SHADER_UNUSED_KHR,
      .intersectionShader = VK_SHADER_UNUSED_KHR,
  };

  shaderGroups.push_back(shaderGroup);

  for (auto& rayMissShader : rayTracingPipelineDesc_.rayMissShaders_) {
    const auto rayMissShaderPtr = rayMissShader.lock();

    VkPipelineShaderStageCreateInfo rayMissShaderInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = rayMissShaderPtr->vkShaderStageFlags(),
        .module = rayMissShaderPtr->vkShaderModule(),
        .pName = rayMissShaderPtr->entryPoint().c_str(),
    };

    shaderStages.push_back(rayMissShaderInfo);

    VkRayTracingShaderGroupCreateInfoKHR shaderGroup{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_GENERAL_KHR,
        .generalShader = static_cast<uint32_t>(shaderStages.size()) - 1,
        .closestHitShader = VK_SHADER_UNUSED_KHR,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR,
    };

    shaderGroups.push_back(shaderGroup);
  }

  for (auto& rayClosestHitShader : rayTracingPipelineDesc_.rayClosestHitShaders_) {
    const auto rayClosestHitShaderPtr = rayClosestHitShader.lock();

    VkPipelineShaderStageCreateInfo rayClosestHitShaderInfo{
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = rayClosestHitShaderPtr->vkShaderStageFlags(),
        .module = rayClosestHitShaderPtr->vkShaderModule(),
        .pName = rayClosestHitShaderPtr->entryPoint().c_str(),
    };

    shaderStages.push_back(rayClosestHitShaderInfo);

    VkRayTracingShaderGroupCreateInfoKHR shaderGroup{
        .sType = VK_STRUCTURE_TYPE_RAY_TRACING_SHADER_GROUP_CREATE_INFO_KHR,
        .type = VK_RAY_TRACING_SHADER_GROUP_TYPE_TRIANGLES_HIT_GROUP_KHR,
        .generalShader = VK_SHADER_UNUSED_KHR,
        .closestHitShader = static_cast<uint32_t>(shaderStages.size()) - 1,
        .anyHitShader = VK_SHADER_UNUSED_KHR,
        .intersectionShader = VK_SHADER_UNUSED_KHR,
    };

    shaderGroups.push_back(shaderGroup);
  }

  VkRayTracingPipelineCreateInfoKHR rayTracingPipelineInfo{
      .sType = VK_STRUCTURE_TYPE_RAY_TRACING_PIPELINE_CREATE_INFO_KHR,
      .stageCount = static_cast<uint32_t>(shaderStages.size()),
      .pStages = shaderStages.data(),
      .groupCount = static_cast<uint32_t>(shaderGroups.size()),
      .pGroups = shaderGroups.data(),
      .maxPipelineRayRecursionDepth = 10,
      .layout = vkPipelineLayout_,
  };
  VK_CHECK(vkCreateRayTracingPipelinesKHR(context_->device(), VK_NULL_HANDLE,
                                          VK_NULL_HANDLE, 1, &rayTracingPipelineInfo,
                                          nullptr, &vkPipeline_));

  context_->setVkObjectname(vkPipeline_, VK_OBJECT_TYPE_PIPELINE,
                            "RayTracing pipeline: " + name_);
}

VkPipelineLayout Pipeline::createPipelineLayout(
    const std::vector<VkDescriptorSetLayout>& descLayouts,
    const std::vector<VkPushConstantRange>& pushConsts) const {
  const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = (uint32_t)descLayouts.size(),
      .pSetLayouts = descLayouts.data(),
      .pushConstantRangeCount =
          !pushConsts.empty() ? static_cast<uint32_t>(pushConsts.size()) : 0,
      .pPushConstantRanges = !pushConsts.empty() ? pushConsts.data() : nullptr,
  };

  VkPipelineLayout pipelineLayout{VK_NULL_HANDLE};
  VK_CHECK(vkCreatePipelineLayout(context_->device(), &pipelineLayoutInfo, nullptr,
                                  &pipelineLayout));
  context_->setVkObjectname(pipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT,
                            "pipeline layout: " + name_);

  return pipelineLayout;
}

void Pipeline::initDescriptorPool() {
  std::vector<SetDescriptor> sets;

  if (bindPoint_ == VK_PIPELINE_BIND_POINT_GRAPHICS) {
    sets = graphicsPipelineDesc_.sets_;
  } else if (bindPoint_ == VK_PIPELINE_BIND_POINT_COMPUTE) {
    sets = computePipelineDesc_.sets_;
  } else if (bindPoint_ == VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR) {
    sets = rayTracingPipelineDesc_.sets_;
  }

  std::vector<VkDescriptorPoolSize> poolSizes;
  for (size_t setIndex = 0; const auto& set : sets) {
    for (const auto& binding : set.bindings_) {
      poolSizes.push_back({binding.descriptorType, MAX_DESCRIPTOR_SETS});
    }
  }

  const VkDescriptorPoolCreateInfo descriptorPoolInfo = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT |
               VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT,
      .maxSets = MAX_DESCRIPTOR_SETS,
      .poolSizeCount = static_cast<uint32_t>(poolSizes.size()),
      .pPoolSizes = poolSizes.data(),
  };
  VK_CHECK(vkCreateDescriptorPool(context_->device(), &descriptorPoolInfo, nullptr,
                                  &vkDescriptorPool_));
  context_->setVkObjectname(vkDescriptorPool_, VK_OBJECT_TYPE_DESCRIPTOR_POOL,
                            "Graphics pipeline descriptor pool: " + name_);
}

void Pipeline::initDescriptorLayout() {
  std::vector<SetDescriptor> sets;

  if (bindPoint_ == VK_PIPELINE_BIND_POINT_GRAPHICS) {
    sets = graphicsPipelineDesc_.sets_;
  } else if (bindPoint_ == VK_PIPELINE_BIND_POINT_COMPUTE) {
    sets = computePipelineDesc_.sets_;
  } else if (bindPoint_ == VK_PIPELINE_BIND_POINT_RAY_TRACING_KHR) {
    sets = rayTracingPipelineDesc_.sets_;
  }
  constexpr VkDescriptorBindingFlags flagsToEnable =
      VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
      VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
  /* | VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT; */  // This is disabled
                                                        // because the feature
                                                        // is disabled. See
                                                        // Context::createDefaultFeatureChain
  for (size_t setIndex = 0; const auto& set : sets) {
    std::vector<VkDescriptorBindingFlags> bindFlags(set.bindings_.size(), flagsToEnable);
    /* this won't work for android */
    const VkDescriptorSetLayoutBindingFlagsCreateInfo extendedInfo{
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .pNext = nullptr,
        .bindingCount = static_cast<uint32_t>(set.bindings_.size()),
        .pBindingFlags = bindFlags.data(),
    };
    /* end of not working for android */

    const VkDescriptorSetLayoutCreateInfo dslci = {
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
    /* the next two lines won't work for android */
#if defined(_WIN32)
      .pNext = &extendedInfo,
      .flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT_EXT,
#endif
      /* end of not working for android*/
      .bindingCount = static_cast<uint32_t>(set.bindings_.size()),
      .pBindings = !set.bindings_.empty() ? set.bindings_.data() : nullptr,
    };

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VK_CHECK(vkCreateDescriptorSetLayout(context_->device(), &dslci, nullptr,
                                         &descriptorSetLayout));
    context_->setVkObjectname(descriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
                              "Graphics pipeline descriptor set " +
                                  std::to_string(setIndex++) + " layout: " + name_);
    descriptorSets_[set.set_].vkLayout_ = descriptorSetLayout;
  }
}

}  // namespace VulkanCore
