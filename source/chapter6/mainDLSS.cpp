#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <stb_image.h>

#include <array>
#include <filesystem>
#include <gli/gli.hpp>
#include <glm/glm.hpp>
#include <memory>
#include <tracy/Tracy.hpp>

#include "enginecore/AsyncDataUploader.hpp"
#include "enginecore/Camera.hpp"
#include "enginecore/DLSS.hpp"
#include "enginecore/GLBLoader.hpp"
#include "enginecore/GLFWUtils.hpp"
#include "enginecore/ImguiManager.hpp"
#include "enginecore/Model.hpp"
#include "enginecore/RingBuffer.hpp"
#include "enginecore/passes/CullingComputePass.hpp"
#include "enginecore/passes/FullScreenPass.hpp"
#include "enginecore/passes/GBufferPass.hpp"
#include "imgui.h"
#include "vulkancore/Buffer.hpp"
#include "vulkancore/CommandQueueManager.hpp"
#include "vulkancore/Context.hpp"
#include "vulkancore/Framebuffer.hpp"
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
  std::vector<std::string> instExtension = {
      VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
      VK_KHR_SURFACE_EXTENSION_NAME,
      VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
      VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
  };

  std::vector<std::string> deviceExtension = {
#if defined(VK_EXT_calibrated_timestamps)
    VK_EXT_CALIBRATED_TIMESTAMPS_EXTENSION_NAME,
#endif
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_EXT_MEMORY_BUDGET_EXTENSION_NAME,
  };

  EngineCore::DLSS::requiredExtensions(instExtension, deviceExtension);

  std::vector<std::string> validationLayers;
