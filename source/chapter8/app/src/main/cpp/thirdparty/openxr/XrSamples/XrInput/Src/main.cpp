/*******************************************************************************

Filename    :   Main.cpp
Content     :   OpenXR sample showing use of the input API
Created     :
Authors     :   Andreas Selvik, Eryk Pecyna
Language    :   C++
Copyright:  Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*******************************************************************************/

#include <string>
#include <string_view>
#include <unordered_map>

#include <openxr/openxr.h>
#include <openxr/fb_touch_controller_pro.h>

#include "GUI/VRMenuObject.h"
#include "Render/BitmapFont.h"
#include "Render/GeometryBuilder.h"
#include "Render/GlGeometry.h"
#include "XrApp.h"
#include "ActionSetDisplayPanel.h"
#include "GUI/VRMenuObject.h"

#include "OVR_Math.h"
#include "Input/ControllerRenderer.h"
#include "Input/HandRenderer.h"
#include "Input/TinyUI.h"
#include "Render/GlGeometry.h"
#include "Render/GeometryRenderer.h"
#include "Render/SimpleBeamRenderer.h"
#include "openxr/openxr_oculus_helpers.h"

// All physical units in OpenXR are in meters, but sometimes it's more useful
// to think in cm, so this user defined literal converts from centimeters to meters
constexpr float operator"" _cm(long double centimeters) {
    return static_cast<float>(centimeters * 0.01);
}
constexpr float operator"" _cm(unsigned long long centimeters) {
    return static_cast<float>(centimeters * 0.01);
}

// For expressiveness; use _m rather than f literals when we mean meters
constexpr float operator"" _m(long double meters) {
    return static_cast<float>(meters);
}
constexpr float operator"" _m(unsigned long long meters) {
    return static_cast<float>(meters);
}

class XrInputSampleApp : public OVRFW::XrApp {
   public:
    static constexpr std::string_view kSampleIntroduction =
        "This sample is an introduction to using the OpenXR action system to get input.\n"
        "The OpenXR action system is designed to be adaptable to a wide variety of input\n"
        "devices, including forward compatibility with future devices. This is achieved by\n"
        "using the concept of \"actions\" to drive the application logic, separating out\n"
        "the device specific \"bindings\" of controller input to actions.\n\n"
        "Walking through the code will introduce you to how to use Actions, ActionSets, and\n"
        "Spaces, as well as motivate their design. Playing around in here will allow you to see\n"
        "how exactly actions react to change of active action sets, action set priorities, and\n"
        "multiple bindings.\n\n"
        "Try picking up the tool in front of you and create some art. How about a castle, or\n"
        "a palm tree?\n"
        "Notice how the tool action set is only active while you're holding the cube tool.\n";

    static constexpr std::string_view kSampleInstructions =
        "Pick up the cube tool to start modelling with cubes!\n"
        "Hover your controller over the tool and press the grip button (touch controller)\n"
        "to pick it up. Press it again to drop it. If using hand tracking,\n"
        "pinch the tool to pick it up, and use the menu button to drop it\n"
        "\n"
        "Touch Controller (while tool is held): \n"
        "Trigger (with tool hand):                      Place cube\n"
        "Left Thumbstick:                               Rotate template cube         \n"
        "Right Thumbstick Up/Down:                  Offset template cube         \n"
        "Right Thumbsitck Left/Right:                  Change scale of template cube\n"
        "A button:                                      Change cube color            \n"
        "\n"
        "Tracked hand controls (while tool is held):                                \n"
        "Pinch (with tool hand):                        Place cube                   \n"
        "Distance between hands (off hand pinched): Change scale of template cube  \n";

    XrInputSampleApp() : OVRFW::XrApp() {
        BackgroundColor = OVR::Vector4f(0.55f, 0.35f, 0.1f, 1.0f);

        // Disable framework input management, letting this sample explicitly
        // call xrSyncActions() every frame; which includes control over which
        // ActionSet to set as active
        SkipInputHandling = true;
    }

