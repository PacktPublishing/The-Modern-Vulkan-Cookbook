#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>
#include <stb_image.h>

#include <array>
#include <filesystem>
#include <gli/gli.hpp>
#include <glm/glm.hpp>

#include "enginecore/FPSCounter.hpp"
#include "vulkancore/CommandQueueManager.hpp"
#include "vulkancore/Context.hpp"
#include "vulkancore/Framebuffer.hpp"
#include "vulkancore/RenderPass.hpp"
#include "vulkancore/Texture.hpp"

GLFWwindow* window_ = nullptr;

bool initWindow(GLFWwindow** outWindow) {
  if (!glfwInit()) return false;

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  const char* title = "Chapter 1 - Triangle";

  uint32_t posX = 200;
  uint32_t posY = 200;
  uint32_t width = 800;
  uint32_t height = 600;

  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  GLFWwindow* window = glfwCreateWindow(width, height, title, nullptr, nullptr);

  if (!window) {
    glfwTerminate();
    return false;
  }

  glfwSetWindowPos(window, posX, posY);

  glfwSetErrorCallback([](int error, const char* description) {
    printf("GLFW Error (%i): %s\n", error, description);
  });

  glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int, int action, int mods) {
    const bool pressed = action != GLFW_RELEASE;
    if (key == GLFW_KEY_ESCAPE && pressed) {
      glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
    if (key == GLFW_KEY_ESCAPE && pressed) glfwSetWindowShouldClose(window, GLFW_TRUE);
  });

  if (outWindow) {
    *outWindow = window;
  }

  return true;
}

int main(int argc, char** argv) {
  initWindow(&window_);

  std::vector<std::string> validationLayers;
#ifdef _DEBUG
  validationLayers.push_back("VK_LAYER_KHRONOS_validation");
#endif

  // Create Context
  VulkanCore::Context::enableDefaultFeatures();
  VulkanCore::Context context((void*)glfwGetWin32Window(window_),
                              validationLayers,  // layers
                              {
                                  VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
                                  VK_KHR_SURFACE_EXTENSION_NAME,
                                  VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
                                  VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
                                  VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME,
                              },                                  // instance extensions
                              {VK_KHR_SWAPCHAIN_EXTENSION_NAME},  // device extensions
                              VK_QUEUE_GRAPHICS_BIT,  // request a graphics queue only
                              true);

  // Create Swapchain
  const VkExtent2D extents =
      context.physicalDevice().surfaceCapabilities().minImageExtent;

  const VkFormat swapChainFormat = VK_FORMAT_B8G8R8A8_UNORM;
  context.createSwapchain(swapChainFormat, VK_COLORSPACE_SRGB_NONLINEAR_KHR,
                          VK_PRESENT_MODE_FIFO_KHR, extents);
  const VkRect2D renderArea = {.offset = {.x = 0, .y = 0}, .extent = extents};

  // Create Shader Modules
  const auto shadersPath = std::filesystem::current_path() / "resources/shaders";
  const auto vertexShaderPath = shadersPath / "triangle.vert";
  const auto fragShaderPath = shadersPath / "triangle.frag";
  const auto vertexShader =
      context.createShaderModule(vertexShaderPath.string(), VK_SHADER_STAGE_VERTEX_BIT);
  const auto fragShader =
      context.createShaderModule(fragShaderPath.string(), VK_SHADER_STAGE_FRAGMENT_BIT);

  // Create Framebuffers
  std::vector<std::shared_ptr<VulkanCore::Framebuffer>> swapchain_framebuffers(
      context.swapchain()->numberImages());

  // Create Render Pass
  std::shared_ptr<VulkanCore::RenderPass> renderPass = context.createRenderPass(
      {context.swapchain()->texture(0)}, {VK_ATTACHMENT_LOAD_OP_CLEAR},
      {VK_ATTACHMENT_STORE_OP_STORE}, {VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
      VK_PIPELINE_BIND_POINT_GRAPHICS);

  const VkViewport viewport = {
      .x = 0.f,
      .y = 0.f,
      .width = static_cast<float>(context.swapchain()->extent().width),
      .height = static_cast<float>(context.swapchain()->extent().height),
      .minDepth = 0.0f,
      .maxDepth = 1.0f,
  };

  // Create Graphics Pipeline
  auto pipeline = context.createGraphicsPipeline(
      VulkanCore::Pipeline::GraphicsPipelineDescriptor{
          .vertexShader_ = vertexShader,
          .fragmentShader_ = fragShader,
          .colorTextureFormats = {swapChainFormat},
          .frontFace = VK_FRONT_FACE_CLOCKWISE,
          .viewport = viewport,
          .depthTestEnable = false,
      },
      renderPass->vkRenderPass());

  // Create Command Manager
  auto commandMgr = context.createGraphicsCommandQueue(
      context.swapchain()->numberImages(), context.swapchain()->numberImages());

  // FPS Counter
  EngineCore::FPSCounter fps(glfwGetTime());

  // Main Render Loop
  while (!glfwWindowShouldClose(window_)) {
    fps.update(glfwGetTime());

    const auto texture = context.swapchain()->acquireImage();
    const auto swapchainImageIndex = context.swapchain()->currentImageIndex();

    // Create the framebuffer the first time we get here, once for each
    // swapchain image
    if (swapchain_framebuffers[swapchainImageIndex] == nullptr) {
      swapchain_framebuffers[swapchainImageIndex] = context.createFramebuffer(
          renderPass->vkRenderPass(), {texture}, nullptr, nullptr);
    }

    auto commandBuffer = commandMgr.getCmdBufferToBegin();

    // Begin Render Pass
    constexpr VkClearValue clearColor{0.0f, 0.0f, 0.0f, 0.0f};
    const VkRenderPassBeginInfo renderpassInfo = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass->vkRenderPass(),
        .framebuffer = swapchain_framebuffers[swapchainImageIndex]->vkFramebuffer(),
        .renderArea = renderArea,
        .clearValueCount = 1,
        .pClearValues = &clearColor,
    };
    vkCmdBeginRenderPass(commandBuffer, &renderpassInfo, VK_SUBPASS_CONTENTS_INLINE);

    pipeline->bind(commandBuffer);

    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

    commandMgr.endCmdBuffer(commandBuffer);
    constexpr VkPipelineStageFlags flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    const auto submitInfo = context.swapchain()->createSubmitInfo(&commandBuffer, &flags);
    commandMgr.submit(&submitInfo);
    commandMgr.goToNextCmdBuffer();

    // Present render output to the screen
    context.swapchain()->present();

    glfwPollEvents();

    // Increment frame number
    fps.incFrame();
  }

  commandMgr.waitUntilAllSubmitsAreComplete();

  glfwDestroyWindow(window_);
  glfwTerminate();

  return 0;
}
