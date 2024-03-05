#include "ImguiManager.hpp"

#if defined(_WIN32)

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_vulkan.h"
#include "imgui.h"
namespace EngineCore {
static void checkVkResult(VkResult err) {
  if (err == 0) return;
  std::cerr << "[imgui - vulkan] Error";
}
namespace GUI {
ImguiManager::ImguiManager(void* GLFWWindow, const VulkanCore::Context& context,
                           VkCommandBuffer commandBuffer, VkRenderPass renderPass,
                           VkSampleCountFlagBits msaaSamples)
    : device_(context.device()), renderPass_(renderPass) {
  ImGui_ImplVulkan_LoadFunctions([](const char* name, void*) {
    return vkGetInstanceProcAddr(volkGetLoadedInstance(), name);
  });

  const int numElements = 500;
  VkDescriptorPoolSize poolSizes[] = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, numElements},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numElements},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, numElements},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, numElements},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, numElements},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, numElements},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, numElements},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, numElements},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, numElements},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, numElements},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, numElements},
  };
  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  poolInfo.maxSets = numElements * (uint32_t)IM_ARRAYSIZE(poolSizes);
  poolInfo.poolSizeCount = (uint32_t)IM_ARRAYSIZE(poolSizes);
  poolInfo.pPoolSizes = poolSizes;
  VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_));

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO();

  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForVulkan(static_cast<GLFWwindow*>(GLFWWindow), true);

  ImGui_ImplVulkan_InitInfo init_info = {
      .Instance = context.instance(),
      .PhysicalDevice = context.physicalDevice().vkPhysicalDevice(),
      .Device = device_,
      .QueueFamily = context.physicalDevice().graphicsFamilyIndex().value(),
      .Queue = context.graphicsQueue(),
      .PipelineCache = VK_NULL_HANDLE,
      .DescriptorPool = descriptorPool_,
      .MinImageCount = context.swapchain()->numberImages(),
      .ImageCount = context.swapchain()->numberImages(),
      .MSAASamples = msaaSamples,
      .Allocator = nullptr,
      .CheckVkResultFn = &EngineCore::checkVkResult,
  };
  if (!ImGui_ImplVulkan_Init(&init_info, renderPass_)) {
    vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
    ASSERT(false, "Could not initialize imgui");
  }

  auto font = ImGui::GetIO().Fonts->AddFontDefault();

  auto result = ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
  ASSERT(result, "Error creating ImGui fonts.");

}  // namespace GUI

ImguiManager::ImguiManager(void* GLFWWindow, const VulkanCore::Context& context,
                           VkCommandBuffer commandBuffer, const VkFormat swapChainFormat,
                           VkSampleCountFlagBits msaaSamples)
    : device_(context.device()) {
  ImGui_ImplVulkan_LoadFunctions([](const char* name, void*) {
    return vkGetInstanceProcAddr(volkGetLoadedInstance(), name);
  });

  const int numElements = 500;
  VkDescriptorPoolSize poolSizes[] = {
      {VK_DESCRIPTOR_TYPE_SAMPLER, numElements},
      {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, numElements},
      {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, numElements},
      {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, numElements},
      {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, numElements},
      {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, numElements},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, numElements},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, numElements},
      {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, numElements},
      {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, numElements},
      {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, numElements},
  };
  VkDescriptorPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
  poolInfo.maxSets = numElements * (uint32_t)IM_ARRAYSIZE(poolSizes);
  poolInfo.poolSizeCount = (uint32_t)IM_ARRAYSIZE(poolSizes);
  poolInfo.pPoolSizes = poolSizes;
  VK_CHECK(vkCreateDescriptorPool(device_, &poolInfo, nullptr, &descriptorPool_));

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO();

  ImGui::StyleColorsDark();

  ImGui_ImplGlfw_InitForVulkan(static_cast<GLFWwindow*>(GLFWWindow), true);

  ImGui_ImplVulkan_InitInfo init_info = {
      .Instance = context.instance(),
      .PhysicalDevice = context.physicalDevice().vkPhysicalDevice(),
      .Device = device_,
      .QueueFamily = context.physicalDevice().graphicsFamilyIndex().value(),
      .Queue = context.graphicsQueue(),
      .PipelineCache = VK_NULL_HANDLE,
      .DescriptorPool = descriptorPool_,
      .MinImageCount = context.swapchain()->numberImages(),
      .ImageCount = context.swapchain()->numberImages(),
      .MSAASamples = msaaSamples,
      .UseDynamicRendering = true,
      .ColorAttachmentFormat = swapChainFormat,
      .Allocator = nullptr,
      .CheckVkResultFn = &EngineCore::checkVkResult,
  };
  if (!ImGui_ImplVulkan_Init(&init_info, renderPass_)) {
    vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
    ASSERT(false, "Could not initialize imgui");
  }

  auto font = ImGui::GetIO().Fonts->AddFontDefault();

  auto result = ImGui_ImplVulkan_CreateFontsTexture(commandBuffer);
  ASSERT(result, "Error creating ImGui fonts.");
}

ImguiManager::~ImguiManager() {
  if (descriptorPool_ != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
  }
  ImGui_ImplVulkan_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
}

void ImguiManager::frameBegin() {
  ImGui_ImplVulkan_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
}

