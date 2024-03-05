#ifndef OPENXR_SAMPLE_OXRCONTEXT_H
#define OPENXR_SAMPLE_OXRCONTEXT_H

#include <jni.h>

#include <array>
#include <vector>
#include <string>

#include <vulkancore/Context.hpp>
#include <vulkancore/Texture.hpp>

#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>

#include <glm/glm.hpp>

#include "Common.h"
#include "OXRSwapchain.h"

struct android_app;

namespace OXR {

    class Context {
    public:
        static constexpr auto kNumViews = 2; // 2 for stereo
        static constexpr auto kUseSinglePassStereo = true;

    public:
        Context(struct android_app *pApp = nullptr);

        ~Context();

        void setInitialized() noexcept {
          initialized_ = true;
        }

        [[nodiscard]] bool initialized() const noexcept {
            return initialized_;
        }

        void initializeExtensions();

        bool createInstance();

        void systemInfo();

        bool enumerateViewConfigurations();

        bool
        initializeSession(VkInstance vkInstance, VkPhysicalDevice vkPhysDevice, VkDevice vkDevice,
                          uint32_t queueFamilyIndex);

        void enumerateReferenceSpaces();

        void initGraphics();

        VkPhysicalDevice findVkGraphicsDevice(VkInstance vkInstance);

        const std::vector<std::string> &vkInstanceExtensions() const;

        const std::vector<std::string> &vkDeviceExtensions() const;

        void createSwapchains(VulkanCore::Context &ctx);

        void createSpaces();

        XrFrameState beginFrame();

        void endFrame(XrFrameState frameState);

        OXRSwapchain *const swapchain(uint32_t index) const {
          return swapchains_[index].get();
        }

        OXRSwapchain *swapchain(uint32_t index) {
          return swapchains_[index].get();
        }

        void setResumed(bool resumed) {
          resumed_ = resumed;
        }

        bool resumed() const {
          return resumed_;
        }

        void setNativeWindow(void *win) {
          nativeWindow_ = win;
        }

        void *nativeWindow() const {
          return nativeWindow_;
        }

        bool sessionActive() const {
          return sessionActive_;
        }

        void handleSessionStateChanges(XrSessionState state);

        void handleXrEvents();

        glm::mat4 mvp(uint32_t i) const;

        // left: 0 - right: 1
        glm::vec3 eyeGazeScreenPos(uint32_t eye) const {
          return eyeGazePositionScreen_[eye];
        }

        glm::mat4 projection(uint32_t i) const;

        glm::vec3 eyeGazePosition() const {
          return eyeGazePosition_;
        }

    private:
        std::vector<std::string> processExtensionsBuffer(const std::vector<char> &buffer);

        glm::vec3 screenCoordinatesFromEyeGazePose(XrSpaceLocation gazeLocation, int eye, float offset = -100.f);

    private:
        bool initialized_ = false;

        void *nativeWindow_ = nullptr;
        bool resumed_ = false;
        bool sessionActive_ = false;

        std::vector<const char *> requestedExtensions_ = {
            XR_KHR_VULKAN_ENABLE_EXTENSION_NAME,
            XR_FB_SWAPCHAIN_UPDATE_STATE_VULKAN_EXTENSION_NAME,
            XR_KHR_COMPOSITION_LAYER_DEPTH_EXTENSION_NAME,
            XR_EXT_EYE_GAZE_INTERACTION_EXTENSION_NAME,
        };
        std::vector<XrExtensionProperties> availableExtensions_;
        XrInstance instance_ = XR_NULL_HANDLE;
        XrInstanceProperties instanceProps_ = {.type = XR_TYPE_INSTANCE_PROPERTIES, .next = nullptr};
        XrSystemId systemId_ = XR_NULL_SYSTEM_ID;
        XrSystemEyeGazeInteractionPropertiesEXT eyeGazeProperties_ = {
            .type = XR_TYPE_SYSTEM_EYE_GAZE_INTERACTION_PROPERTIES_EXT,
        };
        XrSystemProperties systemProps_ = {
            .type = XR_TYPE_SYSTEM_PROPERTIES,
            .next = &eyeGazeProperties_,
        };
        XrSession session_ = XR_NULL_HANDLE;
        // Eye Gaze
        XrPosef eyePoseIdentity_ = {
            .orientation = {.x = 0, .y = 0, .z = 0, .w = 1.f,},
            .position = {0.f, 0.f, 0.f,},
        };
        XrActionSet eyegazeActionSet_ = XR_NULL_HANDLE;
        XrAction eyegazeAction_ = XR_NULL_HANDLE;
        XrSpace gazeActionSpace_ = XR_NULL_HANDLE;
        XrSpace localReferenceSpace_ = XR_NULL_HANDLE;

        XrViewConfigurationProperties viewConfigProps_ = {.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};
        std::array<XrViewConfigurationView, kNumViews> viewports_;
        std::array<XrView, kNumViews> views_;
        std::array<XrPosef, kNumViews> viewStagePoses_;
        std::array<glm::mat4, kNumViews> viewTransforms_;
        std::array<glm::vec3, kNumViews> cameraPositions_;

        XrSessionState currentState_ = XR_SESSION_STATE_UNKNOWN;

        // initGraphics
        XrGraphicsRequirementsVulkanKHR graphicsRequirements_ = {
            .type = XR_TYPE_GRAPHICS_REQUIREMENTS_VULKAN_KHR,
        };
        std::vector<std::string> requiredVkInstanceExtensions_;
        std::vector<char> requiredVkInstanceExtensionsBuffer_;
        std::vector<std::string> requiredVkDeviceExtensions_;
        std::vector<char> requiredVkDeviceExtensionsBuffer_;

        bool useSinglePassStereo_ = kUseSinglePassStereo;

        // Swapchain
        std::vector<std::unique_ptr<OXRSwapchain>> swapchains_;

        // Spaces
        XrSpace headSpace_ = XR_NULL_HANDLE;
        XrSpace localSpace_ = XR_NULL_HANDLE;
        XrSpace stageSpace_ = XR_NULL_HANDLE;
        bool stageSpaceSupported_ = false;

        // Eye Gaze
        std::array<glm::vec3, 2> eyeGazePositionScreen_;
        glm::vec3 eyeGazePosition_;

        // Projection
        float near_ = 0.05f;
        float far_ = 100.f;
    };

} // OXR

#endif //OPENXR_SAMPLE_OXRCONTEXT_H
