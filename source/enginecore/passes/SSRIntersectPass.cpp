#include "SSRIntersectPass.hpp"

#include <filesystem>

constexpr uint32_t SSR_INTERSECT_OUTPUT_SET = 0;
constexpr uint32_t BINDING_OUT_SSR_INTERSECT = 0;

constexpr uint32_t INPUT_TEXTURES_SET = 1;
constexpr uint32_t BINDING_GBUFFER_WORLDNORMAL = 0;
constexpr uint32_t BINDING_GBUFFER_SPECULAR = 1;
constexpr uint32_t BINDING_GBUFFER_BASECOLOR = 2;
constexpr uint32_t BINDING_HIERARCHICALDEPTH = 3;
constexpr uint32_t BINDING_NOISE = 4;

constexpr uint32_t INPUT_CAMERA_SET = 2;
constexpr uint32_t BINDING_CAMERA_TRANSFORM = 0;

struct Transforms {
  glm::aligned_mat4 model;
  glm::aligned_mat4 view;
  glm::aligned_mat4 projection;
  glm::aligned_mat4 projectionInv;
  glm::aligned_mat4 viewInv;
};

struct PushConst {
  glm::uvec2 resolution;
  uint32_t frameIndex;
};

SSRIntersectPass::SSRIntersectPass() {}

SSRIntersectPass::~SSRIntersectPass() {}

