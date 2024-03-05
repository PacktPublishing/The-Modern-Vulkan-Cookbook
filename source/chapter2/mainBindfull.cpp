#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <stb_image.h>

#include <array>
#include <filesystem>
#include <gli/gli.hpp>
#include <glm/glm.hpp>
#include <tracy/Tracy.hpp>

#include "enginecore/Camera.hpp"
#include "enginecore/GLBLoader.hpp"
#include "enginecore/GLFWUtils.hpp"
#include "enginecore/ImguiManager.hpp"
#include "enginecore/Model.hpp"
#include "enginecore/RingBuffer.hpp"
#include "vulkancore/Buffer.hpp"
#include "vulkancore/CommandQueueManager.hpp"
#include "vulkancore/Context.hpp"
#include "vulkancore/DynamicRendering.hpp"
#include "vulkancore/Pipeline.hpp"
#include "vulkancore/RenderPass.hpp"
#include "vulkancore/Sampler.hpp"
#include "vulkancore/Texture.hpp"

// clang-format off
#include <tracy/TracyVulkan.hpp>
// clang-format on

GLFWwindow* window_ = nullptr;
EngineCore::Camera camera(glm::vec3(-9.f, 2.f, 2.f));
int main(int argc, char* argv[]) {
  initWindow(&window_, &camera);

#pragma region Context initialization
  const std::vector<std::string> instExtension = {
      VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
  };

  const std::vector<std::string> deviceExtension = {
#if defined(VK_EXT_calibrated_timestamps)
    VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
#endif
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
  };

  std::vector<std::string> validationLayers;
#ifdef _DEBUG
  validationLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif

  VulkanCore::Context::enableDefaultFeatures();
  VulkanCore::Context::enableBufferDeviceAddressFeature();
  VulkanCore::Context::enableDynamicRenderingFeature();

  VulkanCore::Context context((void*)glfwGetWin32Window(window_),
                              validationLayers,  // layers
                              instExtension,     // instance extensions
                              deviceExtension,   // device extensions
                              0,                 // request a graphics queue only
                              true);
#pragma endregion

#pragma region Swapchain initialization
  const VkExtent2D extents =
      context.physicalDevice().surfaceCapabilities().minImageExtent;

  const VkFormat swapChainFormat = VK_FORMAT_B8G8R8A8_UNORM;

  context.createSwapchain(swapChainFormat, VK_COLORSPACE_SRGB_NONLINEAR_KHR,
                          VK_PRESENT_MODE_MAILBOX_KHR, extents);

  static const uint32_t framesInFlight = (uint32_t)context.swapchain()->numberImages();
#pragma endregion

  // Create command pools
  auto commandMgr = context.createGraphicsCommandQueue(
      context.swapchain()->numberImages(), framesInFlight, "main command");

#pragma region Tracy
#if defined(VK_EXT_calibrated_timestamps)
  TracyVkCtx tracyCtx_ = TracyVkContextCalibrated(
      context.physicalDevice().vkPhysicalDevice(), context.device(),
      context.graphicsQueue(), commandMgr.getCmdBuffer(),
      vkGetPhysicalDeviceCalibrateableTimeDomainsEXT, vkGetCalibratedTimestampsEXT);
#else
  TracyVkCtx tracyCtx_ =
      TracyVkContext(context.physicalDevice().vkPhysicalDevice(), context.device(),
                     context.graphicsQueue(), commandMgr.getCmdBuffer());
#endif
#pragma endregion

  UniformTransforms transform = {.model = glm::mat4(1.0f),
                                 .view = camera.viewMatrix(),
                                 .projection = camera.getProjectMatrix()};

  std::vector<std::shared_ptr<VulkanCore::Buffer>> buffers;
  std::vector<std::shared_ptr<VulkanCore::Texture>> textures;
  std::vector<std::shared_ptr<VulkanCore::Sampler>> samplers;
  EngineCore::RingBuffer cameraBuffer(context.swapchain()->numberImages(), context,
                                      sizeof(UniformTransforms));
  uint32_t numMeshes = 0;
  std::shared_ptr<EngineCore::Model> bistro;

#pragma region Load model
  {
    const auto commandBuffer = commandMgr.getCmdBufferToBegin();
    {
      samplers.emplace_back(context.createSampler(
          VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT,
          VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, 10.0f,
          "default sampler"));

      ZoneScopedN("Model load");
      EngineCore::GLBLoader glbLoader;
      bistro = glbLoader.load("resources/assets/Bistro.glb");
      TracyVkZone(tracyCtx_, commandBuffer, "Model upload");
      EngineCore::convertModel2OneMeshPerBuffer(
          context, commandMgr, commandBuffer, *bistro.get(), buffers, textures, samplers);

      if (textures.size() == 0) {
        textures.push_back(context.createTexture(
            VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, 0,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VkExtent3D{
                .width = static_cast<uint32_t>(1),
                .height = static_cast<uint32_t>(1),
                .depth = static_cast<uint32_t>(1.0),
            },
            1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
            "Empty Texture"));
      }

      numMeshes = buffers.size() >> 1;
    }

    TracyVkCollect(tracyCtx_, commandBuffer);
    commandMgr.endCmdBuffer(commandBuffer);

    const VkPipelineStageFlags flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
    const auto submitInfo =
        context.swapchain()->createSubmitInfo(&commandBuffer, &flags, false, false);
    commandMgr.submit(&submitInfo);
    commandMgr.waitUntilSubmitIsComplete();
  }
#pragma endregion

#pragma region DepthTexture
  auto depthTexture =
      context.createTexture(VK_IMAGE_TYPE_2D, VK_FORMAT_D24_UNORM_S8_UINT, 0,
                            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                            {
                                .width = context.swapchain()->extent().width,
                                .height = context.swapchain()->extent().height,
                                .depth = 1,
                            },
                            1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false,
                            VK_SAMPLE_COUNT_1_BIT, "depth buffer");
#pragma endregion

  const auto resourcesFolder = std::filesystem::current_path() / "resources/shaders/";
  constexpr uint32_t CAMERA_SET = 0;
  constexpr uint32_t TEXTURES_AND_SAMPLER_SET = 1;
  constexpr uint32_t BINDING_0 = 0;
  constexpr uint32_t BINDING_1 = 1;
  const auto vertexShader =
      context.createShaderModule((resourcesFolder / "bindfull.vert").string(),
                                 VK_SHADER_STAGE_VERTEX_BIT, "main vertex");
  const auto fragmentShader =
      context.createShaderModule((resourcesFolder / "bindfull.frag").string(),
                                 VK_SHADER_STAGE_FRAGMENT_BIT, "main fragment");

  const std::vector<VulkanCore::Pipeline::SetDescriptor> setLayout = {
      {
          .set_ = CAMERA_SET,  // set number
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1,
                                               VK_SHADER_STAGE_VERTEX_BIT),
              },
      },
      {
          .set_ = TEXTURES_AND_SAMPLER_SET,  // set
                                             // number
          .bindings_ =
              {
                  // vector of bindings
                  VkDescriptorSetLayoutBinding(0, VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1,
                                               VK_SHADER_STAGE_FRAGMENT_BIT),
                  VkDescriptorSetLayoutBinding(1, VK_DESCRIPTOR_TYPE_SAMPLER, 1,
                                               VK_SHADER_STAGE_FRAGMENT_BIT),
              },
      },
  };

  const VkVertexInputBindingDescription bindingDesc = {
      .binding = 0,
      .stride = sizeof(EngineCore::Vertex),
      .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
  };

  const std::vector<std::pair<VkFormat, size_t>> vertexAttributesFormatAndOffset = {
      {VK_FORMAT_R32G32B32_SFLOAT, offsetof(EngineCore::Vertex, pos)},
      {VK_FORMAT_R32G32B32_SFLOAT, offsetof(EngineCore::Vertex, normal)},
      {VK_FORMAT_R32G32B32A32_SFLOAT, offsetof(EngineCore::Vertex, tangent)},
      {VK_FORMAT_R32G32_SFLOAT, offsetof(EngineCore::Vertex, texCoord)},
      {VK_FORMAT_R32G32_SFLOAT, offsetof(EngineCore::Vertex, texCoord1)},
      {VK_FORMAT_R32_SINT, offsetof(EngineCore::Vertex, material)}};

  std::vector<VkVertexInputAttributeDescription> vertexInputAttributes;

  for (uint32_t i = 0; i < vertexAttributesFormatAndOffset.size(); ++i) {
    auto [format, offset] = vertexAttributesFormatAndOffset[i];
    vertexInputAttributes.push_back(VkVertexInputAttributeDescription{
        .location = i,
        .binding = 0,
        .format = format,
        .offset = uint32_t(offset),
    });
  }

  uint32_t texturesNotPresentSpecializationData = 0;

  const std::vector<VkSpecializationMapEntry> fragSpecializationMapEntries = {{
      .constantID = 0,
      .offset = 0,
      .size = sizeof(texturesNotPresentSpecializationData),
  }};

  const VulkanCore::Pipeline::GraphicsPipelineDescriptor gpDesc = {
      .sets_ = setLayout,
      .vertexShader_ = vertexShader,
      .fragmentShader_ = fragmentShader,
      .dynamicStates_ = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR,
                         VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE},
      .useDynamicRendering_ = true,
      .colorTextureFormats = {swapChainFormat},
      .depthTextureFormat = depthTexture->vkFormat(),
      .sampleCount = VK_SAMPLE_COUNT_1_BIT,
      .cullMode = VK_CULL_MODE_NONE,
      .viewport = context.swapchain()->extent(),
      .depthTestEnable = true,
      .depthWriteEnable = true,
      .depthCompareOperation = VK_COMPARE_OP_LESS,
      .vertexInputCreateInfo =
          {
              .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
              .vertexBindingDescriptionCount = 1u,
              .pVertexBindingDescriptions = &bindingDesc,
              .vertexAttributeDescriptionCount = uint32_t(vertexInputAttributes.size()),
              .pVertexAttributeDescriptions = vertexInputAttributes.data(),
          },
      .fragmentSpecConstants_ = fragSpecializationMapEntries,
      .fragmentSpecializationData = &texturesNotPresentSpecializationData,
  };

