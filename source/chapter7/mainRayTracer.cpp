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

#include "enginecore/AsyncDataUploader.hpp"
#include "enginecore/Camera.hpp"
#include "enginecore/GLBLoader.hpp"
#include "enginecore/GLFWUtils.hpp"
#include "enginecore/ImguiManager.hpp"
#include "enginecore/Model.hpp"
#include "enginecore/RayTracer.hpp"
#include "enginecore/RingBuffer.hpp"
#include "imgui.h"
#include "vulkancore/Buffer.hpp"
#include "vulkancore/CommandQueueManager.hpp"
#include "vulkancore/Context.hpp"
#include "vulkancore/DynamicRendering.hpp"
#include "vulkancore/Framebuffer.hpp"
#include "vulkancore/Pipeline.hpp"
#include "vulkancore/RenderPass.hpp"
#include "vulkancore/Sampler.hpp"
#include "vulkancore/Texture.hpp"

// clang-format off
#include <tracy/TracyVulkan.hpp>
// clang-format on

// Enum for technique
enum Technique {
  RayTracerRadiance,
  AmbientOcclusion,
  TechniqueCount,
};

// char array for displaying in ImGui
const char* techniqueNames[] = {
    "RayTracerRadiance",
    "AmbientOcclusion",
};

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

  // push extension required for ray tracing
  deviceExtension.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
  deviceExtension.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
  deviceExtension.push_back(VK_KHR_RAY_QUERY_EXTENSION_NAME);
  deviceExtension.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);

  std::vector<std::string> validationLayers;
#ifdef _DEBUG
  validationLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif

  VulkanCore::Context::enableDefaultFeatures();

  VulkanCore::Context::enableBufferDeviceAddressFeature();

  VulkanCore::Context::enableRayTracingFeatures();

  VulkanCore::Context::enableDynamicRenderingFeature();

  VulkanCore::Context context((void*)glfwGetWin32Window(window_),
                              validationLayers,  // layers
                              instExtension,     // instance extensions
                              deviceExtension,   // device extensions
                              VkQueueFlags(VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_TRANSFER_BIT),
                              true, true);

  if (!context.physicalDevice().isRayTracingSupported()) {
    std::cout << "Ray tracing isn't supported on your GPU";
    return 0;
  }
#pragma endregion

#pragma region Swapchain initialization
  const VkExtent2D extents =
      context.physicalDevice().surfaceCapabilities().minImageExtent;

  const VkFormat swapChainFormat = VK_FORMAT_B8G8R8A8_UNORM;

  context.createSwapchain(swapChainFormat, VK_COLORSPACE_SRGB_NONLINEAR_KHR,
                          VK_PRESENT_MODE_MAILBOX_KHR, extents);

  static const uint32_t framesInFlight = (uint32_t)context.swapchain()->numberImages();