void ImguiManager::createMenu() {
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Open", "Ctrl+O")) {
      }
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }
}

void ImguiManager::createDummyText() {
  static float val = .2;
  ImGui::Text("Hello, world!");
  ImGui::SliderFloat("Float", &val, 0.0f, 1.0f);
}
static float cameraPos[3] = {0.0f, 0.0f, 0.0f};
void ImguiManager::createCameraPosition(glm::vec3 pos) {
  cameraPos[0] = pos.x;
  cameraPos[1] = pos.y;
  cameraPos[2] = pos.z;

  ImGui::SliderFloat3("Camera Pos", &cameraPos[0], -100.0f, 100.0f);
}

glm::vec3 ImguiManager::cameraPosition() {
  return glm::vec3(cameraPos[0], cameraPos[1], cameraPos[2]);
}

static float cameraDirVar[3] = {0.0f, 0.0f, 0.0f};
void ImguiManager::createCameraDir(glm::vec3 dir) {
  cameraDirVar[0] = dir.x;
  cameraDirVar[1] = dir.y;
  cameraDirVar[2] = dir.z;

  ImGui::SliderFloat3("Camera Euler angles", &cameraDirVar[0], -360.0f, 360.0f);
}

glm::vec3 ImguiManager::cameraDir() {
  return glm::vec3(cameraDirVar[0], cameraDirVar[1], cameraDirVar[2]);
}

static float cameraUpVar[3] = {0.0f, 1.0f, 0.0f};
void ImguiManager::createCameraUpDir(glm::vec3 up) {
  cameraUpVar[0] = up.x;
  cameraUpVar[1] = up.y;
  cameraUpVar[2] = up.z;

  ImGui::SliderFloat3("Camera Up", &cameraUpVar[0], -1.0f, 1.0f);
}

glm::vec3 ImguiManager::cameraUpDir() {
  return glm::vec3(cameraUpVar[0], cameraUpVar[1], cameraUpVar[2]);
}

static float lightPos[3] = {0.0f, 0.0f, 0.0f};
void ImguiManager::createLightPos(glm::vec3 pos) {
  lightPos[0] = pos.x;
  lightPos[1] = pos.y;
  lightPos[2] = pos.z;

  ImGui::SliderFloat3("LightPos", &lightPos[0], -10.0f, 100.0f);
}

glm::vec3 ImguiManager::lightPosValue() {
  return glm::vec3(lightPos[0], lightPos[1], lightPos[2]);
}

static float lightDir[3] = {0.0f, 1.0f, 0.0f};
void ImguiManager::createLightDir(glm::vec3 pos) {
  lightDir[0] = pos.x;
  lightDir[1] = pos.y;
  lightDir[2] = pos.z;

  ImGui::SliderFloat3("Light Euler angles", &lightDir[0], -360.0f, 360.0f);
}

glm::vec3 ImguiManager::lightDirValue() {
  return glm::vec3(lightDir[0], lightDir[1], lightDir[2]);
}

static float lightUpDir[3] = {0.0f, 1.0f, 0.0f};
void ImguiManager::createLightUpDir(glm::vec3 pos) {
  lightUpDir[0] = pos.x;
  lightUpDir[1] = pos.y;
  lightUpDir[2] = pos.z;

  ImGui::SliderFloat3("LightUpDir", &lightDir[0], -1.0f, 1.0f);
}

glm::vec3 ImguiManager::lightUpDirValue() {
  return glm::vec3(lightUpDir[0], lightUpDir[1], lightUpDir[2]);
}

static float lightColor[3] = {0.0f, 0.0f, 0.0f};
void ImguiManager::createLightColor(glm::vec3 pos) {
  lightColor[0] = pos.x;
  lightColor[1] = pos.y;
  lightColor[2] = pos.z;

  ImGui::SliderFloat3("LightColor", &lightColor[0], 0.0f, 1.0f);
}

glm::vec3 ImguiManager::lightColorValue() {
  return glm::vec3(lightColor[0], lightColor[1], lightColor[2]);
}

static float ambientColor[3] = {0.0f, 0.0f, 0.0f};
void ImguiManager::createAmbientColor(glm::vec3 pos) {
  ambientColor[0] = pos.x;
  ambientColor[1] = pos.y;
  ambientColor[2] = pos.z;

  ImGui::SliderFloat3("Ambient Color", &ambientColor[0], 0.0f, 1.0f);
}

glm::vec3 ImguiManager::ambientColorValue() {
  return glm::vec3(ambientColor[0], ambientColor[1], ambientColor[2]);
}

static bool display = false;
void ImguiManager::setDisplayShadowMapTexture(bool val) {
  display = val;
  ImGui::Checkbox("Display shadowMap", &display);
}

bool ImguiManager::displayShadowMapTexture() { return display; }

void ImguiManager::frameEnd() {
  ImGui::EndFrame();
  ImGui::Render();
}

void ImguiManager::recordCommands(VkCommandBuffer commandBuffer) {
  ImDrawData* drawData = ImGui::GetDrawData();
  if (drawData != nullptr) {
    ImGui_ImplVulkan_RenderDrawData(drawData, commandBuffer);
  }
}

}  // namespace GUI
}  // namespace EngineCore

#endif
