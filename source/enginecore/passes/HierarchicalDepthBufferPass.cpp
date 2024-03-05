#include "HierarchicalDepthBufferPass.hpp"

#include <filesystem>

constexpr uint32_t HIERARCHICALDEPTH_SET = 0;
constexpr uint32_t BINDING_OUT_HIERARCHICAL_DEPTH_TEXTURE = 0;
constexpr uint32_t BINDING_DEPTH_TEXTURE = 1;
constexpr uint32_t BINDING_PREV_HIERARCHICAL_DEPTH_TEXTURE = 2;

struct HierarchicalDepthPushConst {
  glm::uvec2 currentMipDimensions;
  glm::uvec2 prevMipDimensions;
  int32_t mipLevelIndex;
};

HierarchicalDepthBufferPass::HierarchicalDepthBufferPass() {}

HierarchicalDepthBufferPass::~HierarchicalDepthBufferPass() {
  for (auto& imageView : hierarchicalDepthTexturePerMipImageViews_) {
    vkDestroyImageView(context_->device(), *imageView.get(), nullptr);
  }
}

void HierarchicalDepthBufferPass::init(
    VulkanCore::Context* context, std::shared_ptr<VulkanCore::Texture> depthTexture) {
  context_ = context;
  depthTexture_ = depthTexture;

  sampler_ = context_->createSampler(
      VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      100.0f, "default sampler");

  outHierarchicalDepthTexture_ = context_->createTexture(
      VK_IMAGE_TYPE_2D, VK_FORMAT_R32G32_SFLOAT, 0,
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
      VkExtent3D{
          .width = context_->swapchain()->extent().width,
          .height = context_->swapchain()->extent().height,
          .depth = 1u,
      },
                              1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true,
                              VK_SAMPLE_COUNT_1_BIT, "Hierarchical DepthTexture");

  const auto resourcesFolder = std::filesystem::current_path() / "resources/shaders/";

  auto shader = context->createShaderModule(
      (resourcesFolder / "hierarchicaldepthgen.comp").string(),
      VK_SHADER_STAGE_COMPUTE_BIT, "hierarchical depth compute shader");

  const std::vector<VulkanCore::Pipeline::SetDescriptor> setLayout = {
      {
          .set_ = HIERARCHICALDEPTH_SET,
          .bindings_ =
              {
                  VkDescriptorSetLayoutBinding{
                      BINDING_OUT_HIERARCHICAL_DEPTH_TEXTURE,
                      VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
                      outHierarchicalDepthTexture_->numMipLevels(),
                      VK_SHADER_STAGE_COMPUTE_BIT},
                  VkDescriptorSetLayoutBinding{BINDING_DEPTH_TEXTURE,
                                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               1, VK_SHADER_STAGE_COMPUTE_BIT},
                  VkDescriptorSetLayoutBinding{BINDING_PREV_HIERARCHICAL_DEPTH_TEXTURE,
                                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               1, VK_SHADER_STAGE_COMPUTE_BIT},
              },
      },
  };
  std::vector<VkPushConstantRange> pushConstants = {
      VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          .offset = 0,
          .size = sizeof(HierarchicalDepthPushConst),
      },
  };

  const VkSpecializationMapEntry specializationMap = {
      .constantID = 0, .offset = 0, .size = sizeof(int32_t)};

  int32_t numMips = outHierarchicalDepthTexture_->numMipLevels();

  const VulkanCore::Pipeline::ComputePipelineDescriptor desc = {
      .sets_ = setLayout,
      .computeShader_ = shader,
      .pushConstants_ = pushConstants,
      .specializationConsts_ = {specializationMap},
      .specializationData_ = &numMips,
  };
  pipeline_ = context->createComputePipeline(desc, "main");

  pipeline_->allocateDescriptors({
      {.set_ = HIERARCHICALDEPTH_SET, .count_ = 1},
  });

  hierarchicalDepthTexturePerMipImageViews_ =
      outHierarchicalDepthTexture_->generateViewForEachMips();

  pipeline_->bindResource(HIERARCHICALDEPTH_SET, BINDING_OUT_HIERARCHICAL_DEPTH_TEXTURE,
                          0,
                          std::span<std::shared_ptr<VkImageView>>(hierarchicalDepthTexturePerMipImageViews_),
                          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

  pipeline_->bindResource(HIERARCHICALDEPTH_SET, BINDING_DEPTH_TEXTURE, 0, depthTexture_,
                          sampler_);

  pipeline_->bindResource(HIERARCHICALDEPTH_SET, BINDING_PREV_HIERARCHICAL_DEPTH_TEXTURE,
                          0, outHierarchicalDepthTexture_, sampler_);
}

void HierarchicalDepthBufferPass::generateHierarchicalDepthBuffer(VkCommandBuffer cmd) {
  context_->beginDebugUtilsLabel(cmd, "HierarchicalDepth texture gen",
                                 {0.5f, 0.5f, 0.0f, 1.0f});

  pipeline_->bind(cmd);

  pipeline_->bindDescriptorSets(cmd,
                                {
                                    {.set = HIERARCHICALDEPTH_SET,
                                     .bindIdx = BINDING_OUT_HIERARCHICAL_DEPTH_TEXTURE},
                                });
  pipeline_->updateDescriptorSets();

  outHierarchicalDepthTexture_->transitionImageLayout(cmd, VK_IMAGE_LAYOUT_GENERAL);

  glm::uvec2 currentMipDim(outHierarchicalDepthTexture_->vkExtents().width,
                           outHierarchicalDepthTexture_->vkExtents().height);

  auto prevMipDim = currentMipDim;

  for (int i = 0; i < outHierarchicalDepthTexture_->numMipLevels(); ++i) {
    HierarchicalDepthPushConst pushConst{
        .currentMipDimensions = currentMipDim,
        .prevMipDimensions = prevMipDim,
        .mipLevelIndex = i,
    };

    if (i != 0) {
      prevMipDim /= 2;
      prevMipDim.x = std::max(prevMipDim.x, 1u);
      prevMipDim.y = std::max(prevMipDim.y, 1u);
    }

    currentMipDim /= 2;
    currentMipDim.x = std::max(currentMipDim.x, 1u);
    currentMipDim.y = std::max(currentMipDim.y, 1u);

    pipeline_->updatePushConstant(cmd, VK_SHADER_STAGE_COMPUTE_BIT,
                                  sizeof(HierarchicalDepthPushConst), &pushConst);

    vkCmdDispatch(cmd, pushConst.currentMipDimensions.x / 16 + 1,
                  pushConst.currentMipDimensions.y / 16 + 1, 1);

    if (i != outHierarchicalDepthTexture_->numMipLevels() - 1) {
      // insert barrier since we need to access previous mip

      VkImageMemoryBarrier barrier{
          .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
          .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
          .dstAccessMask = VK_ACCESS_SHADER_READ_BIT,
          .oldLayout = VK_IMAGE_LAYOUT_GENERAL,
          .newLayout = VK_IMAGE_LAYOUT_GENERAL,
          .image = outHierarchicalDepthTexture_->vkImage(),
          .subresourceRange =
              {
                  .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                  .baseMipLevel = uint32_t(i),  // barrier on mip level i
                  .levelCount = 1,
                  .baseArrayLayer = 0,
                  .layerCount = 1,
              },
      };

      vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0,
                           nullptr, 1, &barrier);
    }
  }

  outHierarchicalDepthTexture_->transitionImageLayout(
      cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  context_->endDebugUtilsLabel(cmd);
}