void SSRIntersectPass::init(VulkanCore::Context* context, EngineCore::Camera* camera,
                            std::shared_ptr<VulkanCore::Texture> gBufferNormal,
                            std::shared_ptr<VulkanCore::Texture> gBufferSpecular,
                            std::shared_ptr<VulkanCore::Texture> gBufferBaseColor,
                            std::shared_ptr<VulkanCore::Texture> hierarchicalDepth,
                            std::shared_ptr<VulkanCore::Texture> noiseTexture) {
  context_ = context;
  camera_ = camera;
  gBufferNormal_ = gBufferNormal;
  gBufferSpecular_ = gBufferSpecular;
  gBufferBaseColor_ = gBufferBaseColor;
  hierarchicalDepth_ = hierarchicalDepth;
  noiseTexture_ = noiseTexture;

  sampler_ = context_->createSampler(
      VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
      100.0f, "default sampler");

  outSSRIntersectTexture_ = context_->createTexture(
      VK_IMAGE_TYPE_2D, VK_FORMAT_R16G16B16A16_SFLOAT, 0,
      VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT,
      VkExtent3D{
          .width = context_->swapchain()->extent().width,
          .height = context_->swapchain()->extent().height,
          .depth = 1u,
      },
                              1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true,
                              VK_SAMPLE_COUNT_1_BIT, "SSR IntersectTexture");

  cameraBuffer_ = context_->createPersistentBuffer(sizeof(Transforms),
                                                   VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                   "SSR Camera Uniform buffer");

  const auto resourcesFolder = std::filesystem::current_path() / "resources/shaders/";

  auto shader = context->createShaderModule((resourcesFolder / "ssr.comp").string(),
                                            VK_SHADER_STAGE_COMPUTE_BIT,
                                            "SSR Intersect compute shader");

  const std::vector<VulkanCore::Pipeline::SetDescriptor> setLayout = {
      {
          .set_ = SSR_INTERSECT_OUTPUT_SET,
          .bindings_ =
              {
                  VkDescriptorSetLayoutBinding{BINDING_OUT_SSR_INTERSECT,
                                               VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1,
                                               VK_SHADER_STAGE_COMPUTE_BIT},
              },
      },
      {
          .set_ = INPUT_TEXTURES_SET,
          .bindings_ =
              {
                  VkDescriptorSetLayoutBinding{BINDING_GBUFFER_WORLDNORMAL,
                                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               1, VK_SHADER_STAGE_COMPUTE_BIT},
                  VkDescriptorSetLayoutBinding{BINDING_GBUFFER_SPECULAR,
                                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               1, VK_SHADER_STAGE_COMPUTE_BIT},
                  VkDescriptorSetLayoutBinding{BINDING_GBUFFER_BASECOLOR,
                                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               1, VK_SHADER_STAGE_COMPUTE_BIT},
                  VkDescriptorSetLayoutBinding{BINDING_HIERARCHICALDEPTH,
                                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               1, VK_SHADER_STAGE_COMPUTE_BIT},
                  VkDescriptorSetLayoutBinding{BINDING_NOISE,
                                               VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                                               1, VK_SHADER_STAGE_COMPUTE_BIT},
              },
      },
      {
          .set_ = INPUT_CAMERA_SET,
          .bindings_ =
              {
                  VkDescriptorSetLayoutBinding{BINDING_CAMERA_TRANSFORM,
                                               VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                               VK_SHADER_STAGE_COMPUTE_BIT},
              },
      },
  };
  std::vector<VkPushConstantRange> pushConstants = {
      VkPushConstantRange{
          .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
          .offset = 0,
          .size = sizeof(PushConst),
      },
  };

  const VulkanCore::Pipeline::ComputePipelineDescriptor desc = {
      .sets_ = setLayout,
      .computeShader_ = shader,
      .pushConstants_ = pushConstants,
  };
  pipeline_ = context->createComputePipeline(desc, "main");

  pipeline_->allocateDescriptors({
      {.set_ = SSR_INTERSECT_OUTPUT_SET, .count_ = 1},
      {.set_ = INPUT_TEXTURES_SET, .count_ = 1},
      {.set_ = INPUT_CAMERA_SET, .count_ = 1},
  });

  pipeline_->bindResource(SSR_INTERSECT_OUTPUT_SET, BINDING_OUT_SSR_INTERSECT, 0,
                          outSSRIntersectTexture_, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);

  pipeline_->bindResource(INPUT_TEXTURES_SET, BINDING_GBUFFER_WORLDNORMAL, 0,
                          gBufferNormal_, sampler_);

  pipeline_->bindResource(INPUT_TEXTURES_SET, BINDING_GBUFFER_SPECULAR, 0,
                          gBufferSpecular_, sampler_);

  pipeline_->bindResource(INPUT_TEXTURES_SET, BINDING_GBUFFER_BASECOLOR, 0,
                          gBufferBaseColor_, sampler_);

  pipeline_->bindResource(INPUT_TEXTURES_SET, BINDING_HIERARCHICALDEPTH, 0,
                          hierarchicalDepth_, sampler_);

  pipeline_->bindResource(INPUT_TEXTURES_SET, BINDING_NOISE, 0, noiseTexture_, sampler_);

  pipeline_->bindResource(INPUT_CAMERA_SET, BINDING_CAMERA_TRANSFORM, 0, cameraBuffer_, 0,
                          sizeof(Transforms), VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
}

void SSRIntersectPass::run(VkCommandBuffer cmd) {
  if (index_ == std::numeric_limits<uint32_t>::max()) {
    index_ = 0;
  }

  Transforms transform;
  transform.model = glm::mat4(1.0f);
  transform.view = camera_->viewMatrix();
  transform.projection = camera_->getProjectMatrix();
  transform.viewInv = glm::inverse(camera_->viewMatrix());
  transform.projectionInv = glm::inverse(camera_->getProjectMatrix());
  cameraBuffer_->copyDataToBuffer(&transform, sizeof(Transforms));

  context_->beginDebugUtilsLabel(cmd, "SSR Intersection Pass", {0.5f, 0.5f, 0.0f, 1.0f});

  pipeline_->bind(cmd);

  PushConst pushConst{
      .resolution = glm::uvec2(context_->swapchain()->extent().width,
                               context_->swapchain()->extent().height),
      .frameIndex = index_,
  };

  pipeline_->updatePushConstant(cmd, VK_SHADER_STAGE_COMPUTE_BIT, sizeof(PushConst),
                                &pushConst);

  pipeline_->bindDescriptorSets(
      cmd, {
               {.set = SSR_INTERSECT_OUTPUT_SET, .bindIdx = BINDING_OUT_SSR_INTERSECT},
               {.set = INPUT_TEXTURES_SET, .bindIdx = BINDING_GBUFFER_WORLDNORMAL},
               {.set = INPUT_CAMERA_SET, .bindIdx = BINDING_CAMERA_TRANSFORM},
           });
  pipeline_->updateDescriptorSets();

  outSSRIntersectTexture_->transitionImageLayout(cmd, VK_IMAGE_LAYOUT_GENERAL);

  vkCmdDispatch(cmd, pushConst.resolution.x / 16 + 1, pushConst.resolution.y / 16 + 1, 1);

  outSSRIntersectTexture_->transitionImageLayout(
      cmd, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  context_->endDebugUtilsLabel(cmd);
  index_++;
}