#ifdef _DEBUG
  validationLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif

  VulkanCore::Context::enableDefaultFeatures();
  VulkanCore::Context::enableIndirectRenderingFeature();
  VulkanCore::Context::enableSynchronization2Feature();  // needed for acquire/release
                                                         // barriers
  VulkanCore::Context::enableBufferDeviceAddressFeature();

  VulkanCore::Context context(
      (void*)glfwGetWin32Window(window_),
      validationLayers,  // layers
      instExtension,     // instance extensions
      deviceExtension,   // device extensions
      VkQueueFlags(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT | VK_QUEUE_TRANSFER_BIT),
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

  auto dlssOutputTexture = context.createTexture(
      VK_IMAGE_TYPE_2D, swapChainFormat, 0,
      VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
          VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
          VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      VkExtent3D{
          .width = context.swapchain()->extent().width,
          .height = context.swapchain()->extent().height,
          .depth = static_cast<uint32_t>(1.0),
      },
      1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false, VK_SAMPLE_COUNT_1_BIT,
      "DLSS Output Texture");

  EngineCore::DLSS dlss(context.instance(), context.physicalDevice().vkPhysicalDevice(),
                        context.device());
  if (dlss.isSupported()) {
    dlss.init(extents.width, extents.height, 1.0, commandMgr);
  } else {
    std::cerr << "DLSS not supported";
    return 0;
  }

#pragma region Tracy initialization
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

  UniformTransforms transform = {
      .model = glm::mat4(1.0f),
      .view = camera.viewMatrix(),
      .projection = camera.getProjectMatrix(),
      .prevViewMat = camera.viewMatrix(),
  };

  constexpr uint32_t CAMERA_SET = 0;
  constexpr uint32_t TEXTURES_SET = 1;
  constexpr uint32_t SAMPLER_SET = 2;
  constexpr uint32_t STORAGE_BUFFER_SET =
      3;  // storing vertex/index/indirect/material buffer in array
  constexpr uint32_t BINDING_0 = 0;
  constexpr uint32_t BINDING_1 = 1;
  constexpr uint32_t BINDING_2 = 2;
  constexpr uint32_t BINDING_3 = 3;

  auto emptyTexture =
      context.createTexture(VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, 0,
                            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
                                VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                            VkExtent3D{
                                .width = static_cast<uint32_t>(1),
                                .height = static_cast<uint32_t>(1),
                                .depth = static_cast<uint32_t>(1.0),
                            },
                            1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, false,
                            VK_SAMPLE_COUNT_1_BIT, "Empty Texture");

  std::vector<std::shared_ptr<VulkanCore::Buffer>> buffers;
  std::vector<std::shared_ptr<VulkanCore::Texture>> textures;
  std::vector<std::shared_ptr<VulkanCore::Sampler>> samplers;
  samplers.emplace_back(context.createSampler(
      VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT,
      VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, 10.0f,
      "default sampler"));
  EngineCore::RingBuffer cameraBuffer(context.swapchain()->numberImages(), context,
                                      sizeof(UniformTransforms));

  uint32_t numMeshes = 0;
  std::shared_ptr<EngineCore::Model> bistro;
  BS::thread_pool pool(std::thread::hardware_concurrency() - 2);
  pool.pause();

  CullingComputePass cullingPass;

  GBufferPass gbufferPass;
  gbufferPass.init(&context, context.swapchain()->extent().width,
                   context.swapchain()->extent().height);

  FullScreenPass fullscreenPass;
  fullscreenPass.init(&context, {swapChainFormat});

  auto textureToDisplay = dlssOutputTexture;

  fullscreenPass.pipeline()->bindResource(0, 0, 0, {&textureToDisplay, 1}, samplers[0]);

  std::shared_ptr<VulkanCore::Pipeline> gbufferPipeline = gbufferPass.pipeline();

  auto textureReadyCB = [&gbufferPipeline, &textures](int textureIndex, int modelId) {
    gbufferPipeline->bindResource(TEXTURES_SET, BINDING_0, 0,
                                  {textures.begin() + textureIndex, 1}, nullptr,
                                  textureIndex);
  };

  EngineCore::AsyncDataUploader dataUploader(context, textureReadyCB);

  auto glbTextureDataLoadedCB = [&context, &bistro, &textures, &dataUploader](
                                    int textureIndex, int modelId) {
    EngineCore::AsyncDataUploader::TextureLoadTask t;
    textures[textureIndex] = context.createTexture(
        VK_IMAGE_TYPE_2D, VK_FORMAT_R8G8B8A8_UNORM, 0,
        VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VkExtent3D{
            .width = static_cast<uint32_t>(bistro->textures[textureIndex]->width),
            .height = static_cast<uint32_t>(bistro->textures[textureIndex]->height),
            .depth = 1u,
        },
        1, 1, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, true, VK_SAMPLE_COUNT_1_BIT,
        std::to_string(textureIndex));
    t.texture = textures[textureIndex].get();
    t.data = bistro->textures[textureIndex]->data;
    t.index = textureIndex;
    t.modelIndex = modelId;
    dataUploader.queueTextureUploadTasks(t);
  };

#pragma region Load model
  {
    const auto commandBuffer = commandMgr.getCmdBufferToBegin();
    emptyTexture->transitionImageLayout(commandBuffer,
                                        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    {
      ZoneScopedN("Model load");
      EngineCore::GLBLoader glbLoader;
      bistro =
          glbLoader.load("resources/assets/Bistro.glb", pool, glbTextureDataLoadedCB);
      TracyVkZone(tracyCtx_, commandBuffer, "Model upload");
      EngineCore::convertModel2OneBuffer(context, commandMgr, commandBuffer,
                                         *bistro.get(), buffers, samplers);
      textures.resize(bistro->textures.size(), emptyTexture);
      numMeshes = bistro->meshes.size();
    }

    TracyVkCollect(tracyCtx_, commandBuffer);
    commandMgr.endCmdBuffer(commandBuffer);

    VkPipelineStageFlags flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
    const auto submitInfo =
        context.swapchain()->createSubmitInfo(&commandBuffer, &flags, false, false);
    commandMgr.submit(&submitInfo);
    commandMgr.waitUntilSubmitIsComplete();
  }
#pragma endregion

#pragma region Pipeline initialization

  cullingPass.init(&context, &camera, *bistro.get(), buffers[3]);
  cullingPass.upload(commandMgr);

  gbufferPipeline->bindResource(CAMERA_SET, BINDING_0, 0, cameraBuffer.buffer(0), 0,
                                sizeof(UniformTransforms),
                                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  gbufferPipeline->bindResource(CAMERA_SET, BINDING_0, 1, cameraBuffer.buffer(1), 0,
                                sizeof(UniformTransforms),
                                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  gbufferPipeline->bindResource(CAMERA_SET, BINDING_0, 2, cameraBuffer.buffer(2), 0,
                                sizeof(UniformTransforms),
                                VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
  gbufferPipeline->bindResource(STORAGE_BUFFER_SET, BINDING_0, 0,
                                {buffers[0], buffers[1], buffers[3],
                                 buffers[2]},  // vertex, index, indirect, material
                                VK_DESCRIPTOR_TYPE_STORAGE_BUFFER);
  gbufferPipeline->bindResource(TEXTURES_SET, BINDING_0, 0,
                                {textures.begin(), textures.end()});
  gbufferPipeline->bindResource(SAMPLER_SET, BINDING_0, 0, {samplers.begin(), 1});

#pragma endregion

  float r = 0.6f, g = 0.6f, b = 1.f;
  size_t frameIndex = 0;
  size_t previousFrameIndex = 0;
  const std::array<VkClearValue, 1> clearValues = {
      VkClearValue{.color = {r, g, b, 0.0f}}};

  const glm::mat4 view = glm::translate(glm::mat4(1.f), {0.f, 0.f, 0.5f});
  auto time = glfwGetTime();

  std::unique_ptr<EngineCore::GUI::ImguiManager> imguiMgr = nullptr;

  TracyPlotConfig("Swapchain image index", tracy::PlotFormatType::Number, true, false,
                  tracy::Color::Aqua);

  dataUploader.startProcessing();
  pool.unpause();

  glm::mat4 prevViewMat = camera.viewMatrix();

  while (!glfwWindowShouldClose(window_)) {
    const auto now = glfwGetTime();
    const auto delta = now - time;
    if (delta > 1) {
      const auto fps = static_cast<double>(frameIndex - previousFrameIndex) / delta;
      std::cerr << "FPS: " << fps << std::endl;
      previousFrameIndex = frameIndex;
      time = now;
    }

    if (frameIndex == std::numeric_limits<uint32_t>::max()) {
      frameIndex = 0;
      previousFrameIndex = 0;
    }

    camera.updateJitterMat(frameIndex, 16, 1600, 1200);

    if (camera.isDirty()) {
      transform.view = camera.viewMatrix();
      camera.setNotDirty();
    }

    transform.prevViewMat = prevViewMat;
    transform.jitter = camera.jitterMat();

    cameraBuffer.buffer()->copyDataToBuffer(&transform, sizeof(UniformTransforms));

    prevViewMat = camera.viewMatrix();

    commandMgr.waitUntilSubmitIsComplete();
    const auto texture = context.swapchain()->acquireImage();
    const auto index = context.swapchain()->currentImageIndex();
    TracyPlot("Swapchain image index", (int64_t)index);

    auto commandBuffer = commandMgr.getCmdBufferToBegin();

    cullingPass.cull(commandBuffer, index);
    cullingPass.addBarrierForCulledBuffers(
        commandBuffer, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
        context.physicalDevice().graphicsFamilyIndex().value(),
        context.physicalDevice().graphicsFamilyIndex().value());

    gbufferPass.render(commandBuffer, index,
                       {
                           {.set = CAMERA_SET, .bindIdx = (uint32_t)index},
                           {.set = TEXTURES_SET, .bindIdx = 0},
                           {.set = SAMPLER_SET, .bindIdx = 0},
                           {.set = STORAGE_BUFFER_SET, .bindIdx = 0},
                       },
                       buffers[1]->vkBuffer(),
                       cullingPass.culledIndirectDrawBuffer()->vkBuffer(),
                       cullingPass.culledIndirectDrawCountBuffer()->vkBuffer(), numMeshes,
                       sizeof(EngineCore::IndirectDrawCommandAndMeshData));

    dlss.render(commandBuffer, *gbufferPass.baseColorTexture(),
                *gbufferPass.depthTexture(), *gbufferPass.velocityTexture(),
                *dlssOutputTexture, camera.jitterInPixelSpace());

    if (!imguiMgr) {
      imguiMgr = std::make_unique<EngineCore::GUI::ImguiManager>(
          window_, context, commandBuffer,
          fullscreenPass.renderPass() ? fullscreenPass.renderPass()->vkRenderPass()
                                      : VK_NULL_HANDLE,
          VK_SAMPLE_COUNT_1_BIT);
    }

    if (imguiMgr) {
      imguiMgr->frameBegin();
      imguiMgr->createCameraPosition(camera.position());
      camera.setPos(imguiMgr->cameraPosition());
      imguiMgr->createCameraDir(camera.eulerAngles());
      camera.setEulerAngles(imguiMgr->cameraDir());

      static bool displayBaseColorTexture = false;
      ImGui::Checkbox("Display original texture", &displayBaseColorTexture);

      if (displayBaseColorTexture) {
        auto texture = gbufferPass.baseColorTexture();

        fullscreenPass.pipeline()->bindResource(0, 0, 0, {&texture, 1}, samplers[0]);
      } else {
        fullscreenPass.pipeline()->bindResource(0, 0, 0, {&textureToDisplay, 1},
                                                samplers[0]);
      }

      imguiMgr->frameEnd();
    }

    fullscreenPass.render(commandBuffer, index, imguiMgr.get(),
                          imguiMgr->displayShadowMapTexture() ? true : false);

    TracyVkCollect(tracyCtx_, commandBuffer);

    commandMgr.endCmdBuffer(commandBuffer);

    VkPipelineStageFlags flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    const auto submitInfo = context.swapchain()->createSubmitInfo(&commandBuffer, &flags);
    commandMgr.submit(&submitInfo);
    commandMgr.goToNextCmdBuffer();

    context.swapchain()->present();
    glfwPollEvents();

    ++frameIndex;

    cameraBuffer.moveToNextBuffer();

    FrameMarkNamed("main frame");
  }

  vkDeviceWaitIdle(context.device());

  if (imguiMgr) {
    imguiMgr.reset();
  }

  return 0;
}
