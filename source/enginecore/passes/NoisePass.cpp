#include "NoisePass.hpp"

#include <filesystem>

#include "../thirdparty/samplerCPP/samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp.cpp"

constexpr uint32_t NOISE_SET = 0;
constexpr uint32_t BINDING_OUT_NOISE_TEXTURE = 0;
constexpr uint32_t BINDING_SOBOL_BUFFER = 1;
constexpr uint32_t BINDING_RANKING_TILE_BUFFER = 2;
constexpr uint32_t BINDING_SCRAMBLING_TILE_BUFFER = 3;

struct NoisePushConst {
  uint32_t frameIndex;
};

NoisePass::NoisePass() {}

void NoisePass::init(VulkanCore::Context* context) {
  context_ = context;

  outNoiseTexture_ =
      context_->createTexture(VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8_UNORM, 0,
                              VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
                              VkExtent3D{
                                  .width = 128u,
                                  .height = 128u,
                                  .depth = 1u,
                              },
                              1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

  sobolBuffer_ = context_->createBuffer(
      sizeof(sobol_256spp_256d),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, "sobolBuffer for noise");

  rankingTile_ = context_->createBuffer(
      sizeof(rankingTile),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, "rankingTile for noise");

  scramblingTileBuffer_ = context_->createBuffer(
      sizeof(scramblingTile),
      VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
      VMA_MEMORY_USAGE_GPU_ONLY, "scramblingTile for noise");

  const auto resourcesFolder = std::filesystem::current_path() / "resources/shaders/";

  auto shader = context->createShaderModule((resourcesFolder / "noisegen.comp").string(),
                                            VK_SHADER_STAGE_COMPUTE_BIT, "noise compute");

  const std::vector<VulkanCore::Pipeline::SetDescriptor> setLayout = {
      {
          .set_ = NOISE_SET,
          .bindings_ =
              {
                  VkDescriptorSetLayoutBinding{BINDING_OUT_NOISE_TEXTURE,
                                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                                               VK_SHADER_STAGE_COMPUTE_BIT},
                  VkDescriptorSetLayoutBinding{BINDING_SOBOL_BUFFER,
                                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                               VK_SHADER_STAGE_COMPUTE_BIT},
                  VkDescriptorSetLayoutBinding{BINDING_RANKING_TILE_BUFFER,
                                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                               VK_SHADER_STAGE_COMPUTE_BIT},
                  VkDescriptorSetLayoutBinding{BINDING_SCRAMBLING_TILE_BUFFER,
                                               VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1,
                                               VK_SHADER_STAGE_COMPUTE_BIT},
              },
      },
  };
  std::vector<VkPushConstantRange> pushConstants = {
      VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          .offset = 0,
          .size = sizeof(NoisePushConst),
      },
  };
  const VulkanCore::Pipeline::ComputePipelineDescriptor desc = {
      .sets_ = setLayout,
      .computeShader_ = shader,
      .pushConstants_ = pushConstants,
  };
  pipeline_ = context->createComputePipeline(desc, "main");

  pipeline_->allocateDescriptors({
      {.set_ = NOISE_SET, .count_ = 1},
  });

  pipeline_->bindResource(NOISE_SET, BINDING_OUT_NOISE_TEXTURE, 0, outNoiseTexture_,
                          VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

  pipeline_->bindResource(NOISE_SET, BINDING_SOBOL_BUFFER, 0, sobolBuffer_, 0,
                          sobolBuffer_->size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

  pipeline_->bindResource(NOISE_SET, BINDING_RANKING_TILE_BUFFER, 0, rankingTile_, 0,
                          rankingTile_->size(), VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);

  pipeline_->bindResource(NOISE_SET, BINDING_SCRAMBLING_TILE_BUFFER, 0,
                          scramblingTileBuffer_, 0, scramblingTileBuffer_->size(),
                          VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
}

void NoisePass::upload(VulkanCore::CommandQueueManager& commandMgr) {
  const auto commandBuffer = commandMgr.getCmdBufferToBegin();

  context_->uploadToGPUBuffer(commandMgr, commandBuffer, sobolBuffer_.get(),
                              reinterpret_cast<const void*>(sobol_256spp_256d),
                              sizeof(sobol_256spp_256d));

  context_->uploadToGPUBuffer(commandMgr, commandBuffer, rankingTile_.get(),
                              reinterpret_cast<const void*>(rankingTile),
                              sizeof(rankingTile));

  context_->uploadToGPUBuffer(commandMgr, commandBuffer, scramblingTileBuffer_.get(),
                              reinterpret_cast<const void*>(scramblingTile),
                              sizeof(scramblingTile));

  commandMgr.endCmdBuffer(commandBuffer);

  VkPipelineStageFlags flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
  const auto submitInfo =
      context_->swapchain()->createSubmitInfo(&commandBuffer, &flags, false, false);
  commandMgr.submit(&submitInfo);
  commandMgr.waitUntilSubmitIsComplete();
}

void NoisePass::generateNoise(VkCommandBuffer cmd) {
  context_->beginDebugUtilsLabel(cmd, "Noise texture gen", {1.0f, 0.5f, 0.0f, 1.0f});

  if (index_ == std::numeric_limits<uint32_t>::max()) {
    index_ = 0;
  }

  NoisePushConst pushConst{
      .frameIndex = index_,
  };

  pipeline_->bind(cmd);

  pipeline_->updatePushConstant(cmd, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(NoisePushConst),
                                &pushConst);

  pipeline_->bindDescriptorSets(
      cmd, {
               {.set = NOISE_SET, .bindIdx = BINDING_OUT_NOISE_TEXTURE},
           });
  pipeline_->updateDescriptorSets();

  outNoiseTexture_->transitionImageLayout(cmd, VK_IMAGE_LAYOUT_GENERAL);

  vkCmdDispatch(cmd, 128 / 16 + 1, 128 / 16 + 1, 1);

  outNoiseTexture_->transitionImageLayout(cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  index_++;

  context_->endDebugUtilsLabel(cmd);
}