#pragma endregion

  std::unique_ptr<EngineCore::GUI::ImguiManager> imguiMgr = nullptr;

  // Create command pools
  auto commandMgr = context.createGraphicsCommandQueue(
      context.swapchain()->numberImages(), framesInFlight, "main command");

  float r = 1.f, g = 0.3f, b = 0.3f;
  size_t frame = 0;
  size_t previousFrame = 0;

  const glm::mat4 view = glm::translate(glm::mat4(1.f), {0.f, 0.f, 0.5f});
  auto time = glfwGetTime();

  std::vector<std::shared_ptr<VulkanCore::Buffer>> buffers;
  std::vector<std::shared_ptr<VulkanCore::Texture>> textures;
  std::vector<std::shared_ptr<VulkanCore::Sampler>> samplers;

  samplers.emplace_back(context.createSampler(
      VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_REPEAT,
      VK_SAMPLER_ADDRESS_MODE_REPEAT, VK_SAMPLER_ADDRESS_MODE_REPEAT, 10.0f,
      "default sampler"));

  const auto commandBuffer = commandMgr.getCmdBufferToBegin();

  EngineCore::GLBLoader glbLoader;
  std::shared_ptr<EngineCore::Model> bistro =
      glbLoader.load("resources/assets/Bistro.glb");

  EngineCore::convertModel2OneBuffer(context, commandMgr, commandBuffer, *bistro.get(),
                                     buffers, textures, samplers);

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

  commandMgr.endCmdBuffer(commandBuffer);

  VkPipelineStageFlags flags = VK_PIPELINE_STAGE_TRANSFER_BIT;
  const auto submitInfo =
      context.swapchain()->createSubmitInfo(&commandBuffer, &flags, false, false);
  commandMgr.submit(&submitInfo);
  commandMgr.waitUntilSubmitIsComplete();

  EngineCore::RayTracer raytracer;
  raytracer.init(&context, bistro, buffers, textures, samplers);

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

    auto commandBuffer = commandMgr.getCmdBufferToBegin();

    static Technique imgui_currentTechnique = RayTracerRadiance;

    if (!imguiMgr) {
      imguiMgr = std::make_unique<EngineCore::GUI::ImguiManager>(
          window_, context, commandBuffer, swapChainFormat, VK_SAMPLE_COUNT_1_BIT);
    }

    bool showAOImage = false;

    if (imgui_currentTechnique == RayTracerRadiance) {
      showAOImage = false;
    } else if (imgui_currentTechnique == AmbientOcclusion) {
      showAOImage = true;
    }

    raytracer.execute(commandBuffer, index, camera.viewMatrix(),
                      camera.getProjectMatrix(), showAOImage);

    raytracer.currentImage(index)->transitionImageLayout(
        commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

    context.swapchain()->texture(index)->transitionImageLayout(
        commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    VkImageCopy region{
        .srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .srcOffset = {0, 0, 0},
        .dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
        .dstOffset = {0, 0, 0},
        .extent = {context.swapchain()->texture(index)->vkExtents().width,
                   context.swapchain()->texture(index)->vkExtents().height, 1},
    };

    texture->transitionImageLayout(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

    vkCmdCopyImage(commandBuffer, raytracer.currentImage(index)->vkImage(),
                   VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                   context.swapchain()->texture(index)->vkImage(),
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

#pragma region imgui
    context.beginDebugUtilsLabel(commandBuffer, "Imgui pass", {0.0f, 1.0f, 0.0f, 1.0f});
    if (imguiMgr) {
      imguiMgr->frameBegin();

      int currentItem = static_cast<int>(imgui_currentTechnique);
      ImGui::Combo("Ray Tracing", &currentItem, techniqueNames, TechniqueCount);
      imgui_currentTechnique = static_cast<Technique>(currentItem);

      imguiMgr->frameEnd();
    }

    const VulkanCore::DynamicRendering::AttachmentDescription colorAttachmentDesc{
        .imageView = texture->vkImageView(),
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .resolveModeFlagBits = VK_RESOLVE_MODE_NONE,
        .resolveImageView = VK_NULL_HANDLE,
        .resolveImageLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .attachmentLoadOp =
            VK_ATTACHMENT_LOAD_OP_LOAD,  // load since we want to preserve content
        .attachmentStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = VkClearValue{.color = {0.0f, 0.0f, 0.0f, 0.0f}},
    };

    VulkanCore::DynamicRendering::beginRenderingCmd(
        commandBuffer, texture->vkImage(), 0,
        {{0, 0}, {texture->vkExtents().width, texture->vkExtents().height}}, 1, 0,
        {colorAttachmentDesc}, nullptr, nullptr, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    if (imguiMgr) {
      imguiMgr->recordCommands(commandBuffer);
    }
    VulkanCore::DynamicRendering::endRenderingCmd(commandBuffer, texture->vkImage());
    texture->setImageLayout(VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
    context.endDebugUtilsLabel(commandBuffer);
#pragma endregion

    commandMgr.endCmdBuffer(commandBuffer);

    VkPipelineStageFlags flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    const auto submitInfo = context.swapchain()->createSubmitInfo(&commandBuffer, &flags);
    commandMgr.submit(&submitInfo);
    commandMgr.goToNextCmdBuffer();
    context.swapchain()->present();
    glfwPollEvents();

    vkDeviceWaitIdle(context.device());

    ++frame;

    FrameMarkNamed("main frame");
  }

  vkDeviceWaitIdle(context.device());

  return 0;
}