    // Returns a list of OpenXR extensions requested for this app
    // Note that the sample framework will filter out any extension
    // that is not listed as supported.
    virtual std::vector<const char*> GetExtensions() override {
        std::vector<const char*> extensions = XrApp::GetExtensions();
        extensions.push_back(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
        extensions.push_back(XR_FB_TOUCH_CONTROLLER_PRO_EXTENSION_NAME);
        return extensions;
    }

    std::unordered_map<XrPath, std::vector<XrActionSuggestedBinding>> GetSuggestedBindings(
        XrInstance instance) override {
        OXR(xrStringToPath(Instance, "/user/hand/left", &leftHandPath_));
        OXR(xrStringToPath(Instance, "/user/hand/right", &rightHandPath_));

        // Actions in OpenXR are attached to action sets, which can be thought of as a "context"
        // for when those actions will be available. An application selects which
        // actionsets to enable every frame. For instance, a game might have an actionset
        // for its main world navigation, one for menu interaction, and another for when the player
        // is seated in a helicopter.

        // The OpenXR input system is designed in a way which allows systems to provide
        // highly flexible rebinding solutions, which requires information about the usage of the
        // actions beyond a simple button-focused API.

        // This sample uses three action sets:
        //   - Menu:  For actions used to select and press buttons on the UI panels
        //   - World: The base action set that's always active
        //   - Tool:  For usage of the cube-spawning tool. This actionset is only active
        //            while the user is holding the tool.
        //
        // Note: Action sets have a numerical priority value which is used to resolve conflict
        // on a per-binding action. In this sample the tool action set has a higher priority than
        // the others, which disables the menu interactions while the tool is held.
        // Try changing the priorities! And notice how it impacts the isActive value of actions
        actionSetMenu_ = CreateActionSet(0, "menu_action_set", "UI Action Set");
        actionSetWorld_ = CreateActionSet(0, "world_action_set", "World Action Set");
        actionSetTool_ = CreateActionSet(9, "tool_action_set", "Tool Action Set");

        actionSelect_ = CreateAction(
            actionSetMenu_,
            XR_ACTION_TYPE_BOOLEAN_INPUT,
            "select",
            "Select/Click UI Element" // Displayed to users, should be translated to the user's
                                      // local language
        );

        // If we do not specify subActionPaths, we cannot use them to distinguish input from
        // separate sources later. It is being used here to allow us to bind the same action
        // to both hands while still being able to query the state of a specific hand.
        XrPath bothHands[]{leftHandPath_, rightHandPath_};

        actionGrabRelease_ = CreateAction(
            actionSetWorld_,
            XR_ACTION_TYPE_BOOLEAN_INPUT,
            "activate_tool",
            "Activate Tool",
            2,
            bothHands);

        actionToggleColor_ = CreateAction(
            actionSetTool_, XR_ACTION_TYPE_BOOLEAN_INPUT, "toggle_color", "Change Box Color");

        actionHandsDrop_ =
            CreateAction(actionSetWorld_, XR_ACTION_TYPE_BOOLEAN_INPUT, "drop_tool", "Drop Tool");

        actionSpawnCube_ = CreateAction(
            actionSetTool_, XR_ACTION_TYPE_BOOLEAN_INPUT, "spawn_cube", "Spawn Cube", 2, bothHands);

        actionRotateCube_ = CreateAction(
            actionSetTool_, XR_ACTION_TYPE_VECTOR2F_INPUT, "rotate_cube", "Rotate Cube");

        actionScaleCube_ =
            CreateAction(actionSetTool_, XR_ACTION_TYPE_FLOAT_INPUT, "scale_cube", "Scale Cube");

        actionTranslateCube_ = CreateAction(
            actionSetTool_, XR_ACTION_TYPE_FLOAT_INPUT, "translate_cube", "Translate Cube");

        // All controller interaction profiles in OpenXR defines two separate poses; aim and pose.
        // These are used to get controller position and orientation, and it is important to
        // understand the difference between the two:
        //    - Grip pose is defined to be centered inside the controller aligned with the
        //      center of the users palm. Anything that the user is holding, whether it's a
        //      controller representation or a tomato should use the grip pose. In this sample both
        //      the controller model and tool pose is driven by grip.
        //    - Aim pose is defined to be a system dependent way to get a good origin and direction
        //      of a ray used for pointing and selecting things. Note that this can vary depending
        //      on system conventions and controller geometry, but is the preferred way to draw UI
        //      selection rays, as is done in this sample
        //
        // For a more exact defintion of the grip and aim pose, see "Standard pose identifier" part
        // of the OpenXR 1.0 specification. See also: XR_EXT_palm_pose for use cases where you need
        // to know where the users palm surface is.

        actionMenuBeamPose_ = CreateAction(
            actionSetWorld_,
            XR_ACTION_TYPE_POSE_INPUT,
            "menu_beam_pose",
            "Menu Beam Pose",
            2,
            bothHands);

        actionCubeAimPose_ = CreateAction(
            actionSetTool_,
            XR_ACTION_TYPE_POSE_INPUT,
            "cube_aim_pose",
            "Cube Aim Pose",
            2,
            bothHands);

        actionControllerGripPose_ = CreateAction(
            actionSetWorld_, XR_ACTION_TYPE_POSE_INPUT, "grip_pose", "Grip Pose", 2, bothHands);

        // A few things worth pointing out about these bindings:
        //  - Binding the same action to both hands is not a problem,
        //    since you can use subActionPath later to distinguish them
        //
        //  - actionRotateCube_ gets bound to input/thumbstick rather than ../x and ../y
        //    to get the state as a 2D float vector. While actionScaleCube_ and actionTranslateCube_
        //    gets bound to one specific axis /x and /y, respectively.

        // clang-format off
        // == Bindings for /interaction_profiles/oculus/touch_controller ==
        std::vector<std::pair<XrAction, const char*>> touchBindings{
            {actionSelect_,             "/user/hand/left/input/trigger/value"},
            {actionSpawnCube_,          "/user/hand/left/input/trigger/value"},
            {actionGrabRelease_,        "/user/hand/left/input/squeeze/value"},
            {actionRotateCube_,         "/user/hand/left/input/thumbstick"},
            {actionMenuBeamPose_,       "/user/hand/left/input/aim/pose"},
            {actionCubeAimPose_,        "/user/hand/left/input/aim/pose"},
            {actionControllerGripPose_, "/user/hand/left/input/grip/pose"},

            {actionSelect_,             "/user/hand/right/input/trigger/value"},
            {actionSpawnCube_,          "/user/hand/right/input/trigger/value"},
            {actionGrabRelease_,        "/user/hand/right/input/squeeze/value"},
            {actionScaleCube_,          "/user/hand/right/input/thumbstick/x"},
            {actionTranslateCube_,      "/user/hand/right/input/thumbstick/y"},
            {actionToggleColor_,        "/user/hand/right/input/a/click"},
            {actionMenuBeamPose_,       "/user/hand/right/input/aim/pose"},
            {actionCubeAimPose_,        "/user/hand/right/input/aim/pose"},
            {actionControllerGripPose_, "/user/hand/right/input/grip/pose"}};

        // If the touch controller pro bindings below are dropped, the touch
        // controller will be automatically emulated. Try it for yourself!
        // == Bindings for /interaction_profiles/oculus/touch_controller_pro
        std::vector<std::pair<XrAction, const char*>> touchProBindings{
            {actionSelect_,             "/user/hand/left/input/trigger/value"},
            {actionSpawnCube_,          "/user/hand/left/input/trigger/value"},
            {actionGrabRelease_,        "/user/hand/left/input/squeeze/value"},
            {actionRotateCube_,         "/user/hand/left/input/thumbstick"},
            {actionMenuBeamPose_,       "/user/hand/left/input/aim/pose"},
            {actionCubeAimPose_,        "/user/hand/left/input/aim/pose"},
            {actionControllerGripPose_, "/user/hand/left/input/grip/pose"},

            {actionSelect_,             "/user/hand/right/input/trigger/value"},
            {actionSpawnCube_,          "/user/hand/right/input/trigger/value"},
            {actionGrabRelease_,        "/user/hand/right/input/squeeze/value"},
            {actionScaleCube_,          "/user/hand/right/input/thumbstick/x"},
            {actionTranslateCube_,      "/user/hand/right/input/thumbstick/y"},
            {actionToggleColor_,        "/user/hand/right/input/a/click"},
            {actionMenuBeamPose_,       "/user/hand/right/input/aim/pose"},
            {actionCubeAimPose_,        "/user/hand/right/input/aim/pose"},
            {actionControllerGripPose_, "/user/hand/right/input/grip/pose"}};

        // == Bindings for /interaction_profiles/khr/simple_controller ==
        //
        // While interaction profiles in general map to specific input hardware,
        // khr/simple_controller is a special general purpose interaction profile
        // that most input can bind to, including hand tracking.
        // In this sample these bindings are used to drive hand tracking interactions,
        // but if the touch controller can also use these bindings (try commenting out the
        // touchBindings)
        std::vector<std::pair<XrAction, const char*>> simpleBindings{
            {actionSelect_,             "/user/hand/left/input/select/click"},
            {actionGrabRelease_,        "/user/hand/left/input/select/click"},
            {actionSpawnCube_,          "/user/hand/left/input/select/click"},
            {actionHandsDrop_,          "/user/hand/left/input/menu/click"},
            {actionControllerGripPose_, "/user/hand/left/input/grip/pose"},
            {actionMenuBeamPose_,       "/user/hand/left/input/aim/pose"},
            {actionCubeAimPose_,        "/user/hand/left/input/aim/pose"},

            {actionSelect_,             "/user/hand/right/input/select/click"},
            {actionGrabRelease_,        "/user/hand/right/input/select/click"},
            {actionSpawnCube_,          "/user/hand/right/input/select/click"},
            {actionHandsDrop_,          "/user/hand/right/input/menu/click"},
            {actionControllerGripPose_, "/user/hand/right/input/grip/pose"},
            {actionMenuBeamPose_,       "/user/hand/right/input/aim/pose"},
            {actionCubeAimPose_,        "/user/hand/right/input/aim/pose"}};
        // clang-format on

        OXR(xrStringToPath(
            instance, "/interaction_profiles/oculus/touch_controller", &touchInteractionProfile_));
        OXR(xrStringToPath(
            instance, "/interaction_profiles/khr/simple_controller", &simpleInteractionProfile_));
        OXR(xrStringToPath(
            instance,
            "/interaction_profiles/facebook/touch_controller_pro",
            &touchProInteractionProfile_));

        // Get the default bindings suggested by XrApp framework
        auto suggestedBindings = XrApp::GetSuggestedBindings(instance);

        // Append the binding information to the sample framework specific
        // data structure
        for (const auto& binding : touchBindings) {
            suggestedBindings[touchInteractionProfile_].emplace_back(
                ActionSuggestedBinding(binding.first, binding.second));
        }
        for (const auto& binding : simpleBindings) {
            suggestedBindings[simpleInteractionProfile_].emplace_back(
                ActionSuggestedBinding(binding.first, binding.second));
        }
        for (const auto& binding : touchProBindings) {
            suggestedBindings[touchProInteractionProfile_].emplace_back(
                ActionSuggestedBinding(binding.first, binding.second));
        }

        // The sample framework uses this data structure to call
        // xrSuggestInteractionProfileBindings() for each of the
        // interaction profiles provided.
        //
        // Be sure to pay attention to any error codes returned from
        // xrSuggestInteractionProfileBindings(), as even a single
        // typo in a path will fail the setup for a full interaction profile
        return suggestedBindings;
    }

    // OVRFW::XrApp::Init() calls, among other things;
    //  - xrInitializeLoaderKHR(...)
    //  - xrCreateInstance with the extensions from GetExtensions(...),
    //  - xrSuggestInteractionProfileBindings(...) to set up action bindings
    // before calling the function below; AppInit():
    virtual bool AppInit(const xrJava* context) override {
        /// TinyUI setup
        int fontVertexBufferSize = 32 * 1024; // Custom large text buffer size for all the text
        bool updateColors = true; // Update UI colors on interaction
        if (false == ui_.Init(context, GetFileSys(), updateColors, fontVertexBufferSize)) {
            ALOG("TinyUI::Init FAILED.");
            return false;
        }

        // Even if the runtime supports the hand tracking extension,
        // the actual device might not support hand tracking.
        // Inspect the system properties to find out.
        XrSystemHandTrackingPropertiesEXT handTrackingSystemProperties{
            XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT};
        XrSystemProperties systemProperties{
            XR_TYPE_SYSTEM_PROPERTIES, &handTrackingSystemProperties};
        OXR(xrGetSystemProperties(GetInstance(), GetSystemId(), &systemProperties));
        supportsHandTracking_ = handTrackingSystemProperties.supportsHandTracking;

        if (supportsHandTracking_) {
            /// Hook up extensions for hand tracking
            OXR(xrGetInstanceProcAddr(
                GetInstance(),
                "xrCreateHandTrackerEXT",
                (PFN_xrVoidFunction*)(&xrCreateHandTrackerEXT_)));
            assert(xrCreateHandTrackerEXT_);

            OXR(xrGetInstanceProcAddr(
                GetInstance(),
                "xrDestroyHandTrackerEXT",
                (PFN_xrVoidFunction*)(&xrDestroyHandTrackerEXT_)));
            assert(xrDestroyHandTrackerEXT_);

            OXR(xrGetInstanceProcAddr(
                GetInstance(),
                "xrLocateHandJointsEXT",
                (PFN_xrVoidFunction*)(&xrLocateHandJointsEXT_)));
            assert(xrLocateHandJointsEXT_);
        }

        return true;
    }

    // XrApp::InitSession() calls:
    // - xrCreateSession(...) to create our Session
    // - xrCreateReferenceSpace(...) for local and stage space
    // - Create swapchain with xrCreateSwapchain(...)
    // - xrAttachSessionActionSets(...)
    // before calling SessionInit():
    virtual bool SessionInit() override {
        //  --- Creation of action spaces
        //
        // Pose actions are located by first creating an XrSpace, which can later be used
        // in xrLocateSpace(). Note how subactionPath is used to create two XrSpaces from
        // the same action that's bound to both hands.

        XrActionSpaceCreateInfo actionSpaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        actionSpaceCreateInfo.action = actionMenuBeamPose_;
        actionSpaceCreateInfo.poseInActionSpace = {{0, 0, 0, 1}, {0_m, 0_m, 0_m}}; // Identity Pose
        actionSpaceCreateInfo.subactionPath = leftHandPath_;
        OXR(xrCreateActionSpace(GetSession(), &actionSpaceCreateInfo, &spaceMenuBeamLeft_));
        actionSpaceCreateInfo.subactionPath = rightHandPath_;
        OXR(xrCreateActionSpace(GetSession(), &actionSpaceCreateInfo, &spaceMenuBeamRight_));

        actionSpaceCreateInfo.action = actionCubeAimPose_;
        // Offset the space for creating cubes, a nudge away from the user
        actionSpaceCreateInfo.poseInActionSpace = {{0, 0, 0, 1}, {0_cm, 0_cm, -5_cm}};
        actionSpaceCreateInfo.subactionPath = leftHandPath_;
        OXR(xrCreateActionSpace(GetSession(), &actionSpaceCreateInfo, &spaceCubeAimLeft_));
        actionSpaceCreateInfo.subactionPath = rightHandPath_;
        OXR(xrCreateActionSpace(GetSession(), &actionSpaceCreateInfo, &spaceCubeAimRight_));

        actionSpaceCreateInfo.action = actionControllerGripPose_;
        actionSpaceCreateInfo.poseInActionSpace = {{0, 0, 0, 1}, {0_m, 0_m, 0_m}}; // Identity Pose
        actionSpaceCreateInfo.subactionPath = leftHandPath_;
        OXR(xrCreateActionSpace(GetSession(), &actionSpaceCreateInfo, &spaceGripLeft_));
        actionSpaceCreateInfo.subactionPath = rightHandPath_;
        OXR(xrCreateActionSpace(GetSession(), &actionSpaceCreateInfo, &spaceGripRight_));

        //  --- Creation of reference spaces
        //
        // OpenXR does not provide a concept of "World Space", since different devices
        // provide different types of tracking, which can't garantuee a stable
        // global world space.
        // Instead, OpenXR defines a set of "well-known reference spaces" that can be used
        // for spatial reasoning. The two most common ones are:
        //    - LOCAL: Guaranteed to be available. Origin is set at user eye-height, and
        //      can be recentered by the user at will. This changes both rotation (gravity locked)
        //      and moves the origin to the current user head location.
        //    - STAGE: This space is locked to the real world, with the origin at floor level. It is
        //      not affected by user recenter events. On Quest it is tied to the guardian
        //      definition. However, it is not guaranteed to exist on all OpenXR systems, as it
        //      requires 6DOF tracking
        //
        //  This sample uses LOCAL for easy recentering, but feel free to try changing it!
        //
        // See the Spaces chapter in the OpenXR specification for more details

        XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        referenceSpaceCreateInfo.poseInReferenceSpace = {{0, 0, 0, 1}, {0_m, 0_m, 0_m}}; // Identity
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        OXR(xrCreateReferenceSpace(GetSession(), &referenceSpaceCreateInfo, &spaceLocal_));

        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
        OXR(xrCreateReferenceSpace(GetSession(), &referenceSpaceCreateInfo, &spaceStage_));

        // Try switching this to see the difference between local and stage
        mainReferenceSpace_ = spaceLocal_;

        // Make sure the sample framework is set to the correct space as well
        CurrentSpace = mainReferenceSpace_;

        //  --- Attach ActionSets to session
        //
        // This is required before any call to xrSyncActions for these action sets
        // and can only be done once. This mechanism ensures immutability of actions and actionsets
        // used for a session, which allows runtimes to know the whole set of actions up-front for
        // rebinding purposes.
        std::vector<XrActionSet> actionSets{{actionSetWorld_, actionSetMenu_, actionSetTool_}};
        XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
        attachInfo.countActionSets = actionSets.size();
        attachInfo.actionSets = actionSets.data();
        OXR(xrAttachSessionActionSets(Session, &attachInfo));
        // After this point all actions and bindings are final for the session
        // (calls to xrSuggestInteractionProfileBindings and xrAttachSessionActionSets fail)

        // --- Hand rendering setup
        if (supportsHandTracking_) {
            SetupHandTrackers();
        }

        // --- Create the model for the cube-spawning tool
        OVRFW::GeometryBuilder toolGeometry;

        OVR::Vector4f toolColor = {0.1f, 0.1f, 0.1f, 1.0f};

        std::vector<OVR::Matrix4f> toolElementTransforms{
            OVR::Matrix4f::Scaling(0.05f, 0.12f, 0.05f),
            OVR::Matrix4f::RotationZ(OVR::DegreeToRad(-45.f)) *
                OVR::Matrix4f::Translation(6_cm, -2_cm, 0_cm) *
                OVR::Matrix4f::Scaling(0.075f, 0.05f, 0.05f),
            OVR::Matrix4f::RotationZ(OVR::DegreeToRad(45.f)) *
                OVR::Matrix4f::Translation(-6_cm, -2_cm, 0_cm) *
                OVR::Matrix4f::Scaling(0.075f, 0.05f, 0.05f),
            OVR::Matrix4f::Translation(-5.5_cm, -9_cm, 0_cm) *
                OVR::Matrix4f::Scaling(0.025f, 0.05f, 0.025f),
            OVR::Matrix4f::Translation(5.5_cm, -9_cm, 0_cm) *
                OVR::Matrix4f::Scaling(0.025f, 0.05f, 0.025f),
            OVR::Matrix4f::RotationX(OVR::DegreeToRad(45.f)) *
                OVR::Matrix4f::Scaling(0.05f, 0.05f, 0.05f),
            OVR::Matrix4f::RotationZ(OVR::DegreeToRad(45.f)) *
                OVR::Matrix4f::Scaling(0.05f, 0.05f, 0.05f),
            OVR::Matrix4f::Translation(0_cm, -3_cm, 0_cm) *
                OVR::Matrix4f::RotationX(OVR::DegreeToRad(45.f)) *
                OVR::Matrix4f::Scaling(0.05f, 0.05f, 0.05f),
            OVR::Matrix4f::Translation(0_cm, 3_cm, 0_cm) *
                OVR::Matrix4f::RotationX(OVR::DegreeToRad(45.f)) *
                OVR::Matrix4f::Scaling(0.05f, 0.05f, 0.05f),
            OVR::Matrix4f::Translation(0_cm, 3_cm, 0_cm) *
                OVR::Matrix4f::RotationZ(OVR::DegreeToRad(45.f)) *
                OVR::Matrix4f::Scaling(0.05f, 0.05f, 0.05f),
        };

        for (auto transform : toolElementTransforms) {
            // Slight adjustment to make the tool point at the cube
            transform = OVR::Matrix4f::Translation(0_cm, 0_cm, -3.5_cm) *
                (OVR::Matrix4f::RotationX(OVR::DegreeToRad(30.f)) * transform);
            toolGeometry.Add(
                OVRFW::BuildUnitCubeDescriptor(),
                OVRFW::GeometryBuilder::kInvalidIndex,
                toolColor,
                transform);
        }

        toolRenderer_.Init(toolGeometry.ToGeometryDescriptor());
        toolRenderer_.SetPose(OVR::Posef(OVR::Quat<float>::Identity(), {0_m, -0.3_m, -0.5_m}));

        // Display a translucent version of the cube before it is placed
        OVRFW::GeometryBuilder templateCubeGeometry;
        templateCubeGeometry.Add(
            OVRFW::BuildUnitCubeDescriptor(),
            OVRFW::GeometryBuilder::kInvalidIndex,
            colorOptions_[cubeColorIndex_]);
        templateCubeRenderer_.ChannelControl = OVR::Vector4f(1, 1, 1, 0.8);
        templateCubeRenderer_.Init(templateCubeGeometry.ToGeometryDescriptor());
        templateCubeRenderer_.SetPose(
            OVR::Posef(OVR::Quat<float>::Identity(), {0_m, -0.3_m, -0.65_m}));

        // Scale to 5cm cube
        templateCubeRenderer_.SetScale({5_cm, 5_cm, 5_cm});

        //  --- UI setup
        CreateSampleDescriptionPanel();
        SetupActionUIPanels();
        SetupMenuPanels();

        /// Disable scene navitgation
        GetScene().SetFootPos({0.0_m, 0.0_m, 0.0_m});
        this->FreeMove = false;

        if (false == controllerRenderL_.Init(true)) {
            ALOG("SessionInit::Init L controller renderer FAILED.");
            return false;
        }

        if (false == controllerRenderR_.Init(false)) {
            ALOG("SessionInit::Init R controller renderer FAILED.");
            return false;
        }

        cursorBeamRenderer_.Init(GetFileSys(), nullptr, OVR::Vector4f(1.0f), 1.0f);

        return true;
    }

    // The update function is called every frame before the Render() function.
    // Some of the key OpenXR function called by the framework prior to calling this function:
    //  - xrPollEvent(...)
    //  - xrWaitFrame(...)
    virtual void Update(const OVRFW::ovrApplFrameIn& in) override {
        //
        // --- xrSyncAction
        //

        std::vector<XrActiveActionSet> activeActionSets = {{actionSetWorld_}, {actionSetMenu_}};

        // Only activate the tool action set while the tool is being held
        // This is the mechanism that makes the trigger button only spawn cubes
        // while the tool is held
        if (toolHeldInLeft_ || toolHeldInRight_) {
            activeActionSets.push_back({actionSetTool_});
        }

        // xrSyncAction updates the state of all the input at once
        // and subsequent calls to xrGetActionState* just retrieves
        // the state that was synced during this call. This is important
        // to ensure that the state during a frame is consistent. For instance
        // if you call xrGetActionStateBoolean(myAction) twice between
        // calls to xrSyncActions, they are guaranteed to return the same
        // data.
        XrActionsSyncInfo syncInfo = {XR_TYPE_ACTIONS_SYNC_INFO};
        syncInfo.countActiveActionSets = activeActionSets.size();
        syncInfo.activeActionSets = activeActionSets.data();
        OXR(xrSyncActions(Session, &syncInfo));

        // The hit test devices are rays used for hit detection in the UI.
        // Clear the rays from last frame
        ui_.HitTestDevices().clear();

        if (supportsHandTracking_) {
            UpdateHands(in.PredictedDisplayTime);
        }

        //
        // --- Locate controller grip and aim poses
        //

        // DisplayTime is the time returned by the latest xrWaitFrame() call.
        // It's the time when the current frame is expected to be shown to the user.
        // xrLocateSpace returns a prediction of where these spaces spaces will be at that
        // future time.
        // IMPORTANT: Make sure the correct time is passed to xrLocateSpace, otherwise
        // there will be additional latency
        XrTime time = ToXrTime(in.PredictedDisplayTime);
        OXR(xrLocateSpace(spaceGripRight_, mainReferenceSpace_, time, &locationGripRight_));
        OXR(xrLocateSpace(spaceGripLeft_, mainReferenceSpace_, time, &locationGripLeft_));
        OXR(xrLocateSpace(spaceMenuBeamLeft_, mainReferenceSpace_, time, &locationMenuBeamLeft_));
        OXR(xrLocateSpace(spaceMenuBeamRight_, mainReferenceSpace_, time, &locationMenuBeamRight_));
        OXR(xrLocateSpace(spaceCubeAimLeft_, mainReferenceSpace_, time, &locationCubeAimLeft_));
        OXR(xrLocateSpace(spaceCubeAimRight_, mainReferenceSpace_, time, &locationCubeAimRight_));

        // It is also possible to use xrLocateSpace between action spaces
        XrSpaceLocation locationGripRelative{XR_TYPE_SPACE_LOCATION};
        OXR(xrLocateSpace(spaceGripRight_, spaceGripLeft_, time, &locationGripRelative));
        distBetweenHands_ = FromXrPosef(locationGripRelative.pose).Translation.Length();

        // Get current interaction profile to adapt behavior to simple controller
        XrInteractionProfileState ipState{XR_TYPE_INTERACTION_PROFILE_STATE};
        OXR(xrGetCurrentInteractionProfile(GetSession(), leftHandPath_, &ipState));
        auto currentInteractionProfile = ipState.interactionProfile;
        OXR(xrGetCurrentInteractionProfile(GetSession(), rightHandPath_, &ipState));
        currentInteractionProfile = (currentInteractionProfile == XR_NULL_PATH)
            ? ipState.interactionProfile
            : currentInteractionProfile;

        //
        // --- Picking up and dropping the tool
        //
        auto grabState = GetActionStateBoolean(actionGrabRelease_);
        auto dropState = GetActionStateBoolean(actionHandsDrop_);

        // We are allowed to specifically query the rightHand XrPath for this action only
        // because it was listed under subActionPaths when actionGrabRelease_ was created. This
        // lets us differentiate between the different possible inputs that could have caused this
        // action.
        auto grabbedRight = GetActionStateBoolean(actionGrabRelease_, rightHandPath_).currentState;

        // Since changedSinceLastSync is only true for a single frame after
        // a boolean action has changed, it is a useful way to detect
        // the "rising edge of the signal", that is the first frame after
        // the state has changed.

        // Detect rising edge of grabState or DropState
        if ((grabState.changedSinceLastSync && grabState.currentState) ||
            (dropState.changedSinceLastSync && dropState.currentState)) {
            // If holding the tool, drop it if using the correct hand to drop
            // Sepcifically only allow dropping by the hand holding the tool
            if (toolHeldInLeft_ || toolHeldInRight_) {
                // Special case for simple controller (limited inputs)
                // to allow dropping it from the off hand.
                // (On quest right hand menu action is used as a system gesture)
                if (currentInteractionProfile == simpleInteractionProfile_) {
                    if (dropState.currentState) {
                        toolHeldInLeft_ = toolHeldInRight_ = false;
                    }
                } else {
                    if (toolHeldInLeft_ && !grabbedRight) {
                        toolHeldInLeft_ = false;
                    }
                    if (toolHeldInRight_ && grabbedRight) {
                        toolHeldInRight_ = false;
                    }
                }
            } else {
                // Tool not held so pick up if it is close to hand
                OVR::Posef toolPos = toolRenderer_.GetPose();
                auto& grabberGripLocation = grabbedRight ? locationGripRight_ : locationGripLeft_;
                if (toolPos.Translation.Distance(
                        FromXrPosef(grabberGripLocation.pose).Translation) < toolHitBox_) {
                    toolHeldInRight_ = grabbedRight;
                    toolHeldInLeft_ = !grabbedRight;
                }
            }
        }

        //
        // --- Update location of the tool when held
        //
        if (toolHeldInLeft_ || toolHeldInRight_) {
            auto xrToolPose = toolHeldInRight_ ? locationGripRight_.pose : locationGripLeft_.pose;
            xrToolPose.orientation = toolHeldInRight_ ? locationCubeAimRight_.pose.orientation
                                                      : locationCubeAimLeft_.pose.orientation;
            auto toolPose = FromXrPosef(xrToolPose);

                        // This is a 60 degree rotation around the X-axis of the aim pose to make the
            // tool point towards the template cube
            toolPose.Rotation *= OVR::Quatf({OVR::DegreeToRad(60.f), 0.f, 0.f}, 1.f);
            toolRenderer_.SetPose(toolPose);

            auto templatePose = FromXrPosef(
                toolHeldInRight_ ? locationCubeAimRight_.pose : locationCubeAimLeft_.pose);

            // The aim pose is defined with the Y axis pointing up, and -Z pointing away from
            // the controller.
            templatePose.Translation = templatePose.Transform({0_cm, 0_cm, -templateCubeOffset_});
            templatePose.Rotation *= templateCubeRotation_;
            templateCubeRenderer_.SetPose(templatePose);
            templateCubeRenderer_.SetScale(templateCubeScale_ * OVR::Vector3f{1.0f, 1.0f, 1.0f});
        }
        // Call the update method on the renderer to update the model matrix
        toolRenderer_.Update();

        //
        // --- Spawn cubes!
        //
        auto spawnLeftState = GetActionStateBoolean(actionSpawnCube_, leftHandPath_);
        auto spawnRightState = GetActionStateBoolean(actionSpawnCube_, rightHandPath_);

        // Detect spawn action rising edge from the hand holding the tool
        if ((toolHeldInLeft_ && spawnLeftState.changedSinceLastSync &&
             spawnLeftState.currentState) ||
            (toolHeldInRight_ && spawnRightState.changedSinceLastSync &&
             spawnRightState.currentState)) {
            auto transform = OVR::Matrix4f(templateCubeRenderer_.GetPose()) *
                OVR::Matrix4f::Scaling(templateCubeRenderer_.GetScale());

            cubeGeometry_.Add(
                OVRFW::BuildUnitCubeDescriptor(),
                OVRFW::GeometryBuilder::kInvalidIndex,
                colorOptions_[cubeColorIndex_],
                transform);

            cubeRenderer_.Init(cubeGeometry_.ToGeometryDescriptor());
        }
        // Update matrices!
        cubeRenderer_.Update();

        //
        // --- Change cube color
        //
        auto toggleColorState = GetActionStateBoolean(actionToggleColor_);
        if (toggleColorState.changedSinceLastSync && toggleColorState.currentState) {
            cubeColorIndex_ = (cubeColorIndex_ + 1) % colorOptions_.size();

            OVRFW::GeometryBuilder templateCubeGeometry;
            auto templatePose = templateCubeRenderer_.GetPose();
            templateCubeGeometry.Add(
                OVRFW::BuildUnitCubeDescriptor(),
                OVRFW::GeometryBuilder::kInvalidIndex,
                colorOptions_[cubeColorIndex_]);
            templateCubeRenderer_.Init(templateCubeGeometry.ToGeometryDescriptor());
            templateCubeRenderer_.SetPose(templatePose);
            templateCubeRenderer_.SetScale(templateCubeScale_ * OVR::Vector3f{1.0f, 1.0f, 1.0f});
        }
        templateCubeRenderer_.Update();

        // Using the current interaction profile to change behavior is a common technique.
        // In this case we're introducing an alternate behavior for control schemes
        // that lack a thumbstick (hand tracking!) and we instead use the
        // distance between the hands to scale the cube.
        //
        // Note that xrGetCurrentInteractionProfile() is guaranteed to only return interaction
        // profiles for which the app has suggested bindings for (or XR_NULL_PATH), so it can
        // safely be used to change behavior.
        //
        // (Be aware that the actual controller being used might not correspond to the
        // interaction profile, for compatibility reasons. For instance, a Quest Pro controller will
        // "pretend" to be a Quest controller if the app only have bindings for Quest)
        //
        if (currentInteractionProfile == simpleInteractionProfile_) {
            // Detect press or release of the offhand spawn action
            if ((toolHeldInLeft_ && spawnRightState.changedSinceLastSync) ||
                (toolHeldInRight_ && spawnLeftState.changedSinceLastSync)) {
                if ((toolHeldInLeft_ && spawnRightState.currentState) ||
                    (toolHeldInRight_ && spawnLeftState.currentState)) {
                    currentlyScalingTemplate_ = true;
                    oldTemplateCubeScale_ = templateCubeScale_;
                    startingScalingDistance_ = distBetweenHands_;
                }
                if ((toolHeldInLeft_ && !spawnRightState.currentState) ||
                    (toolHeldInRight_ && !spawnLeftState.currentState)) {
                    currentlyScalingTemplate_ = false;
                }
            }
            if (!toolHeldInLeft_ && !toolHeldInRight_) {
                currentlyScalingTemplate_ = false;
            }
        }

        //
        // --- Rotate, scale and move the cube template
        //
        auto cubeRotateState = GetActionStateVector2(actionRotateCube_);
        auto cubeTranslateState = GetActionStateFloat(actionTranslateCube_);
        auto cubeScaleState = GetActionStateFloat(actionScaleCube_);

        float deltaCubeOffset =
            cubeTranslateState.isActive ? cubeTranslateState.currentState : 0.0_m;
        float deltaCubeScale = cubeScaleState.isActive ? cubeScaleState.currentState : 0.0f;
        float manualCubeScale = 0.0f;

        if (currentlyScalingTemplate_) {
            manualCubeScale = distBetweenHands_ - startingScalingDistance_;
            templateCubeScale_ = oldTemplateCubeScale_ + manualCubeScale;
        }

        // Only scale or translate cube at once, pick the action with largest magnitude
        if (abs(deltaCubeOffset) > abs(deltaCubeScale)) {
            templateCubeOffset_ += 1.5f * in.DeltaSeconds * deltaCubeOffset;
        } else {
            templateCubeScale_ += in.DeltaSeconds * deltaCubeScale;
        }

        // Clamp cube offset and scale
        if (templateCubeOffset_ > kMaxTemplateCubeOffset_) {
            templateCubeOffset_ = kMaxTemplateCubeOffset_;
        }
        if (templateCubeOffset_ < kMinTemplateCubeOffset_) {
            templateCubeOffset_ = kMinTemplateCubeOffset_;
        }
        if (templateCubeScale_ > kMaxTemplateCubeScale_) {
            templateCubeScale_ = kMaxTemplateCubeScale_;
        }
        if (templateCubeScale_ < kMinTemplateCubeScale_) {
            templateCubeScale_ = kMinTemplateCubeScale_;
        }

        if (cubeRotateState.isActive) {
            // Quaternion magic! (R.Conj() * new_rotation * R) gives us a small rotation
            // relative to the current tool space. Multiplying it back into templateCubeRotation
            // to accumulate the rotation.
            templateCubeRotation_ *=
                templateCubeRotation_.Conj() *
                OVR::Quatf::FromRotationVector(
                    2.5f * in.DeltaSeconds * // multiply by delta frametime for consistent
                                             // rotation speed
                    OVR::Vector3f{
                        -cubeRotateState.currentState.y, cubeRotateState.currentState.x, 0.0f}) *
                templateCubeRotation_;
            templateCubeRotation_.Normalize();
        }

        // Check validity of grip location before updating controllers with new location
        // All apps rendering controllers should do this, otherwise you draw floating
        // controllers in cases where tracking is lost or where there's a system menu on top
        // taking input focus
        if ((locationGripLeft_.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
            !toolHeldInLeft_) {
            controllerRenderL_.Update(FromXrPosef(locationGripLeft_.pose));
        }
        if ((locationGripRight_.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
            !toolHeldInRight_) {
            controllerRenderR_.Update(FromXrPosef(locationGripRight_.pose));
        }

        // Note that these flags will be forced to 0 when the tool action set is active
        // due to the collision with actionCubeAimPose_ and the higher priority of the
        // tool action set.
        bool menuBeamActiveLeft = ActionPoseIsActive(actionMenuBeamPose_, rightHandPath_);
        if (menuBeamActiveLeft &&
            (locationMenuBeamLeft_.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) &&
            (locationMenuBeamLeft_.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) {
            // Add new UI hit detection ray based on the aim pose (not grip!)
            bool click = GetActionStateBoolean(actionSelect_, leftHandPath_).currentState;
            ui_.AddHitTestRay(FromXrPosef(locationMenuBeamLeft_.pose), click);
        }

        bool menuBeamActiveRight = ActionPoseIsActive(actionMenuBeamPose_, rightHandPath_);
        if (menuBeamActiveRight &&
            (locationMenuBeamRight_.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) &&
            (locationMenuBeamRight_.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) {
            bool click = GetActionStateBoolean(actionSelect_, rightHandPath_).currentState;
            ui_.AddHitTestRay(FromXrPosef(locationMenuBeamRight_.pose), click);
        }

        cursorBeamRenderer_.Update(in, ui_.HitTestDevices());

        UpdateUI(in);
    }

    // Utility function to split out the UI updates
    void UpdateUI(const OVRFW::ovrApplFrameIn& in) {
        // Update all the action panels
        for (auto& panelPair : actionSetPanels_) {
            panelPair.second.Update();
        }

        boxCountLabel_->SetText("%d boxes placed.", cubeGeometry_.Nodes().size());
        boxColorLabel_->SetText("Box Color: %s", colorNames_[cubeColorIndex_].c_str());

        //
        //   Update current interaction profile display
        //
        std::string leftInteractionProfileString = "XR_NULL_PATH";
        XrInteractionProfileState ipState{XR_TYPE_INTERACTION_PROFILE_STATE};
        OXR(xrGetCurrentInteractionProfile(GetSession(), leftHandPath_, &ipState));
        if (ipState.interactionProfile != XR_NULL_PATH) {
            char buf[XR_MAX_PATH_LENGTH];
            uint32_t outLength = 0;
            OXR(xrPathToString(
                GetInstance(), ipState.interactionProfile, XR_MAX_PATH_LENGTH, &outLength, buf));
            leftInteractionProfileString = std::string(buf);
        }

        std::string rightInteractionProfileString = "XR_NULL_PATH";
        OXR(xrGetCurrentInteractionProfile(GetSession(), rightHandPath_, &ipState));
        if (ipState.interactionProfile != XR_NULL_PATH) {
            char buf[XR_MAX_PATH_LENGTH];
            uint32_t outLength = 0;
            OXR(xrPathToString(
                GetInstance(), ipState.interactionProfile, XR_MAX_PATH_LENGTH, &outLength, buf));
            rightInteractionProfileString = std::string(buf);
        }

        currentInteractionProfileText_->SetText(
            "xrGetCurrentInteractionProfile(...):\n"
            "/user/hand/left: %s\n"
            "/user/hand/right: %s\n",
            leftInteractionProfileString.c_str(),
            rightInteractionProfileString.c_str());

        ui_.Update(in);
    }

    // Called by the XrApp framework after the Update function
    virtual void Render(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out) override {
        XrInteractionProfileState ipState{XR_TYPE_INTERACTION_PROFILE_STATE};
        OXR(xrGetCurrentInteractionProfile(GetSession(), leftHandPath_, &ipState));
        auto currentInteractionProfile = ipState.interactionProfile;
        OXR(xrGetCurrentInteractionProfile(GetSession(), rightHandPath_, &ipState));
        currentInteractionProfile = (currentInteractionProfile == XR_NULL_PATH)
            ? ipState.interactionProfile
            : currentInteractionProfile;

        ui_.Render(in, out);
        toolRenderer_.Render(out.Surfaces);
        cubeRenderer_.Render(out.Surfaces);
        templateCubeRenderer_.Render(out.Surfaces);

        // Check validity of grip location before updating controllers with new location
        // All apps rendering controllers should do this, otherwise you draw floating
        // controllers in cases where tracking is lost or where there's a system menu on top
        // taking input focus
        if ((locationGripLeft_.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0) {
            // Only render tool, not controller, while held
            // And don't render the controller if the hand is tracked
            if (!toolHeldInLeft_ && !handTrackedL_) {
                controllerRenderL_.Render(out.Surfaces);
            }
        }

        if ((locationGripRight_.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0) {
            if (!toolHeldInRight_ && !handTrackedR_) {
                controllerRenderR_.Render(out.Surfaces);
            }
        }

        if (supportsHandTracking_) {
            for (int i = 0; i < XR_HAND_JOINT_COUNT_EXT; ++i) {
                if (handTrackedR_ && !toolHeldInRight_) {
                    handJointRenderersR_[i].Render(out.Surfaces);
                }
                if (handTrackedL_ && !toolHeldInLeft_) {
                    handJointRenderersL_[i].Render(out.Surfaces);
                }
            }
        }

        /// Render beams last, since they render with transparency (alpha blending)
        cursorBeamRenderer_.Render(in, out);
    }

    virtual void SessionEnd() override {
        controllerRenderL_.Shutdown();
        controllerRenderR_.Shutdown();
        cursorBeamRenderer_.Shutdown();
        toolRenderer_.Shutdown();
        cubeRenderer_.Shutdown();
        templateCubeRenderer_.Shutdown();

        if (supportsHandTracking_) {
            /// Hand Trackers
            OXR(xrDestroyHandTrackerEXT_(handTrackerL_));
            OXR(xrDestroyHandTrackerEXT_(handTrackerR_));
            for (int i = 0; i < XR_HAND_JOINT_COUNT_EXT; ++i) {
                handJointRenderersR_[i].Shutdown();
                handJointRenderersR_[i].Shutdown();
            }
        }
    }

    virtual void AppShutdown(const xrJava* context) override {
        /// Unhook extensions for hand tracking
        xrCreateHandTrackerEXT_ = nullptr;
        xrDestroyHandTrackerEXT_ = nullptr;
        xrLocateHandJointsEXT_ = nullptr;

        OVRFW::XrApp::AppShutdown(context);
        ui_.Shutdown();
    }

    void SetupHandTrackers() {
        XrHandTrackerCreateInfoEXT createInfo{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
        createInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
        createInfo.hand = XR_HAND_LEFT_EXT;
        OXR(xrCreateHandTrackerEXT_(GetSession(), &createInfo, &handTrackerL_));
        createInfo.hand = XR_HAND_RIGHT_EXT;
        OXR(xrCreateHandTrackerEXT_(GetSession(), &createInfo, &handTrackerR_));

        for (int handIndex = 0; handIndex < 2; ++handIndex) {
            /// Alias everything for initialization
            const bool isLeft = (handIndex == 0);
            auto& handJointRenderers = isLeft ? handJointRenderersL_ : handJointRenderersR_;
            handJointRenderers.resize(XR_HAND_JOINT_COUNT_EXT);
            for (int i = 0; i < XR_HAND_JOINT_COUNT_EXT; ++i) {
                OVRFW::GeometryRenderer& gr = handJointRenderers[i];
                gr.Init(OVRFW::BuildUnitCubeDescriptor());
                gr.SetScale({0.01f, 0.01f, 0.01f});
                gr.DiffuseColor = jointColor_;
            }
        }
    }

    void UpdateHands(const double predictedDisplayTime) {
        for (int handIndex = 0; handIndex < 2; ++handIndex) {
            const bool isLeft = (handIndex == 0);
            auto& handJointRenderers = isLeft ? handJointRenderersL_ : handJointRenderersR_;
            auto& handTracker = isLeft ? handTrackerL_ : handTrackerR_;
            auto& handTracked = isLeft ? handTrackedL_ : handTrackedR_;
            auto* jointLocations = isLeft ? jointLocationsL_ : jointLocationsR_;

            XrHandJointLocationsEXT locations{XR_TYPE_HAND_JOINT_LOCATIONS_EXT};
            locations.jointCount = XR_HAND_JOINT_COUNT_EXT;
            locations.jointLocations = jointLocations;

            XrHandJointsLocateInfoEXT locateInfo{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT};
            locateInfo.baseSpace = mainReferenceSpace_;
            locateInfo.time = ToXrTime(predictedDisplayTime);

            OXR(xrLocateHandJointsEXT_(handTracker, &locateInfo, &locations));

            handTracked = false;
            if (locations.isActive) {
                handTracked = true;
                for (uint32_t i = 0; i < locations.jointCount; ++i) {
                    OVRFW::GeometryRenderer& gr = handJointRenderers[i];
                    if ((jointLocations[i].locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) &&
                        (jointLocations[i].locationFlags &
                         XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)) {
                        const auto p = FromXrPosef(jointLocations[i].pose);
                        gr.SetPose(p);
                        gr.Update();
                    }
                }
            }
        }
    }

    void SetupActionUIPanels() { // Setup all the UI panels to display the state of each action
        // Sets up the UI panels that displays the state of all the actions
        // See ActionSetDisplayPanel.cpp for the implementation

        // Action sets
        actionSetPanels_.insert(
            {actionSetMenu_,
             ActionSetDisplayPanel(
                 "Menu Action Set", Session, Instance, &ui_, {-2.0_m, 1.0_m, -2.5_m})});

        actionSetPanels_.insert(
            {actionSetWorld_,
             ActionSetDisplayPanel(
                 "World Action Set", Session, Instance, &ui_, {-0.5_m, 1.0_m, -2.5_m})});

        actionSetPanels_.insert(
            {actionSetTool_,
             ActionSetDisplayPanel(
                 "Tool Action Set", Session, Instance, &ui_, {1.0_m, 1.0_m, -2.5_m})});

        // Menu actions
        actionSetPanels_.at(actionSetMenu_).AddBoolAction(actionSelect_, "Select");
        actionSetPanels_.at(actionSetMenu_).AddPoseAction(actionMenuBeamPose_, "Menu Beam Pose");

        // World actions
        actionSetPanels_.at(actionSetWorld_).AddBoolAction(actionGrabRelease_, "Grab/Release");
        actionSetPanels_.at(actionSetWorld_).AddBoolAction(actionHandsDrop_, "Drop (hands)");
        actionSetPanels_.at(actionSetWorld_).AddPoseAction(actionControllerGripPose_, "Grip Pose");

        // Tool actions
        actionSetPanels_.at(actionSetTool_).AddPoseAction(actionCubeAimPose_, "Cube Aim Pose");
        actionSetPanels_.at(actionSetTool_).AddBoolAction(actionSpawnCube_, "Spawn");
        actionSetPanels_.at(actionSetTool_).AddBoolAction(actionToggleColor_, "Toggle Color");
        actionSetPanels_.at(actionSetTool_).AddVec2Action(actionRotateCube_, "Rotate");
        actionSetPanels_.at(actionSetTool_).AddFloatAction(actionScaleCube_, "Scale");
        actionSetPanels_.at(actionSetTool_).AddFloatAction(actionTranslateCube_, "Translate");
    }

    void SetupMenuPanels() { // Setup all the UI panels to display the state of each action
        currentInteractionProfileText_ = ui_.AddLabel(
            "xrGetCurrentInteractionProfile(...): \n/user/hand/left: N/A \n/user/hand/right: N/A",
            {3.0_m, 0.1_m, -1.5_m},
            {850.0f, 120.0f});

        boxCountLabel_ = ui_.AddLabel("0 cubes placed.", {3.0_m, -0.1_m, -1.5_m}, {450.0f, 45.0f});
        boxColorLabel_ = ui_.AddLabel("Box Color: Red", {3.0_m, -0.2_m, -1.5_m}, {450.0f, 45.0f});

        auto button = ui_.AddButton(
            "Clear placed cubes", {3.0_m, -0.315_m, -1.5_m}, {450.0f, 60.0f}, [this]() {
                this->cubeGeometry_.clear_nodes();
                this->cubeRenderer_.Init(this->cubeGeometry_.ToGeometryDescriptor());
                this->boxCountLabel_->SetText("0 cubes placed.");
            });

        // Tilt the interaction UI towards user
        currentInteractionProfileText_->SetLocalRotation(
            OVR::Quat<float>::FromRotationVector({0, OVR::DegreeToRad(-60.0f), 0}));
        boxCountLabel_->SetLocalRotation(
            OVR::Quat<float>::FromRotationVector({0, OVR::DegreeToRad(-60.0f), 0}));
        button->SetLocalRotation(
            OVR::Quat<float>::FromRotationVector({0, OVR::DegreeToRad(-60.0f), 0}));
        boxColorLabel_->SetLocalRotation(
            OVR::Quat<float>::FromRotationVector({0, OVR::DegreeToRad(-60.0f), 0}));
    }

    void CreateSampleDescriptionPanel() {
        // Panel to provide sample description to the user for context
        auto titleLabel = ui_.AddLabel("XrInput Sample", {-2.5_m, 0.7_m, -1.5_m}, {950.0f, 80.0f});
        auto descriptionLabel = ui_.AddLabel(
            static_cast<std::string>(kSampleIntroduction),
            {-2.5_m, 0.15_m, -1.5_m},
            {950.0f, 430.0f});
        auto instructionsTitleLabel =
            ui_.AddLabel("Instructions", {-2.5_m, -0.395_m, -1.5_m}, {950.0f, 80.0f});
        auto instructionsLabel = ui_.AddLabel(
            static_cast<std::string>(kSampleInstructions),
            {-2.5_m, -0.93_m, -1.5_m},
            {950.0f, 420.0f});

        // Align and size the description text for readability
        OVRFW::VRMenuFontParms fontParams{};
        fontParams.Scale = 0.5f;
        fontParams.AlignHoriz = OVRFW::HORIZONTAL_LEFT;
        descriptionLabel->SetFontParms(fontParams);
        descriptionLabel->SetTextLocalPosition({-0.88_m, -0.02_m, 0});
        instructionsLabel->SetFontParms(fontParams);
        instructionsLabel->SetTextLocalPosition({-0.88_m, -0.03_m, 0});
        fontParams.Scale = 1.f;
        fontParams.AlignHoriz = OVRFW::HORIZONTAL_CENTER;
        titleLabel->SetFontParms(fontParams);
        instructionsTitleLabel->SetFontParms(fontParams);

        // Tilt the description billboard 60 degrees towards the user
        descriptionLabel->SetLocalRotation(
            OVR::Quat<float>::FromRotationVector({0, OVR::DegreeToRad(60.0f), 0}));
        instructionsLabel->SetLocalRotation(
            OVR::Quat<float>::FromRotationVector({0, OVR::DegreeToRad(60.0f), 0}));
        instructionsTitleLabel->SetLocalRotation(
            OVR::Quat<float>::FromRotationVector({0, OVR::DegreeToRad(60.0f), 0}));
        titleLabel->SetLocalRotation(
            OVR::Quat<float>::FromRotationVector({0, OVR::DegreeToRad(60.0f), 0}));
    }

   private:
    OVRFW::TinyUI ui_;

    OVRFW::ControllerRenderer controllerRenderL_;
    OVRFW::ControllerRenderer controllerRenderR_;
    OVRFW::SimpleBeamRenderer cursorBeamRenderer_;
    OVRFW::GeometryRenderer toolRenderer_;

    // Menu Objects
    OVRFW::VRMenuObject* boxCountLabel_{nullptr};
    OVRFW::VRMenuObject* currentInteractionProfileText_{nullptr};
    OVRFW::VRMenuObject* boxColorLabel_{nullptr};
    const std::vector<OVR::Vector4f> colorOptions_{
        {0.65f, 0.f, 0.f, 1.f},
        {0.f, 0.65f, 0.f, 1.f},
        {0.f, 0.f, 0.65f, 1.f}};
    const std::vector<std::string> colorNames_{"Red", "Green", "Blue"};
    unsigned cubeColorIndex_ = 0;

    // Collection of all placed cubes
    OVRFW::GeometryBuilder cubeGeometry_;

    // Renderer of all the placed cubes, gets reset from cubeGeometry for any new cube
    OVRFW::GeometryRenderer cubeRenderer_;

    // Renderer of the single cube that shows where the user is about to place a cube
    OVRFW::GeometryRenderer templateCubeRenderer_;
    float templateCubeOffset_{20_cm}; // Default position 20cm out from the tool
    float oldTemplateCubeScale_{0.05f};
    float startingScalingDistance_{0.0f};
    float distBetweenHands_{0.0_m};
    bool currentlyScalingTemplate_{false};

    float templateCubeScale_{0.1f}; // Default size 10cm cube
    OVR::Quatf templateCubeRotation_{};
    float toolHitBox_{17_cm}; // Circular hitbox

    static constexpr float kMinTemplateCubeOffset_{0_m};
    static constexpr float kMaxTemplateCubeOffset_{5_m}; // Maximum 5m reach
    static constexpr float kMinTemplateCubeScale_{0.01f}; // 1 centimeter cube minimum
    static constexpr float kMaxTemplateCubeScale_{1.f}; // 1 meter cube max

    XrActionSet actionSetMenu_{XR_NULL_HANDLE};
    XrActionSet actionSetWorld_{XR_NULL_HANDLE};
    XrActionSet actionSetTool_{XR_NULL_HANDLE};

    XrAction actionSelect_{XR_NULL_HANDLE};
    XrAction actionMenuBeamPose_{XR_NULL_HANDLE};

    XrAction actionToggleColor_{XR_NULL_HANDLE};
    XrAction actionGrabRelease_{XR_NULL_HANDLE};
    XrAction actionHandsDrop_{XR_NULL_HANDLE};
    XrAction actionControllerGripPose_{XR_NULL_HANDLE};
    XrAction actionSpawnCube_{XR_NULL_HANDLE};
    XrAction actionCubeAimPose_{XR_NULL_HANDLE};
    XrAction actionRotateCube_{XR_NULL_HANDLE};
    XrAction actionScaleCube_{XR_NULL_HANDLE};
    XrAction actionTranslateCube_{XR_NULL_HANDLE};

    bool toolHeldInRight_{false};
    bool toolHeldInLeft_{false};

    // Reference spaces
    XrSpace spaceStage_{XR_NULL_HANDLE};
    XrSpace spaceLocal_{XR_NULL_HANDLE};
    XrSpace mainReferenceSpace_{XR_NULL_HANDLE};

    // Action spaces
    XrSpace spaceMenuBeamLeft_{XR_NULL_HANDLE};
    XrSpace spaceMenuBeamRight_{XR_NULL_HANDLE};
    XrSpace spaceCubeAimLeft_{XR_NULL_HANDLE};
    XrSpace spaceCubeAimRight_{XR_NULL_HANDLE};
    XrSpace spaceGripLeft_{XR_NULL_HANDLE};
    XrSpace spaceGripRight_{XR_NULL_HANDLE};

    // Updated every frame
    XrSpaceLocation locationMenuBeamLeft_{XR_TYPE_SPACE_LOCATION};
    XrSpaceLocation locationMenuBeamRight_{XR_TYPE_SPACE_LOCATION};
    XrSpaceLocation locationCubeAimLeft_{XR_TYPE_SPACE_LOCATION};
    XrSpaceLocation locationCubeAimRight_{XR_TYPE_SPACE_LOCATION};
    XrSpaceLocation locationGripRight_{XR_TYPE_SPACE_LOCATION};
    XrSpaceLocation locationGripLeft_{XR_TYPE_SPACE_LOCATION};

    // XrPaths for convenience
    XrPath leftHandPath_{XR_NULL_PATH};
    XrPath rightHandPath_{XR_NULL_PATH};
    XrPath simpleInteractionProfile_{XR_NULL_PATH};
    XrPath touchInteractionProfile_{XR_NULL_PATH};
    XrPath touchProInteractionProfile_{XR_NULL_PATH};

    /// Hands
    PFN_xrCreateHandTrackerEXT xrCreateHandTrackerEXT_ = nullptr;
    PFN_xrDestroyHandTrackerEXT xrDestroyHandTrackerEXT_ = nullptr;
    PFN_xrLocateHandJointsEXT xrLocateHandJointsEXT_ = nullptr;
    XrHandTrackerEXT handTrackerL_ = XR_NULL_HANDLE;
    XrHandTrackerEXT handTrackerR_ = XR_NULL_HANDLE;
    bool supportsHandTracking_{false};

    XrHandJointLocationEXT jointLocationsL_[XR_HAND_JOINT_COUNT_EXT];
    XrHandJointLocationEXT jointLocationsR_[XR_HAND_JOINT_COUNT_EXT];
    std::vector<OVRFW::GeometryRenderer> handJointRenderersL_;
    std::vector<OVRFW::GeometryRenderer> handJointRenderersR_;
    bool handTrackedL_ = false;
    bool handTrackedR_ = false;
    OVR::Vector4f jointColor_{0.196, 0.3725, 0.1412, 0.8};

    std::unordered_map<XrActionSet, ActionSetDisplayPanel> actionSetPanels_{};
};

ENTRY_POINT(XrInputSampleApp)