#pragma region Pipeline initialization
  auto pipelineWithTexture = context.createGraphicsPipeline(
      gpDesc, VK_NULL_HANDLE, "Pipeline With BaseColorTexture");

  texturesNotPresentSpecializationData = 1;

  auto pipelineWithoutTexture = context.createGraphicsPipeline(
      gpDesc, VK_NULL_HANDLE, "Pipeline Without BaseColorTexture");

  pipelineWithTexture->allocateDescriptors({
      {.set_ = CAMERA_SET, .count_ = 3},
      {.set_ = TEXTURES_AND_SAMPLER_SET, .count_ = uint32_t(textures.size() + 1)},
  });

  pipelineWithoutTexture->allocateDescriptors({
      {.set_ = CAMERA_SET, .count_ = 3},
      {.set_ = TEXTURES_AND_SAMPLER_SET, .count_ = 1},
  });

  pipelineWithTexture->bindResource(CAMERA_SET, BINDING_0, 0, cameraBuffer.buffer(0), 0,
                                    sizeof(UniformTransforms),
                                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  pipelineWithTexture->bindResource(CAMERA_SET, BINDING_0, 1, cameraBuffer.buffer(1), 0,
                                    sizeof(UniformTransforms),
                                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  pipelineWithTexture->bindResource(CAMERA_SET, BINDING_0, 2, cameraBuffer.buffer(2), 0,
                                    sizeof(UniformTransforms),
                                    VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

  pipelineWithoutTexture->bindResource(CAMERA_SET, BINDING_0, 0, cameraBuffer.buffer(0),
                                       0, sizeof(UniformTransforms),
                                       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  pipelineWithoutTexture->bindResource(CAMERA_SET, BINDING_0, 1, cameraBuffer.buffer(1),
                                       0, sizeof(UniformTransforms),
                                       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  pipelineWithoutTexture->bindResource(CAMERA_SET, BINDING_0, 2, cameraBuffer.buffer(2),
                                       0, sizeof(UniformTransforms),
                                       VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);

  // just use any texture (could be a dummy texture)
  pipelineWithoutTexture->bindResource(TEXTURES_AND_SAMPLER_SET, BINDING_0, 0,
                                       {textures.begin(), 1});

  pipelineWithoutTexture->bindResource(TEXTURES_AND_SAMPLER_SET, BINDING_1, 0,
                                       {samplers.begin(), 1});

  for (uint32_t i = 0; i < textures.size(); ++i) {
    pipelineWithTexture->bindResource(TEXTURES_AND_SAMPLER_SET, BINDING_0, i,
                                      {textures.begin() + i, 1});
    pipelineWithTexture->bindResource(TEXTURES_AND_SAMPLER_SET, BINDING_1, i,
                                      {samplers.begin(), 1});
  }

#pragma endregion

  float r = 0.6f, g = 0.6f, b = 1.f;
  size_t frame = 0;
  size_t previousFrame = 0;
  const std::array<VkClearValue, 2> clearValues = {VkClearValue{.color = {r, g, b, 0.0f}},
                                                   VkClearValue{.depthStencil = {1.0f}}};

  const glm::mat4 view = glm::translate(glm::mat4(1.f), {0.f, 0.f, 0.5f});
  auto time = glfwGetTime();

  std::unique_ptr<EngineCore::GUI::ImguiManager> imguiMgr = nullptr;

  TracyPlotConfig("Swapchain image index", tracy::PlotFormatType::Number, true, false,
                  tracy::Color::Aqua);

  while (!glfwWindowShouldClose(window_)) {
    const auto now = glfwGetTime();
    const auto delta = now - time;
    if (delta > 1) {
      const auto fps = static_cast<double>(frame - previousFrame) / delta;
      std::cerr << "FPS: " << fps << std::endl;
      previousFrame = frame;
      time = now;
    }

    const auto texture = context.swapchain()->acquireImage();
    const auto index = context.swapchain()->currentImageIndex();
    TracyPlot("Swapchain image index", (int64_t)index);

    const VulkanCore::DynamicRendering::AttachmentDescription colorAttachmentDesc{
        .imageView = texture->vkImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveModeFlagBits = VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .attachmentLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .attachmentStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clearValues[0]};

    const VulkanCore::DynamicRendering::AttachmentDescription depthAttachmentDesc{
        .imageView = depthTexture->vkImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
        .resolveModeFlagBits = VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .attachmentLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .attachmentStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE,
        .clearValue = clearValues[1],
    };

    auto commandBuffer = commandMgr.getCmdBufferToBegin();

    VulkanCore::DynamicRendering::beginRenderingCmd(commandBuffer, texture->vkImage(), 0,
                                                    {{0, 0},
                                                     {
                                                         texture->vkExtents().width,
                                                         texture->vkExtents().height,
                                                     }},
                                                    1, 0, {colorAttachmentDesc},
                                                    &depthAttachmentDesc, nullptr);

#pragma endregion

#pragma region Dynamic States
    const VkViewport viewport = {
        .x = 0.0f,
        .y = static_cast<float>(context.swapchain()->extent().height),
        .width = static_cast<float>(context.swapchain()->extent().width),
        .height = -static_cast<float>(context.swapchain()->extent().height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
    const VkRect2D scissor = {
        .offset = {0, 0},
        .extent = context.swapchain()->extent(),
    };
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    vkCmdSetDepthTestEnable(commandBuffer, VK_TRUE);
#pragma endregion

#pragma region Render

    if (camera.isDirty()) {
      transform.view = camera.viewMatrix();
      camera.setNotDirty();
    }
    cameraBuffer.buffer()->copyDataToBuffer(&transform, sizeof(UniformTransforms));

    for (uint32_t meshIdx = 0; meshIdx < numMeshes; ++meshIdx) {
      EngineCore::Material mat;
      if (bistro->meshes[meshIdx].material != -1) {
        mat = bistro->materials[bistro->meshes[meshIdx].material];
      }

      auto pipeline = pipelineWithTexture;

      if (mat.basecolorTextureId == -1) {
        pipeline = pipelineWithoutTexture;
      }

      auto vertexbufferIndex = meshIdx * 2;
      auto indexbufferIndex = meshIdx * 2 + 1;

      pipeline->bind(commandBuffer);

      pipeline->bindVertexBuffer(commandBuffer, buffers[vertexbufferIndex]->vkBuffer());
      pipeline->bindIndexBuffer(commandBuffer, buffers[indexbufferIndex]->vkBuffer());

      pipeline->bindDescriptorSets(
          commandBuffer,
          {
              {.set = CAMERA_SET, .bindIdx = (uint32_t)index},
              {.set = TEXTURES_AND_SAMPLER_SET,
               .bindIdx =
                   uint32_t(mat.basecolorTextureId == -1 ? 0 : mat.basecolorTextureId)},
          });

      const auto vertexCount = buffers[indexbufferIndex]->size() / sizeof(uint32_t);

      vkCmdDrawIndexed(commandBuffer, vertexCount, 1, 0, 0, 0);
    }

#pragma endregion

    VulkanCore::DynamicRendering::endRenderingCmd(commandBuffer, texture->vkImage());

    TracyVkCollect(tracyCtx_, commandBuffer);

    commandMgr.endCmdBuffer(commandBuffer);

    const VkPipelineStageFlags flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    const auto submitInfo = context.swapchain()->createSubmitInfo(&commandBuffer, &flags);
    commandMgr.submit(&submitInfo);
    commandMgr.goToNextCmdBuffer();

    context.swapchain()->present();
    glfwPollEvents();

    ++frame;

    cameraBuffer.moveToNextBuffer();

    FrameMarkNamed("main frame");
  }

  vkDeviceWaitIdle(context.device());

  return 0;
}
