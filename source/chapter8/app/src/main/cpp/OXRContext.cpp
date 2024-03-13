#include <cassert>

#include <android_native_app_glue.h>

#include "Common.h"
#include "OXRContext.h"

#include "xr_linear.h"
#include <glm/gtc/type_ptr.hpp>

namespace {
    constexpr auto kSupportedViewConfigType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO; //XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO; //XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
}

namespace OXR {

    Context::Context(struct android_app *pApp) {
      assert(pApp);

      PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR;
      const auto result = xrGetInstanceProcAddr(
          XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction *) &xrInitializeLoaderKHR);
      if (result != XR_SUCCESS) {
        LOGE("Error retrieving xrInitializeLoaderKHR");
      }
      if (xrInitializeLoaderKHR) {
        LOGI("Got xrInitializeLoaderKHR!");
        XrLoaderInitInfoAndroidKHR loaderInitializeInfoAndroid = {
            XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR,
            nullptr,
            pApp->activity->vm,
            pApp->activity->clazz,
        };

        xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR *) &loaderInitializeInfoAndroid);
      }
    }

    Context::~Context() {
      if (!initialized_) {
        return;
      }

      xrDestroySpace(localReferenceSpace_);
      xrDestroySpace(gazeActionSpace_);
      xrDestroyAction(eyegazeAction_);
      xrDestroyActionSet(eyegazeActionSet_);
      xrDestroySpace(stageSpace_);
      xrDestroySpace(localSpace_);
      xrDestroySpace(headSpace_);
      xrDestroySession(session_);
      xrDestroyInstance(instance_);
    }

    void Context::initializeExtensions() {
      // Check that the extensions required are present.
      XrResult result = XR_RESULT_MAX_ENUM;
      PFN_xrEnumerateInstanceExtensionProperties xrEnumerateInstanceExtensionProperties;
      result = xrGetInstanceProcAddr(XR_NULL_HANDLE,
                                     "xrEnumerateInstanceExtensionProperties",
                                     (PFN_xrVoidFunction *) &xrEnumerateInstanceExtensionProperties);
      if (result != XR_SUCCESS) {
        LOGE("Failed to get xrEnumerateInstanceExtensionProperties function pointer.");
        return;
      }

      uint32_t numExtensions = 0;
      result = xrEnumerateInstanceExtensionProperties(nullptr, 0, &numExtensions, nullptr);
      LOGI("xrEnumerateInstanceExtensionProperties found %u extension(s).", numExtensions);

      availableExtensions_.resize(numExtensions, {XR_TYPE_EXTENSION_PROPERTIES});

      result = xrEnumerateInstanceExtensionProperties(
          nullptr, numExtensions, &numExtensions, availableExtensions_.data());
      for (uint32_t i = 0; i < numExtensions; i++) {
        LOGI("%s", availableExtensions_[i].extensionName);
      }

      requestedExtensions_.erase(std::remove_if(requestedExtensions_.begin(), requestedExtensions_.end(),
                                                [this](const char* ext) {
                                                    return std::none_of(availableExtensions_.begin(), availableExtensions_.end(),
                                                                        [ext](const XrExtensionProperties &props) { return strcmp(props.extensionName, ext) == 0;});
                                                }),
                                 requestedExtensions_.end());

    }

    bool Context::createInstance() {
      const XrApplicationInfo appInfo = {
          .applicationName = "OpenXR Example",
          .applicationVersion = 0,
          .engineName = "OpenXR Example",
          .engineVersion = 0,
          .apiVersion = XR_CURRENT_API_VERSION,};

      const XrInstanceCreateInfo instanceCreateInfo = {
          .type = XR_TYPE_INSTANCE_CREATE_INFO,
          .createFlags = 0,
          .applicationInfo = appInfo,
          .enabledApiLayerCount = 0,
          .enabledApiLayerNames = nullptr,
          .enabledExtensionCount = static_cast<uint32_t>(requestedExtensions_.size()),
          .enabledExtensionNames = requestedExtensions_.data(),
      };

      XR_CHECK(xrCreateInstance(&instanceCreateInfo, &instance_));
      XR_CHECK(xrGetInstanceProperties(instance_, &instanceProps_));

      // EYE_GAZE **********************************************************************************
      XrActionSetCreateInfo actionSetInfo{.type = XR_TYPE_ACTION_SET_CREATE_INFO,
          .actionSetName = "gameplay",
          .localizedActionSetName = "Eye Gaze Action Set",
          .priority = 0,};
      XR_CHECK(xrCreateActionSet(instance_, &actionSetInfo, &eyegazeActionSet_));

      // Create user intent action
      const XrActionCreateInfo actionInfo{.type = XR_TYPE_ACTION_CREATE_INFO,
          .actionName = "user_intent",
          .actionType = XR_ACTION_TYPE_POSE_INPUT,
          .localizedActionName = "Eye Gaze Action",
      };
      XR_CHECK(xrCreateAction(eyegazeActionSet_, &actionInfo, &eyegazeAction_));

      // Create suggested bindings
      XrPath eyeGazeInteractionProfilePath;
      XR_CHECK(xrStringToPath(instance_, "/interaction_profiles/ext/eye_gaze_interaction",
                              &eyeGazeInteractionProfilePath));

      XrPath gazePosePath;
      XR_CHECK(xrStringToPath(instance_, "/user/eyes_ext/input/gaze_ext/pose", &gazePosePath));

      const XrActionSuggestedBinding bindings{
          .action = eyegazeAction_,
          .binding = gazePosePath,
      };

      const XrInteractionProfileSuggestedBinding suggestedBindings{
          .type = XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING,
          .interactionProfile = eyeGazeInteractionProfilePath,
          .countSuggestedBindings = 1,
          .suggestedBindings = &bindings,
      };
      XR_CHECK(xrSuggestInteractionProfileBindings(instance_, &suggestedBindings));

      return true;
    }

    void Context::systemInfo() {
      const XrSystemGetInfo systemGetInfo = {
          .type = XR_TYPE_SYSTEM_GET_INFO,
          .formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY,
      };
      XR_CHECK(xrGetSystem(instance_, &systemGetInfo, &systemId_));
      XR_CHECK(xrGetSystemProperties(instance_, systemId_, &systemProps_));
    }

    bool Context::enumerateViewConfigurations() {
      uint32_t numViewConfigs = 0;
      XR_CHECK(xrEnumerateViewConfigurations(instance_, systemId_, 0, &numViewConfigs, nullptr));

      std::vector<XrViewConfigurationType> viewConfigTypes(numViewConfigs);
      XR_CHECK(xrEnumerateViewConfigurations(
          instance_, systemId_, numViewConfigs, &numViewConfigs, viewConfigTypes.data()));

      auto foundViewConfig = false;
      for (auto &viewConfigType: viewConfigTypes) {
        if (viewConfigType != kSupportedViewConfigType) {
          continue;
        }

        // Check properties
        XrViewConfigurationProperties viewConfigProps = {XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};

        XR_CHECK(xrGetViewConfigurationProperties(instance_, systemId_, viewConfigType,
                                                  &viewConfigProps));

        uint32_t numViewports = 0;
        XR_CHECK(xrEnumerateViewConfigurationViews(
            instance_, systemId_, viewConfigType, 0, &numViewports, nullptr));
        if (numViewports != kNumViews) {
          LOGE(
              "numViewports must be %d. Make sure XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO is used.",
              kNumViews);
          return false;
        }

        for (uint32_t i = 0; i < numViewports; ++i) {
          viewports_[i].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
        }
        XR_CHECK(xrEnumerateViewConfigurationViews(
            instance_, systemId_, viewConfigType, numViewports, &numViewports, viewports_.data()));

        viewConfigProps_ = viewConfigProps;

        foundViewConfig = true;

        break;
      }

      assert(foundViewConfig);
      if (!foundViewConfig) {
        LOGE("XrViewConfigurationType %d not found.", kSupportedViewConfigType);
      }

      return true;
    }

    bool Context::initializeSession(VkInstance vkInstance, VkPhysicalDevice vkPhysDevice,
                                    VkDevice vkDevice, uint32_t queueFamilyIndex) {
      // Bind Vulkan to XR session
      const XrGraphicsBindingVulkanKHR graphicsBinding = {
          XR_TYPE_GRAPHICS_BINDING_VULKAN_KHR,
          NULL,
          vkInstance,
          vkPhysDevice,
          vkDevice,
          queueFamilyIndex,
          0,
      };

      const XrSessionCreateInfo sessionCreateInfo = {
          .type = XR_TYPE_SESSION_CREATE_INFO,
          .next = &graphicsBinding,
          .createFlags = 0,
          .systemId = systemId_,
      };

      XR_CHECK(xrCreateSession(instance_, &sessionCreateInfo, &session_));

      return true;
    }

    void Context::enumerateReferenceSpaces() {
      uint32_t numRefSpaceTypes = 0;
      XR_CHECK(xrEnumerateReferenceSpaces(session_, 0, &numRefSpaceTypes, nullptr));

      std::vector<XrReferenceSpaceType> refSpaceTypes(numRefSpaceTypes);

      XR_CHECK(xrEnumerateReferenceSpaces(
          session_, numRefSpaceTypes, &numRefSpaceTypes, refSpaceTypes.data()));

      stageSpaceSupported_ =
          std::any_of(std::begin(refSpaceTypes), std::end(refSpaceTypes), [](const auto &type) {
              return type == XR_REFERENCE_SPACE_TYPE_STAGE;
          });
    }

    VkPhysicalDevice Context::findVkGraphicsDevice(VkInstance vkInstance) {
      PFN_xrGetVulkanGraphicsDeviceKHR pfnGetVulkanGraphicsDeviceKHR = NULL;
      XR_CHECK(xrGetInstanceProcAddr(instance_,
                                     "xrGetVulkanGraphicsDeviceKHR",
                                     (PFN_xrVoidFunction *) (&pfnGetVulkanGraphicsDeviceKHR)));

      VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
      XR_CHECK(
          pfnGetVulkanGraphicsDeviceKHR(instance_, systemId_, vkInstance, &physicalDevice));
      if (physicalDevice == VK_NULL_HANDLE) {
        LOGE("OpenXR: Failed to get vulkan physical device");
      }

      return physicalDevice;
    }

    void Context::initGraphics() {
      PFN_xrGetVulkanGraphicsRequirementsKHR pfnGetVulkanGraphicsRequirementsKHR = NULL;
      XR_CHECK(xrGetInstanceProcAddr(instance_,
                                     "xrGetVulkanGraphicsRequirementsKHR",
                                     (PFN_xrVoidFunction *) (&pfnGetVulkanGraphicsRequirementsKHR)));

      XR_CHECK(pfnGetVulkanGraphicsRequirementsKHR(instance_, systemId_, &graphicsRequirements_));

      // Get required instance extensions
      PFN_xrGetVulkanInstanceExtensionsKHR pfnGetVulkanInstanceExtensionsKHR = NULL;
      XR_CHECK(xrGetInstanceProcAddr(instance_,
                                     "xrGetVulkanInstanceExtensionsKHR",
                                     (PFN_xrVoidFunction *) (&pfnGetVulkanInstanceExtensionsKHR)));

      uint32_t bufferSize = 0;
      XR_CHECK(pfnGetVulkanInstanceExtensionsKHR(instance_, systemId_, 0, &bufferSize, NULL));

      requiredVkInstanceExtensionsBuffer_.resize(bufferSize);
      XR_CHECK(pfnGetVulkanInstanceExtensionsKHR(
          instance_, systemId_, bufferSize, &bufferSize,
          requiredVkInstanceExtensionsBuffer_.data()));
      requiredVkInstanceExtensions_ = processExtensionsBuffer(requiredVkInstanceExtensionsBuffer_);

      /* Additional Vulkan extensions */
      requiredVkInstanceExtensions_.push_back("VK_EXT_debug_utils");

      LOGI("Number of required Vulkan extensions: %zu", requiredVkInstanceExtensions_.size());
      for (const auto &extension: requiredVkInstanceExtensions_) {
        LOGI("\t%s", extension.c_str());
      }

      // Get the required device extensions.
      bufferSize = 0;
      PFN_xrGetVulkanDeviceExtensionsKHR pfnGetVulkanDeviceExtensionsKHR = NULL;
      XR_CHECK(xrGetInstanceProcAddr(instance_,
                                     "xrGetVulkanDeviceExtensionsKHR",
                                     (PFN_xrVoidFunction *) (&pfnGetVulkanDeviceExtensionsKHR)));

      XR_CHECK(pfnGetVulkanDeviceExtensionsKHR(instance_, systemId_, 0, &bufferSize, NULL));

      requiredVkDeviceExtensionsBuffer_.resize(bufferSize);
      XR_CHECK(pfnGetVulkanDeviceExtensionsKHR(
          instance_, systemId_, bufferSize, &bufferSize, requiredVkDeviceExtensionsBuffer_.data()));

      requiredVkDeviceExtensions_ = processExtensionsBuffer(requiredVkDeviceExtensionsBuffer_);

      requiredVkDeviceExtensions_.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
      requiredVkDeviceExtensions_.push_back(VK_KHR_SHADER_NON_SEMANTIC_INFO_EXTENSION_NAME);
      requiredVkDeviceExtensions_.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);
      requiredVkDeviceExtensions_.push_back(VK_EXT_FRAGMENT_DENSITY_MAP_EXTENSION_NAME);
      requiredVkDeviceExtensions_.push_back(VK_QCOM_FRAGMENT_DENSITY_MAP_OFFSET_EXTENSION_NAME);
      requiredVkDeviceExtensions_.push_back(VK_KHR_CREATE_RENDERPASS_2_EXTENSION_NAME);
    }

    const std::vector<std::string> &Context::vkInstanceExtensions() const {
      return requiredVkInstanceExtensions_;
    }

    const std::vector<std::string> &Context::vkDeviceExtensions() const {
      return requiredVkDeviceExtensions_;
    }

    void Context::createSwapchains(VulkanCore::Context &ctx) {
      const uint32_t numSwapchainProviders = useSinglePassStereo_ ? 1 : kNumViews;
      const uint32_t numViewsPerSwapchain = useSinglePassStereo_ ? kNumViews : 1;
      swapchains_.reserve(numSwapchainProviders);

      for (uint32_t i = 0; i < numSwapchainProviders; i++) {
        swapchains_.emplace_back(
            std::make_unique<OXRSwapchain>(ctx,
                                           session_,
                                           viewports_[i],
                                           numViewsPerSwapchain));
        swapchains_.back()->initialize();
      }
    }

    void Context::createSpaces() {
      XrReferenceSpaceCreateInfo spaceCreateInfo = {
          XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
          nullptr,
          XR_REFERENCE_SPACE_TYPE_VIEW,
          {{0, 0, 0, 1.0f}, {10.f, -1.4f, -5.f},},
      };
      XR_CHECK(xrCreateReferenceSpace(session_, &spaceCreateInfo, &headSpace_));

      spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
      XR_CHECK(xrCreateReferenceSpace(session_, &spaceCreateInfo, &localSpace_));

      if (stageSpaceSupported_) {
        spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
        XR_CHECK(xrCreateReferenceSpace(session_, &spaceCreateInfo, &stageSpace_));
      }

      // EYE_GAZE **********************************************************************************
      const XrSessionActionSetsAttachInfo attachInfo{.type = XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO,
          .countActionSets = 1,
          .actionSets = &eyegazeActionSet_,};
      XR_CHECK(xrAttachSessionActionSets(session_, &attachInfo));

      const XrActionSpaceCreateInfo createActionSpaceInfo{.type = XR_TYPE_ACTION_SPACE_CREATE_INFO,
          .action = eyegazeAction_,
          .poseInActionSpace = eyePoseIdentity_,};
      XR_CHECK(xrCreateActionSpace(session_, &createActionSpaceInfo, &gazeActionSpace_));

      const XrReferenceSpaceCreateInfo createReferenceSpaceInfo{.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
          .referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW,
          .poseInReferenceSpace = eyePoseIdentity_,};
      XR_CHECK(xrCreateReferenceSpace(session_, &createReferenceSpaceInfo, &localReferenceSpace_));
    }

    XrFrameState Context::beginFrame() {
      const XrFrameWaitInfo waitFrameInfo = {XR_TYPE_FRAME_WAIT_INFO};
      XrFrameState frameState = {XR_TYPE_FRAME_STATE};
      XR_CHECK(xrWaitFrame(session_, &waitFrameInfo, &frameState));

      XrFrameBeginInfo beginFrameInfo = {XR_TYPE_FRAME_BEGIN_INFO};
      XR_CHECK(xrBeginFrame(session_, &beginFrameInfo));

      XrSpaceLocation loc = {
          loc.type = XR_TYPE_SPACE_LOCATION,
      };
      XR_CHECK(xrLocateSpace(headSpace_, stageSpace_, frameState.predictedDisplayTime, &loc));
      XrPosef headPose = loc.pose;

      XrViewState viewState = {XR_TYPE_VIEW_STATE};
      const XrViewLocateInfo projectionInfo = {
          .type = XR_TYPE_VIEW_LOCATE_INFO,
          .viewConfigurationType = viewConfigProps_.viewConfigurationType,
          .displayTime = frameState.predictedDisplayTime,
          .space = headSpace_,
      };

      uint32_t numViews = views_.size();
      views_[0].type = XR_TYPE_VIEW;
      views_[1].type = XR_TYPE_VIEW;

      XR_CHECK(xrLocateViews(
          session_, &projectionInfo, &viewState, views_.size(), &numViews, views_.data()));

      for (size_t i = 0; i < kNumViews; i++) {
        XrPosef eyePose = views_[i].pose;
        XrPosef_Multiply(&viewStagePoses_[i], &headPose, &eyePose);
        XrPosef viewTransformXrPosef{};
        XrPosef_Invert(&viewTransformXrPosef, &viewStagePoses_[i]);
        XrMatrix4x4f xrMat4{};
        XrMatrix4x4f_CreateFromRigidTransform(&xrMat4, &viewTransformXrPosef);
        viewTransforms_[i] = glm::make_mat4(xrMat4.m);
        cameraPositions_[i] = glm::vec3(eyePose.position.x, eyePose.position.y, eyePose.position.z);
      }

      // EYE_GAZE **********************************************************************************
      if (currentState_ == XR_SESSION_STATE_FOCUSED) {
        XrActiveActionSet activeActionSet{.actionSet = eyegazeActionSet_, .subactionPath = XR_NULL_PATH,};

        const XrActionsSyncInfo syncInfo{.type = XR_TYPE_ACTIONS_SYNC_INFO,
            .countActiveActionSets = 1,
            .activeActionSets = &activeActionSet,};
        XR_CHECK(xrSyncActions(session_, &syncInfo));
        XrActionStatePose actionStatePose{XR_TYPE_ACTION_STATE_POSE};
        const XrActionStateGetInfo getActionStateInfo{.type = XR_TYPE_ACTION_STATE_GET_INFO,
            .action = eyegazeAction_,};
        XR_CHECK(
            xrGetActionStatePose(session_, &getActionStateInfo, &actionStatePose));

        if (actionStatePose.isActive) {
          XrEyeGazeSampleTimeEXT eyeGazeSampleTime{XR_TYPE_EYE_GAZE_SAMPLE_TIME_EXT};
          XrSpaceLocation gazeLocation{XR_TYPE_SPACE_LOCATION, &eyeGazeSampleTime};
          XR_CHECK(
              xrLocateSpace(gazeActionSpace_, localReferenceSpace_, frameState.predictedDisplayTime,
                            &gazeLocation));
          const bool orientationValid =
              gazeLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
          const bool positionValid =
              gazeLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT;
          if (orientationValid && positionValid) {
            eyeGazePositionScreen_[0] = screenCoordinatesFromEyeGazePose(gazeLocation, 0, 0);
            eyeGazePositionScreen_[1] = screenCoordinatesFromEyeGazePose(gazeLocation, 1, 0);
          }
        }
      }

      return frameState;
    }

    glm::vec3
    Context::screenCoordinatesFromEyeGazePose(XrSpaceLocation gazeLocation, int eye, float offset) {
      XrVector3f canonicalViewDirection{0, 0, -1.f};
      gazeLocation.pose.position = {0, 0, 0};
      XrVector3f transformedViewDirection;
      XrPosef_TransformVector3f(&transformedViewDirection, &gazeLocation.pose,
                                &canonicalViewDirection);

      XrMatrix4x4f proj;
      XrMatrix4x4f_CreateProjectionFov(&proj, GRAPHICS_OPENGL, views_[eye].fov, near_,
                                       far_);
      const XrVector4f tanAngle = {-transformedViewDirection.x / transformedViewDirection.z,
                                   -transformedViewDirection.y / transformedViewDirection.z, -1.f,
                                   0};

      const auto width = swapchain(0)->viewport().recommendedImageRectWidth;
      const auto height = swapchain(0)->viewport().recommendedImageRectHeight;

      XrMatrix4x4f scalem;
      XrMatrix4x4f_CreateScale(&scalem, 0.5f, 0.5f, 1.f);
      XrMatrix4x4f biasm;
      XrMatrix4x4f_CreateTranslation(&biasm, 0.5f, 0.5f, 0);
      XrMatrix4x4f rectscalem;
      XrMatrix4x4f_CreateScale(&rectscalem, width, height, 1.f);
      XrMatrix4x4f rectbiasm;
      XrMatrix4x4f_CreateTranslation(&rectbiasm, 0, 0, 0);
      XrMatrix4x4f rectfromclipm;
      XrMatrix4x4f_Multiply(&rectfromclipm, &rectbiasm, &rectscalem);
      XrMatrix4x4f_Multiply(&rectfromclipm, &rectfromclipm, &biasm);
      XrMatrix4x4f_Multiply(&rectfromclipm, &rectfromclipm, &scalem);
      XrMatrix4x4f rectfromeyem;
      XrMatrix4x4f_Multiply(&rectfromeyem, &rectfromclipm, &proj);
      rectfromeyem.m[11] = -1.f;
      XrVector4f texCoords;
      XrMatrix4x4f_TransformVector4f(&texCoords, &rectfromeyem, &tanAngle);

      return glm::vec3(texCoords.x, height - texCoords.y - offset, texCoords.y);
    }

    void Context::endFrame(XrFrameState frameState) {
      std::array<XrCompositionLayerProjectionView, kNumViews> projectionViews;
      std::array<XrCompositionLayerDepthInfoKHR, kNumViews> depthInfos;

      const XrCompositionLayerProjection projection = {
          .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION,
          .next = nullptr,
          .layerFlags = 0,
          .space = stageSpace_,
          .viewCount = static_cast<uint32_t>(kNumViews),
          .views = projectionViews.data(),
      };

      for (size_t i = 0; i < kNumViews; i++) {
        projectionViews[i] = {
            .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW,
            .next = &depthInfos[i],
            .pose = viewStagePoses_[i],
            .fov = views_[i].fov,
        };
        depthInfos[i] = {
            .type = XR_TYPE_COMPOSITION_LAYER_DEPTH_INFO_KHR,
            .next = nullptr,
        };
        const XrRect2Di imageRect = {.offset = {.x = 0, .y = 0},
            .extent = {
                .width = (int32_t) viewports_[i].recommendedImageRectWidth,
                .height = (int32_t) viewports_[i].recommendedImageRectHeight,
            },};
        const auto index = useSinglePassStereo_ ? static_cast<uint32_t>(i) : 0;
        projectionViews[i].subImage = {
            .swapchain = useSinglePassStereo_ ? swapchains_[0]->colorSwapchain_
                                              : swapchains_[i]->colorSwapchain_,
            .imageRect = imageRect,
            .imageArrayIndex =            index,
        };
        depthInfos[i].subImage = {
            .swapchain = useSinglePassStereo_ ? swapchains_[0]->depthSwapchain_
                                              : swapchains_[i]->depthSwapchain_,
            .imageRect = imageRect,
            .imageArrayIndex = index,
        };

        depthInfos[i].minDepth = 0.f;
        depthInfos[i].maxDepth = 1.f;
        depthInfos[i].nearZ = near_;
        depthInfos[i].farZ = far_;
      }

      const XrCompositionLayerBaseHeader *const layers[] = {
          (const XrCompositionLayerBaseHeader *) &projection,
      };

      const XrFrameEndInfo endFrameInfo = {
          .type = XR_TYPE_FRAME_END_INFO,
          .next = nullptr,
          .displayTime = frameState.predictedDisplayTime,
          .environmentBlendMode =          XR_ENVIRONMENT_BLEND_MODE_OPAQUE,
          .layerCount = 1,
          .layers =layers,
      };

      XR_CHECK(xrEndFrame(session_, &endFrameInfo));
    }

    void Context::handleSessionStateChanges(XrSessionState state) {
      currentState_ = state;
      if (state == XR_SESSION_STATE_READY) {
        assert(resumed_);
        // assert(nativeWindow_ != nullptr);
        assert(sessionActive_ == false);

        XrSessionBeginInfo sessionBeginInfo{
            XR_TYPE_SESSION_BEGIN_INFO,
            nullptr,
            viewConfigProps_.viewConfigurationType,
        };

        XR_CHECK(xrBeginSession(session_, &sessionBeginInfo));

        sessionActive_ = true;// (result == XR_SUCCESS);
        LOGI("XR session active");
      } else if (state == XR_SESSION_STATE_STOPPING) {
        assert(resumed_ == false);
        assert(sessionActive_);
        XR_CHECK(xrEndSession(session_));
        sessionActive_ = false;
        LOGI("XR session inactive");
      }
    }

    void Context::handleXrEvents() {
      XrEventDataBuffer eventDataBuffer = {};

      // Poll for events
      for (;;) {
        XrEventDataBaseHeader *baseEventHeader = (XrEventDataBaseHeader *) (&eventDataBuffer);
        baseEventHeader->type = XR_TYPE_EVENT_DATA_BUFFER;
        baseEventHeader->next = nullptr;
        XrResult res;
        res = xrPollEvent(instance_, &eventDataBuffer);
        if (res != XR_SUCCESS) {
          break;
        }
        const auto stateEvent = *reinterpret_cast<const XrEventDataSessionStateChanged *>(&eventDataBuffer);
        LOGI("Current state: %d", stateEvent.state);

        switch (baseEventHeader->type) {
          case XR_TYPE_EVENT_DATA_EVENTS_LOST:
            LOGI("xrPollEvent: received XR_TYPE_EVENT_DATA_EVENTS_LOST event");
            break;
          case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
            LOGI("xrPollEvent: received XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING event");
            break;
          case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
            LOGI("xrPollEvent: received XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED event");
            break;
          case XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT: {
            const XrEventDataPerfSettingsEXT *perf_settings_event =
                (XrEventDataPerfSettingsEXT *) (baseEventHeader);
            (void) perf_settings_event; // suppress unused warning
            LOGI(
                "xrPollEvent: received XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT event: type %d subdomain %d "
                ": level %d -> level %d",
                perf_settings_event->type,
                perf_settings_event->subDomain,
                perf_settings_event->fromLevel,
                perf_settings_event->toLevel);
          }
            break;
          case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
            LOGI("xrPollEvent: received XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING event");
            break;
          case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
            const XrEventDataSessionStateChanged *session_state_changed_event =
                (XrEventDataSessionStateChanged *) (baseEventHeader);
            LOGI(
                "xrPollEvent: received XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: %d for session %p at "
                "time %lld",
                session_state_changed_event->state,
                (void *) session_state_changed_event->session,
                session_state_changed_event->time);

            switch (session_state_changed_event->state) {
              case XR_SESSION_STATE_READY:
              case XR_SESSION_STATE_STOPPING:
              case XR_SESSION_STATE_FOCUSED:
                handleSessionStateChanges(session_state_changed_event->state);
                break;
              default:
                break;
            }
          }
            break;
          default:
            LOGI("xrPollEvent: Unknown event");
            break;
        }
      }
    }

    std::vector<std::string> Context::processExtensionsBuffer(const std::vector<char> &buffer) {
      std::vector<std::string> extensions;
      std::string extension;
      for (auto &ch: buffer) {
        if (ch == '\0' || ch == ' ') {
          extensions.push_back(extension);
          extension.clear();
        } else {
          extension += ch;
        }
      }

      return extensions;
    }

    glm::mat4 Context::mvp(uint32_t i) const {
      XrMatrix4x4f proj;
      XrMatrix4x4f_CreateProjectionFov(&proj, GRAPHICS_OPENGL, views_[i].fov, near_, far_);
      XrMatrix4x4f toView;
      XrVector3f scale{1.f, 1.f, 1.f};
      XrMatrix4x4f_CreateTranslationRotationScale(&toView, &viewStagePoses_[i].position,
                                                  &viewStagePoses_[i].orientation, &scale);
      XrMatrix4x4f view;
      XrMatrix4x4f_InvertRigidBody(&view, &toView);
      XrMatrix4x4f vp;
      XrMatrix4x4f_Multiply(&vp, &proj, &view);

      XrMatrix4x4f model;
      XrVector3f position;
      position.x = 1.f;
      position.y = 1.0f;
      position.z = 1.f;
      XrQuaternionf rotation;
      rotation.x = 0.f;
      rotation.y = 0.f;
      rotation.z = 0.f;
      rotation.w = 1.f;
      XrVector3f scales;
      scales.x = 1.f;
      scales.y = 1.f;
      scales.z = 1.f;
      XrMatrix4x4f_CreateTranslationRotationScale(&model, &position, &rotation, &scales);
      XrMatrix4x4f mvp;
      XrMatrix4x4f_Multiply(&mvp, &vp, &model);

      glm::mat4 result = glm::make_mat4(mvp.m);
      return result;
    }

    glm::mat4 Context::projection(uint32_t i) const {
      XrMatrix4x4f proj;
      XrMatrix4x4f_CreateProjectionFov(&proj, GRAPHICS_OPENGL, views_[i].fov, 0.05f, 100.0f);

      XrMatrix4x4f toView;
      XrVector3f scale{1.f, 1.f, 1.f};
      XrMatrix4x4f_CreateTranslationRotationScale(&toView, &viewStagePoses_[i].position,
                                                  &viewStagePoses_[i].orientation, &scale);
      XrMatrix4x4f view;
      XrMatrix4x4f_InvertRigidBody(&view, &toView);
      XrMatrix4x4f vp;
      XrMatrix4x4f_Multiply(&vp, &proj, &view);

      glm::mat4 result = glm::make_mat4(proj.m);
      // glm::mat4 result = glm::make_mat4(vp.m);
      return result;
    }


} // OXR