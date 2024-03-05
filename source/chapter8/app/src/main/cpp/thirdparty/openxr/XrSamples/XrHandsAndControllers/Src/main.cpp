/*******************************************************************************

Filename    :   Main.cpp
Content     :   OpenXR sample showing use of the hands and controllers API
Created     :
Authors     :   Matthew Langille
Language    :   C++
Copyright   :   Copyright (c) Meta Platforms, Inc. and affiliates.

*******************************************************************************/

#include <string>
#include <string_view>
#include <unordered_map>

#include <openxr/openxr.h>

#include "GUI/VRMenuObject.h"
#include "Render/BitmapFont.h"
#include "Render/GlGeometry.h"
#include "XrApp.h"
#include "ActionSetDisplayPanel.h"
#include "SkyboxRenderer.h"
#include "EnvironmentRenderer.h"
#include "GUI/VRMenuObject.h"

#include "OVR_Math.h"
#include "Input/ControllerRenderer.h"
#include "Input/HandRenderer.h"
#include "Input/TinyUI.h"
#include "Render/GlGeometry.h"
#include "Render/GeometryRenderer.h"
#include "Render/SimpleBeamRenderer.h"
#include "openxr/openxr_oculus_helpers.h"
#include "openxr/fb_touch_controller_pro.h"
#include "openxr/meta_touch_controller_plus.h"
#include "openxr/metax2_detached_controllers.h"
#include "openxr/metax1_simultaneous_hands_controllers_management.h"

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

static int TimesClicked;

class XrHandsAndControllersSampleApp : public OVRFW::XrApp {
   public:
    static constexpr std::string_view sampleExplanation =
        "Sample demonstrating hands and controllers in action at\n"
        "the same time, with detached controllers tracked when not\n"
        "in hand";

    XrHandsAndControllersSampleApp() : OVRFW::XrApp() {
        BackgroundColor = OVR::Vector4f(0.45f, 0.35f, 0.2f, 1.0f);
        TimesClicked = 0;

        // Disable framework input management, letting this sample explicitly
        // call xrSyncActions() every frame; which includes control over which
        // ActionSet to set as active each frame
        SkipInputHandling = true;
    }

    virtual std::vector<const char*> GetExtensions() override {
        std::vector<const char*> extensions = XrApp::GetExtensions();
        isDetachedControllersExtensionAvailable_ =
            isExtensionAvailable(XR_METAX2_DETACHED_CONTROLLERS_EXTENSION_NAME);
        isMSFTHandInteractionExtensionAvailable_ =
            isExtensionAvailable(XR_MSFT_HAND_INTERACTION_EXTENSION_NAME);
        isSimultaneousHandsControllersExtensionAvailable_ = isExtensionAvailable(
            XR_METAX1_SIMULTANEOUS_HANDS_CONTROLLERS_MANAGEMENT_EXTENSION_NAME);

        extensions.push_back(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
        extensions.push_back(XR_FB_HAND_TRACKING_MESH_EXTENSION_NAME);
        extensions.push_back(XR_FB_HAND_TRACKING_AIM_EXTENSION_NAME);
        if (isMSFTHandInteractionExtensionAvailable_) {
            extensions.push_back(XR_MSFT_HAND_INTERACTION_EXTENSION_NAME);
        }
        extensions.push_back(XR_FB_TOUCH_CONTROLLER_PRO_EXTENSION_NAME);
        extensions.push_back(XR_META_TOUCH_CONTROLLER_PLUS_EXTENSION_NAME);
        if (isDetachedControllersExtensionAvailable_) {
            extensions.push_back(XR_METAX2_DETACHED_CONTROLLERS_EXTENSION_NAME);
        }

        if (isSimultaneousHandsControllersExtensionAvailable_) {
            extensions.push_back(XR_METAX1_SIMULTANEOUS_HANDS_CONTROLLERS_MANAGEMENT_EXTENSION_NAME);
        }
        return extensions;
    }

    std::unordered_map<XrPath, std::vector<XrActionSuggestedBinding>> GetSuggestedBindings(
        XrInstance instance) override {
        OXR(xrStringToPath(Instance, "/user/hand/left", &leftHandPath_));
        OXR(xrStringToPath(Instance, "/user/hand/right", &rightHandPath_));
        OXR(xrStringToPath(
            Instance, "/user/detached_controller_meta/left", &leftDetachedControllerPath_));
        OXR(xrStringToPath(
            Instance, "/user/detached_controller_meta/right", &rightDetachedControllerPath_));

        // Get the default bindings suggested by XrApp framework
        auto suggestedBindings = XrApp::GetSuggestedBindings(instance);
        actionSetMenu_ = CreateActionSet(0, "menu_action_set", "UI Action Set");
        actionSetWorld_ = CreateActionSet(0, "world_action_set", "World Action Set");
        actionSetGestures_ = CreateActionSet(0, "gesture_action_set", "Gesture Action Set");
        actionSelect_ = CreateAction(
            actionSetMenu_,
            XR_ACTION_TYPE_BOOLEAN_INPUT,
            "select",
            "Select/Click UI Element" // Displayed to users, should be translated to the user's
                                      // local language
        );

        actionGrab_ = CreateAction(
            actionSetWorld_, XR_ACTION_TYPE_BOOLEAN_INPUT, "action_grab", "Simple Grab");
        actionTrigger_ = CreateAction(
            actionSetGestures_, XR_ACTION_TYPE_BOOLEAN_INPUT, "action_trigger", "Simple Trigger");
        actionThumbstick_ = CreateAction(
            actionSetGestures_,
            XR_ACTION_TYPE_VECTOR2F_INPUT,
            "action_thumbstick",
            "Simple Thumbstick");
        actionThumbstickX_ = CreateAction(
            actionSetGestures_,
            XR_ACTION_TYPE_FLOAT_INPUT,
            "action_thumbstick_x",
            "Simple Thumbstick X");
        actionThumbstickY_ = CreateAction(
            actionSetGestures_,
            XR_ACTION_TYPE_FLOAT_INPUT,
            "action_thumbstick_y",
            "Simple Thumbstick Y");

        // Actions for testing hands interaction
        actionHandSelect_ = CreateAction(
            actionSetMenu_,
            XR_ACTION_TYPE_FLOAT_INPUT,
            "hand_select",
            "Select For Test" // Displayed to users, should be translated to the user's
                              // local language
        );

        actionHandSqueeze_ = CreateAction(
            actionSetMenu_,
            XR_ACTION_TYPE_BOOLEAN_INPUT,
            "squeeze",
            "Squeeze For Test" // Displayed to users, should be translated to the user's
                               // local language
        );

        // In order to be able to distinguish between left and right hand versions
        // of these actions, we need to pass in subactionPaths
        XrPath handsTopLevelPaths[]{leftHandPath_, rightHandPath_};
        actionControllerAimPose_ = CreateAction(
            actionSetWorld_,
            XR_ACTION_TYPE_POSE_INPUT,
            "controller_aim_pose",
            "Controller Aim Pose",
            2,
            handsTopLevelPaths);

        actionControllerGripPose_ = CreateAction(
            actionSetWorld_,
            XR_ACTION_TYPE_POSE_INPUT,
            "controller_grip_pose",
            "Controller Grip Pose",
            2,
            handsTopLevelPaths);

        actionHandAimPose_ = CreateAction(
            actionSetWorld_,
            XR_ACTION_TYPE_POSE_INPUT,
            "hand_aim_pose",
            "Hand Aim Pose",
            2,
            handsTopLevelPaths);

        actionHandGripPose_ = CreateAction(
            actionSetWorld_,
            XR_ACTION_TYPE_POSE_INPUT,
            "hand_grip_pose",
            "Hand Grip Pose",
            2,
            handsTopLevelPaths);

        XrPath detachedControllerPaths[]{leftDetachedControllerPath_, rightDetachedControllerPath_};
        actionDetachedControllerAimPose_ = CreateAction(
            actionSetWorld_,
            XR_ACTION_TYPE_POSE_INPUT,
            "controller_detached_aim_pose",
            "Controller Detached Aim Pose",
            2,
            detachedControllerPaths);

        actionDetachedControllerGripPose_ = CreateAction(
            actionSetWorld_,
            XR_ACTION_TYPE_POSE_INPUT,
            "controller_detached_grip_pose",
            "Controller Detached Grip Pose",
            2,
            detachedControllerPaths);

        ///////////////////////////////////////////////////////////
        ///  touch_controller_pro suggested bindings
        ///////////////////////////////////////////////////////////
        OXR(xrStringToPath(
            instance,
            "/interaction_profiles/facebook/touch_controller_pro",
            &touchProInteractionProfile_));

        // Binding the same action to both hands is not a problem,
        // since you can use subActionPath later to distinguish them
        suggestedBindings[touchProInteractionProfile_].emplace_back(
            ActionSuggestedBinding(actionGrab_, "/user/hand/left/input/trigger/value"));
        suggestedBindings[touchProInteractionProfile_].emplace_back(
            ActionSuggestedBinding(actionSelect_, "/user/hand/right/input/trigger/value"));
        suggestedBindings[touchProInteractionProfile_].emplace_back(
            ActionSuggestedBinding(actionThumbstick_, "/user/hand/left/input/thumbstick"));
        suggestedBindings[touchProInteractionProfile_].emplace_back(
            ActionSuggestedBinding(actionThumbstickX_, "/user/hand/right/input/thumbstick/x"));
        suggestedBindings[touchProInteractionProfile_].emplace_back(
            ActionSuggestedBinding(actionThumbstickY_, "/user/hand/right/input/thumbstick/y"));
        suggestedBindings[touchProInteractionProfile_].emplace_back(
            ActionSuggestedBinding(actionGrab_, "/user/hand/right/input/squeeze/value"));
        suggestedBindings[touchProInteractionProfile_].emplace_back(
            ActionSuggestedBinding(actionTrigger_, "/user/hand/right/input/trigger/value"));
        suggestedBindings[touchProInteractionProfile_].emplace_back(
            ActionSuggestedBinding(actionControllerAimPose_, "/user/hand/left/input/aim/pose"));
        suggestedBindings[touchProInteractionProfile_].emplace_back(
            ActionSuggestedBinding(actionControllerAimPose_, "/user/hand/right/input/aim/pose"));
        suggestedBindings[touchProInteractionProfile_].emplace_back(
            ActionSuggestedBinding(actionControllerGripPose_, "/user/hand/left/input/grip/pose"));
        suggestedBindings[touchProInteractionProfile_].emplace_back(
            ActionSuggestedBinding(actionControllerGripPose_, "/user/hand/right/input/grip/pose"));

        if (isDetachedControllersExtensionAvailable_) {
            // Detached controllers aim and grip poses
            suggestedBindings[touchProInteractionProfile_].emplace_back(ActionSuggestedBinding(
                actionDetachedControllerAimPose_,
                "/user/detached_controller_meta/left/input/aim/pose"));
            suggestedBindings[touchProInteractionProfile_].emplace_back(ActionSuggestedBinding(
                actionDetachedControllerAimPose_,
                "/user/detached_controller_meta/right/input/aim/pose"));
            suggestedBindings[touchProInteractionProfile_].emplace_back(ActionSuggestedBinding(
                actionDetachedControllerGripPose_,
                "/user/detached_controller_meta/right/input/grip/pose"));
            suggestedBindings[touchProInteractionProfile_].emplace_back(ActionSuggestedBinding(
                actionDetachedControllerGripPose_,
                "/user/detached_controller_meta/left/input/grip/pose"));
        }

        ///////////////////////////////////////////////////////////
        ///  touch_controller_plus suggested bindings
        ///////////////////////////////////////////////////////////
        OXR(xrStringToPath(
            instance,
            "/interaction_profiles/meta/touch_controller_plus",
            &touchPlusInteractionProfile_));

        // Binding the same action to both hands is not a problem,
        // since you can use subActionPath later to distinguish them
        suggestedBindings[touchPlusInteractionProfile_].emplace_back(
            ActionSuggestedBinding(actionGrab_, "/user/hand/left/input/trigger/value"));
        suggestedBindings[touchPlusInteractionProfile_].emplace_back(
            ActionSuggestedBinding(actionSelect_, "/user/hand/right/input/trigger/value"));
        suggestedBindings[touchPlusInteractionProfile_].emplace_back(
            ActionSuggestedBinding(actionThumbstick_, "/user/hand/left/input/thumbstick"));
        suggestedBindings[touchPlusInteractionProfile_].emplace_back(
            ActionSuggestedBinding(actionThumbstickX_, "/user/hand/right/input/thumbstick/x"));
        suggestedBindings[touchPlusInteractionProfile_].emplace_back(
            ActionSuggestedBinding(actionThumbstickY_, "/user/hand/right/input/thumbstick/y"));
        suggestedBindings[touchPlusInteractionProfile_].emplace_back(
            ActionSuggestedBinding(actionGrab_, "/user/hand/right/input/squeeze/value"));
        suggestedBindings[touchPlusInteractionProfile_].emplace_back(
            ActionSuggestedBinding(actionTrigger_, "/user/hand/right/input/trigger/value"));
        suggestedBindings[touchPlusInteractionProfile_].emplace_back(
            ActionSuggestedBinding(actionControllerAimPose_, "/user/hand/left/input/aim/pose"));
        suggestedBindings[touchPlusInteractionProfile_].emplace_back(
            ActionSuggestedBinding(actionControllerAimPose_, "/user/hand/right/input/aim/pose"));
        suggestedBindings[touchPlusInteractionProfile_].emplace_back(
            ActionSuggestedBinding(actionControllerGripPose_, "/user/hand/left/input/grip/pose"));
        suggestedBindings[touchPlusInteractionProfile_].emplace_back(
            ActionSuggestedBinding(actionControllerGripPose_, "/user/hand/right/input/grip/pose"));

        if (isDetachedControllersExtensionAvailable_) {
            // Detached controllers aim and grip poses
            suggestedBindings[touchPlusInteractionProfile_].emplace_back(ActionSuggestedBinding(
                actionDetachedControllerAimPose_,
                "/user/detached_controller_meta/left/input/aim/pose"));
            suggestedBindings[touchPlusInteractionProfile_].emplace_back(ActionSuggestedBinding(
                actionDetachedControllerAimPose_,
                "/user/detached_controller_meta/right/input/aim/pose"));
            suggestedBindings[touchPlusInteractionProfile_].emplace_back(ActionSuggestedBinding(
                actionDetachedControllerGripPose_,
                "/user/detached_controller_meta/right/input/grip/pose"));
            suggestedBindings[touchPlusInteractionProfile_].emplace_back(ActionSuggestedBinding(
                actionDetachedControllerGripPose_,
                "/user/detached_controller_meta/left/input/grip/pose"));
        }

        ///////////////////////////////////////////////////////////
        ///  MSFT_hand_interaction suggested bindings
        ///////////////////////////////////////////////////////////
        if (isMSFTHandInteractionExtensionAvailable_) {
            OXR(xrStringToPath(
                instance,
                "/interaction_profiles/microsoft/hand_interaction",
                &msftHandInteractionProfile_));

            suggestedBindings[msftHandInteractionProfile_].emplace_back(
                ActionSuggestedBinding(actionHandSelect_, "/user/hand/left/input/select/value"));
            suggestedBindings[msftHandInteractionProfile_].emplace_back(
                ActionSuggestedBinding(actionHandSelect_, "/user/hand/right/input/select/value"));
            suggestedBindings[msftHandInteractionProfile_].emplace_back(
                ActionSuggestedBinding(actionHandSqueeze_, "/user/hand/left/input/squeeze/value"));
            suggestedBindings[msftHandInteractionProfile_].emplace_back(
                ActionSuggestedBinding(actionHandSqueeze_, "/user/hand/right/input/squeeze/value"));

            // Hands aim and grip poses
            suggestedBindings[msftHandInteractionProfile_].emplace_back(
                ActionSuggestedBinding(actionHandAimPose_, "/user/hand/left/input/aim/pose"));
            suggestedBindings[msftHandInteractionProfile_].emplace_back(
                ActionSuggestedBinding(actionHandAimPose_, "/user/hand/right/input/aim/pose"));
            suggestedBindings[msftHandInteractionProfile_].emplace_back(
                ActionSuggestedBinding(actionHandGripPose_, "/user/hand/left/input/grip/pose"));
            suggestedBindings[msftHandInteractionProfile_].emplace_back(
                ActionSuggestedBinding(actionHandGripPose_, "/user/hand/right/input/grip/pose"));
        }

        return suggestedBindings;
    }

    virtual bool AppInit(const xrJava* context) override {
        int fontVertexBufferSize = 32 * 1024; // Custom large text buffer size for all the text
        bool updateColors = true; // Update UI colors on interaction
        if (false == ui_.Init(context, GetFileSys(), updateColors, fontVertexBufferSize)) {
            ALOG("TinyUI::Init FAILED.");
            return false;
        }

        auto fileSys = std::unique_ptr<OVRFW::ovrFileSys>(OVRFW::ovrFileSys::Create(*context));

        if( fileSys ) {
            std::string environmentPath = "apk:///assets/SmallRoom.gltf.ovrscene";
            environmentRenderer_.Init(environmentPath, fileSys.get());
            std::string skyboxPath = "apk:///assets/Skybox.gltf.ovrscene";
            skyboxRenderer_.Init(skyboxPath, fileSys.get());
        }

        // Inspect hand tracking system properties
        XrSystemHandTrackingPropertiesEXT handTrackingSystemProperties{
            XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT};
        XrSystemProperties systemProperties{
            XR_TYPE_SYSTEM_PROPERTIES, &handTrackingSystemProperties};
        XrSystemSimultaneousHandsControllersPropertiesMETAX1 simultaneousHandsControllersSystemProperties{
                XR_TYPE_SYSTEM_SIMULTANEOUS_HANDS_CONTROLLERS_PROPERTIES_METAX1};
        reinterpret_cast<XrBaseOutStructure*>(&handTrackingSystemProperties)->next =
            reinterpret_cast<XrBaseOutStructure*>(&simultaneousHandsControllersSystemProperties);
        OXR(xrGetSystemProperties(GetInstance(), GetSystemId(), &systemProperties));
        if (!handTrackingSystemProperties.supportsHandTracking ||
            !simultaneousHandsControllersSystemProperties.supportsSimultaneousHandsAndControllers) {
            ALOG("System does not support simultaneous hands and controllers. Exiting.");
            return false;
        }

        /// Hook up extensions for hand tracking, as well as simultaneous hands and controllers.
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
        OXR(xrGetInstanceProcAddr(
            GetInstance(), "xrGetHandMeshFB", (PFN_xrVoidFunction*)(&xrGetHandMeshFB_)));
        assert(xrGetHandMeshFB_);
        OXR(xrGetInstanceProcAddr(
            GetInstance(),
            "xrResumeSimultaneousHandsControllersTrackingMETAX1",
            (PFN_xrVoidFunction*)(&xrResumeSimultaneousHandsControllersTrackingMETAX1_)));
        assert(xrResumeSimultaneousHandsControllersTrackingMETAX1_);
        OXR(xrGetInstanceProcAddr(
            GetInstance(),
            "xrPauseSimultaneousHandsControllersTrackingMETAX1",
            (PFN_xrVoidFunction*)(&xrPauseSimultaneousHandsControllersTrackingMETAX1_)));
        assert(xrPauseSimultaneousHandsControllersTrackingMETAX1_);

        return true;
    }

    virtual bool SessionInit() override {
        XrActionSpaceCreateInfo actionSpaceCreateInfo{XR_TYPE_ACTION_SPACE_CREATE_INFO};
        actionSpaceCreateInfo.action = actionControllerAimPose_;
        actionSpaceCreateInfo.poseInActionSpace = {{0, 0, 0, 1}, {0, 0, 0}};

        actionSpaceCreateInfo.subactionPath = leftHandPath_;
        OXR(xrCreateActionSpace(GetSession(), &actionSpaceCreateInfo, &spaceControllerAimLeft_));
        actionSpaceCreateInfo.subactionPath = rightHandPath_;
        OXR(xrCreateActionSpace(GetSession(), &actionSpaceCreateInfo, &spaceControllerAimRight_));

        actionSpaceCreateInfo.action = actionControllerGripPose_;
        actionSpaceCreateInfo.subactionPath = leftHandPath_;
        OXR(xrCreateActionSpace(GetSession(), &actionSpaceCreateInfo, &spaceControllerGripLeft_));
        actionSpaceCreateInfo.subactionPath = rightHandPath_;
        OXR(xrCreateActionSpace(GetSession(), &actionSpaceCreateInfo, &spaceControllerGripRight_));

        // Hands
        actionSpaceCreateInfo.action = actionHandAimPose_;
        actionSpaceCreateInfo.subactionPath = leftHandPath_;
        OXR(xrCreateActionSpace(GetSession(), &actionSpaceCreateInfo, &spaceHandAimLeft_));
        actionSpaceCreateInfo.subactionPath = rightHandPath_;
        OXR(xrCreateActionSpace(GetSession(), &actionSpaceCreateInfo, &spaceHandAimRight_));

        actionSpaceCreateInfo.action = actionHandGripPose_;
        actionSpaceCreateInfo.subactionPath = leftHandPath_;
        OXR(xrCreateActionSpace(GetSession(), &actionSpaceCreateInfo, &spaceHandGripLeft_));
        actionSpaceCreateInfo.subactionPath = rightHandPath_;
        OXR(xrCreateActionSpace(GetSession(), &actionSpaceCreateInfo, &spaceHandGripRight_));

        actionSpaceCreateInfo.action = actionDetachedControllerAimPose_;
        actionSpaceCreateInfo.poseInActionSpace = {{0, 0, 0, 1}, {0_m, 0_m, 0_m}};
        actionSpaceCreateInfo.subactionPath = leftDetachedControllerPath_;
        OXR(xrCreateActionSpace(
            GetSession(), &actionSpaceCreateInfo, &spaceDetachedControllerAimLeft_));
        actionSpaceCreateInfo.subactionPath = rightDetachedControllerPath_;
        OXR(xrCreateActionSpace(
            GetSession(), &actionSpaceCreateInfo, &spaceDetachedControllerAimRight_));
        actionSpaceCreateInfo.action = actionDetachedControllerGripPose_;
        actionSpaceCreateInfo.subactionPath = leftDetachedControllerPath_;
        OXR(xrCreateActionSpace(
            GetSession(), &actionSpaceCreateInfo, &spaceDetachedControllerGripLeft_));
        actionSpaceCreateInfo.subactionPath = rightDetachedControllerPath_;
        OXR(xrCreateActionSpace(
            GetSession(), &actionSpaceCreateInfo, &spaceDetachedControllerGripRight_));

        {
            // Attach ActionSets to session
            // This is required before any call to xrSyncActions for these action sets
            std::vector<XrActionSet> actionSets{
                {actionSetWorld_, actionSetMenu_, actionSetGestures_}};
            XrSessionActionSetsAttachInfo attachInfo{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};
            attachInfo.countActionSets = actionSets.size();
            attachInfo.actionSets = actionSets.data();
            OXR(xrAttachSessionActionSets(Session, &attachInfo));
            // After this point all actions and bindings are final for the session
            // (calls to xrSuggestInteractionProfileBindings and xrAttachSessionActionSets fail)
        }

        //////////////////////////////////////////////////
        ///  Create reference spaces
        //////////////////////////////////////////////////
        XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
        referenceSpaceCreateInfo.poseInReferenceSpace = {{0, 0, 0, 1}, {0, 0, 0}};
        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        OXR(xrCreateReferenceSpace(GetSession(), &referenceSpaceCreateInfo, &spaceLocal_));

        referenceSpaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
        OXR(xrCreateReferenceSpace(GetSession(), &referenceSpaceCreateInfo, &spaceStage_));

        {
            // Setup all the UI panels to display the state of each action
            actionSetPanels_.insert(
                {actionSetMenu_,
                 ActionSetDisplayPanel(
                     "Menu Action Set", Session, Instance, &ui_, {-2.0, 2.5, -2.5})});
            actionSetPanels_.at(actionSetMenu_).AddBoolAction(actionSelect_, "Select");

            actionSetPanels_.insert(
                {actionSetWorld_,
                 ActionSetDisplayPanel(
                     "World Action Set", Session, Instance, &ui_, {-0.5, 2.5, -2.5})});
            actionSetPanels_.at(actionSetWorld_).AddBoolAction(actionGrab_, "Grab");
            actionSetPanels_.at(actionSetWorld_)
                .AddPoseAction(actionControllerGripPose_, "Grip Pose");
            actionSetPanels_.at(actionSetWorld_)
                .AddPoseAction(actionControllerAimPose_, "Aim Pose");

            if (isDetachedControllersExtensionAvailable_) {
                actionSetPanels_.at(actionSetWorld_)
                    .AddPoseAction(actionDetachedControllerGripPose_, "Detached Grip Pose");
                actionSetPanels_.at(actionSetWorld_)
                    .AddPoseAction(actionDetachedControllerAimPose_, "Detached Aim Pose");
            }

            actionSetPanels_.insert(
                {actionSetGestures_,
                 ActionSetDisplayPanel(
                     "Gesture Action Set", Session, Instance, &ui_, {1.0, 2.5, -2.5})});
            actionSetPanels_.at(actionSetGestures_).AddBoolAction(actionTrigger_, "Trigger");
            actionSetPanels_.at(actionSetGestures_).AddVec2Action(actionThumbstick_, "Thumbstick");
            actionSetPanels_.at(actionSetGestures_).AddFloatAction(actionThumbstickX_, "Scale");
            actionSetPanels_.at(actionSetGestures_).AddFloatAction(actionThumbstickY_, "Translate");
        }

        interactionProfileTextTitle_ = ui_.AddLabel(
            "Interaction Profiles for Top Level Paths", {-2.5f, 2.45f, -1.5f}, {900.0f, 45.0f});
        interactionProfileText_ = ui_.AddLabel("Label", {-2.5f, 2.2f, -1.5f}, {900.0f, 160.0f});

        // Align text for interaction profile view
        OVRFW::VRMenuFontParms fontParams{};
        fontParams.Scale = 0.5f;
        fontParams.AlignHoriz = OVRFW::HORIZONTAL_LEFT;
        fontParams.AlignVert = OVRFW::VERTICAL_CENTER;
        interactionProfileText_->SetFontParms(fontParams);
        interactionProfileText_->SetTextLocalPosition({-0.82_m, 0, 0});

        auto buttonLabel = ui_.AddLabel("Clicked 0 times", {-2.5f, 1.9f, -1.5f}, {700.0f, 45.0f});
        auto button =
            ui_.AddButton("Click me!", {-2.5f, 1.7f, -1.5f}, {700.0f, 90.0f}, [buttonLabel]() {
                TimesClicked++;
                buttonLabel->SetText("Clicked %d times.", TimesClicked);
            });

        // Enable simultaneous hands and controllers mode
        setSimultaneousHandsControllersMode(true);
        auto mmButtonLabel =
            ui_.AddLabel("Simultaneous Hands and Controllers Enabled", {-2.5f, 1.5f, -1.5f}, {700.0f, 45.0f});
        auto mmButton = ui_.AddButton(
            "Click to enable/disable simultaneous hands and controllers",
            {-2.5f, 1.3f, -1.5f},
            {700.0f, 90.0f},
            [mmButtonLabel, self = this]() {
                static bool isSimultaneousHandsControllersEnabled = true;
                isSimultaneousHandsControllersEnabled = !isSimultaneousHandsControllersEnabled;
                self->setSimultaneousHandsControllersMode(isSimultaneousHandsControllersEnabled);
                mmButtonLabel->SetText("Simultaneous Hands and Controllers %s", isSimultaneousHandsControllersEnabled ? "Enabled" : "Disabled");
            });

        // Tilt the interaction UI towards user
        interactionProfileTextTitle_->SetLocalRotation(
            OVR::Quat<float>::FromRotationVector({0, OVR::DegreeToRad(60.0f), 0}));
        interactionProfileText_->SetLocalRotation(
            OVR::Quat<float>::FromRotationVector({0, OVR::DegreeToRad(60.0f), 0}));
        buttonLabel->SetLocalRotation(
            OVR::Quat<float>::FromRotationVector({0, OVR::DegreeToRad(60.0f), 0}));
        button->SetLocalRotation(
            OVR::Quat<float>::FromRotationVector({0, OVR::DegreeToRad(60.0f), 0}));
        mmButtonLabel->SetLocalRotation(
            OVR::Quat<float>::FromRotationVector({0, OVR::DegreeToRad(60.0f), 0}));
        mmButton->SetLocalRotation(
            OVR::Quat<float>::FromRotationVector({0, OVR::DegreeToRad(60.0f), 0}));

        CreateSampleDescriptionPanel();

        /// Disable scene navigation
        GetScene().SetFootPos({10.0f, 0.0f, 0.0f});
        this->FreeMove = false;

        // Init objects that need OpenXR Session
        if (false == controllerRenderL_.Init(true)) {
            ALOG("SessionInit::Init L controller renderer FAILED.");
            return false;
        }
        if (false == controllerRenderR_.Init(false)) {
            ALOG("SessionInit::Init R controller renderer FAILED.");
            return false;
        }

        cursorBeamRenderer_.Init(GetFileSys(), nullptr, OVR::Vector4f(1.0f), 1.0f);

        /// Hand rendering
        if (xrCreateHandTrackerEXT_) {
            SetupHandTrackers();

            // Enable simultaneous hands and controllers mode
            XrSimultaneousHandsControllersTrackingResumeInfoMETAX1 resumeInfo = {
                XR_TYPE_SIMULTANEOUS_HANDS_CONTROLLERS_TRACKING_RESUME_INFO_METAX1};
            OXR(xrResumeSimultaneousHandsControllersTrackingMETAX1_(GetSession(), &resumeInfo));
        }

        return true;
    }

    virtual void Update(const OVRFW::ovrApplFrameIn& in) override {
        {
            // xrSyncAction
            std::vector<XrActiveActionSet> activeActionSets = {{actionSetWorld_}, {actionSetMenu_}};

            XrActionsSyncInfo syncInfo = {XR_TYPE_ACTIONS_SYNC_INFO};
            syncInfo.countActiveActionSets = activeActionSets.size();
            syncInfo.activeActionSets = activeActionSets.data();
            OXR(xrSyncActions(Session, &syncInfo));
        }

        // The hit test devices are rays used for hit detection in the UI.
        // Clear the rays from last frame
        ui_.HitTestDevices().clear();

        if (xrLocateHandJointsEXT_) {
            UpdateHandsInformation(in);
        }

        {
            //
            //   Update current interaction profile display
            //
            auto getStringIpRepresentation = [&](const XrPath& xrPath, std::string& stringOut) {
                XrInteractionProfileState ipState{XR_TYPE_INTERACTION_PROFILE_STATE};
                OXR(xrGetCurrentInteractionProfile(GetSession(), xrPath, &ipState));
                if (ipState.interactionProfile != XR_NULL_PATH) {
                    char buf[XR_MAX_PATH_LENGTH];
                    uint32_t outLength = 0;
                    OXR(xrPathToString(
                        GetInstance(),
                        ipState.interactionProfile,
                        XR_MAX_PATH_LENGTH,
                        &outLength,
                        buf));
                    stringOut = std::string(buf);
                }
            };

            // This is for display purposes
            std::string leftInteractionProfileString = "XR_NULL_PATH";
            std::string rightInteractionProfileString = "XR_NULL_PATH";
            std::string leftDetachedInteractionProfileString = "XR_NULL_PATH";
            std::string rightDetachedInteractionProfileString = "XR_NULL_PATH";
            getStringIpRepresentation(leftHandPath_, leftInteractionProfileString);
            getStringIpRepresentation(rightHandPath_, rightInteractionProfileString);
            getStringIpRepresentation(
                leftDetachedControllerPath_, leftDetachedInteractionProfileString);
            getStringIpRepresentation(
                rightDetachedControllerPath_, rightDetachedInteractionProfileString);

            interactionProfileText_->SetText(
                "/user/hand/left: %s\n"
                "/user/hand/right: %s\n"
                "/user/detached_controller_meta/left: %s\n"
                "/user/detached_controller_meta/right: %s\n",
                leftInteractionProfileString.c_str(),
                rightInteractionProfileString.c_str(),
                leftDetachedInteractionProfileString.c_str(),
                rightDetachedInteractionProfileString.c_str());
        }

        {
            //
            //  Locate controller grip and aim poses
            //

            // DisplayTime is the time returned by the latest xrWaitFrame() call.
            // It's the time when the current frame is expected to be shown to the user.
            // xrLocateSpace returns a prediction of where these spaces spaces will be at that
            // future time.
            XrTime time = ToXrTime(in.PredictedDisplayTime);
            OXR(xrLocateSpace(spaceControllerGripLeft_, spaceStage_, time, &gripLeftLocation_));
            OXR(xrLocateSpace(spaceControllerGripRight_, spaceStage_, time, &gripRightLocation_));
            OXR(xrLocateSpace(spaceControllerAimLeft_, spaceStage_, time, &aimLeftLocation_));
            OXR(xrLocateSpace(spaceControllerAimRight_, spaceStage_, time, &aimRightLocation_));

            // Hands
            OXR(xrLocateSpace(spaceHandGripLeft_, spaceStage_, time, &handGripLeftLocation_));
            OXR(xrLocateSpace(spaceHandGripRight_, spaceStage_, time, &handGripRightLocation_));
            OXR(xrLocateSpace(spaceHandAimLeft_, spaceStage_, time, &handAimLeftLocation_));
            OXR(xrLocateSpace(spaceHandAimRight_, spaceStage_, time, &handAimRightLocation_));

            // Detached controllers
            OXR(xrLocateSpace(
                spaceDetachedControllerGripLeft_,
                spaceStage_,
                time,
                &gripDetachedLeftLocation_));
            OXR(xrLocateSpace(
                spaceDetachedControllerGripRight_,
                spaceStage_,
                time,
                &gripDetachedRightLocation_));
            OXR(xrLocateSpace(
                spaceDetachedControllerAimLeft_, spaceStage_, time, &aimDetachedLeftLocation_));
            OXR(xrLocateSpace(
                spaceDetachedControllerAimRight_,
                spaceStage_,
                time,
                &aimDetachedRightLocation_));
        }

        // Check validity of grip location before updating controllers with new location
        // All apps rendering controllers should do this, otherwise you draw floating controllers
        // in cases where tracking is lost or where there's a system menu on top taking input focus
        if ((gripLeftLocation_.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0) {
            controllerRenderL_.Update(FromXrPosef(gripLeftLocation_.pose));

            bool click2 = GetActionStateBoolean(actionSelect_, leftHandPath_).currentState;
            // Add new UI hit detection ray based on the aim pose (not grip!)
            ui_.AddHitTestRay(FromXrPosef(aimLeftLocation_.pose), click2);
        }
        if ((gripRightLocation_.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0) {
            controllerRenderR_.Update(FromXrPosef(gripRightLocation_.pose));

            bool click = GetActionStateBoolean(actionSelect_, rightHandPath_).currentState;
            ui_.AddHitTestRay(FromXrPosef(aimRightLocation_.pose), click);
        }

        // Check validity of detached grip locations
        if ((gripDetachedLeftLocation_.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) !=
            0) {
            controllerRenderL_.Update(FromXrPosef(gripDetachedLeftLocation_.pose));
        }
        if ((gripDetachedRightLocation_.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) !=
            0) {
            controllerRenderR_.Update(FromXrPosef(gripDetachedRightLocation_.pose));
        }

        for (auto& panelPair : actionSetPanels_) {
            panelPair.second.Update();
        }

        cursorBeamRenderer_.Update(in, ui_.HitTestDevices());
        ui_.Update(in);
    }

    virtual void Render(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out) override {
        skyboxRenderer_.Render(out.Surfaces);
        environmentRenderer_.Render(out.Surfaces);

        ui_.Render(in, out);
        if (isDetachedControllersExtensionAvailable_) {
            // At DetachedControllerExtension enabled case, render controller when hand is not
            // availble or grip detached location is valid.
            if (((gripLeftLocation_.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
                 handTrackedL_ == false) ||
                ((gripDetachedLeftLocation_.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) !=
                 0)) {
                controllerRenderL_.Render(out.Surfaces);
            }
            if (((gripRightLocation_.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
                 handTrackedR_ == false) ||
                ((gripDetachedRightLocation_.locationFlags &
                  XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0)) {
                controllerRenderR_.Render(out.Surfaces);
            }
        } else {
            // At DetachedControllerExtension not enabled case, render controller whenever grip data
            // is valid.
            if ((gripLeftLocation_.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0) {
                controllerRenderL_.Render(out.Surfaces);
            }
            if ((gripRightLocation_.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0) {
                controllerRenderR_.Render(out.Surfaces);
            }
        }

        if (handTrackedL_ && handInFrameL_) {
            handRendererL_.Render(out.Surfaces);
        }

        if (handTrackedR_ && handInFrameR_) {
            handRendererR_.Render(out.Surfaces);
        }

        /// Render beams last, since they render with transparency (alpha blending)
        cursorBeamRenderer_.Render(in, out);
    }

    virtual void SessionEnd() override {
        environmentRenderer_.Shutdown();
        skyboxRenderer_.Shutdown();
        controllerRenderL_.Shutdown();
        controllerRenderR_.Shutdown();
        cursorBeamRenderer_.Shutdown();

        /// Hand Trackers
        OXR(xrDestroyHandTrackerEXT_(handTrackerL_));
        OXR(xrDestroyHandTrackerEXT_(handTrackerR_));
        handRendererL_.Shutdown();
        handRendererR_.Shutdown();

        /// Switch out of simultaneous hands and controllers mode.
        /// Not strictly necessary; terminating the app will
        /// accomplish this as well
        setSimultaneousHandsControllersMode(false);
    }

    virtual void AppShutdown(const xrJava* context) override {
        /// Unhook extensions for hand tracking
        xrCreateHandTrackerEXT_ = nullptr;
        xrDestroyHandTrackerEXT_ = nullptr;
        xrLocateHandJointsEXT_ = nullptr;
        xrGetHandMeshFB_ = nullptr;
        xrResumeSimultaneousHandsControllersTrackingMETAX1_ = nullptr;
        xrPauseSimultaneousHandsControllersTrackingMETAX1_ = nullptr;

        OVRFW::XrApp::AppShutdown(context);
        ui_.Shutdown();
    }

    virtual void HandleXrEvents() override {
        XrEventDataBuffer eventDataBuffer = {};
        XrInteractionProfileState ipState{XR_TYPE_INTERACTION_PROFILE_STATE};

        // Poll for events
        for (;;) {
            XrEventDataBaseHeader* baseEventHeader = (XrEventDataBaseHeader*)(&eventDataBuffer);
            baseEventHeader->type = XR_TYPE_EVENT_DATA_BUFFER;
            baseEventHeader->next = NULL;
            XrResult r;
            OXR(r = xrPollEvent(Instance, &eventDataBuffer));
            if (r != XR_SUCCESS) {
                break;
            }

            switch (baseEventHeader->type) {
                case XR_TYPE_EVENT_DATA_EVENTS_LOST:
                    ALOGV("xrPollEvent: received XR_TYPE_EVENT_DATA_EVENTS_LOST event");
                    break;
                case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                    ALOGV("xrPollEvent: received XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING event");
                    break;
                case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
                    OXR(xrGetCurrentInteractionProfile(GetSession(), leftHandPath_, &ipState));
                    handTrackedL_ = (ipState.interactionProfile == msftHandInteractionProfile_);
                    OXR(xrGetCurrentInteractionProfile(GetSession(), rightHandPath_, &ipState));
                    handTrackedR_ = (ipState.interactionProfile == msftHandInteractionProfile_);
                    break;
                case XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT: {
                    const XrEventDataPerfSettingsEXT* perf_settings_event =
                        (XrEventDataPerfSettingsEXT*)(baseEventHeader);
                    ALOGV(
                        "xrPollEvent: received XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT event: type %d subdomain %d : level %d -> level %d",
                        perf_settings_event->type,
                        perf_settings_event->subDomain,
                        perf_settings_event->fromLevel,
                        perf_settings_event->toLevel);
                } break;
                case XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING:
                    ALOGV(
                        "xrPollEvent: received XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING event");
                    break;
                case XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: {
                    const XrEventDataSessionStateChanged* session_state_changed_event =
                        (XrEventDataSessionStateChanged*)(baseEventHeader);
                    ALOGV(
                        "xrPollEvent: received XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED: %d for session %p at time %f",
                        session_state_changed_event->state,
                        (void*)session_state_changed_event->session,
                        FromXrTime(session_state_changed_event->time));

                    switch (session_state_changed_event->state) {
                        case XR_SESSION_STATE_FOCUSED:
                            Focused = true;
                            break;
                        case XR_SESSION_STATE_VISIBLE:
                            Focused = false;
                            break;
                        case XR_SESSION_STATE_READY:
                            HandleSessionStateChanges(session_state_changed_event->state);
                            break;
                        case XR_SESSION_STATE_STOPPING:
                            HandleSessionStateChanges(session_state_changed_event->state);
                            break;
                        case XR_SESSION_STATE_EXITING:
                            ShouldExit = true;
                            break;
                        default:
                            break;
                    }
                } break;
                default:
                    ALOGV("xrPollEvent: Unknown event");
                    break;
            }
        }
    }

    void SetupHandTrackers() {
        XrHandTrackerCreateInfoEXT createInfo{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};
        createInfo.handJointSet = XR_HAND_JOINT_SET_DEFAULT_EXT;
        createInfo.hand = XR_HAND_LEFT_EXT;
        OXR(xrCreateHandTrackerEXT_(GetSession(), &createInfo, &handTrackerL_));
        createInfo.hand = XR_HAND_RIGHT_EXT;
        OXR(xrCreateHandTrackerEXT_(GetSession(), &createInfo, &handTrackerR_));

        ALOG("xrCreateHandTrackerEXT handTrackerL_=%llx", (long long)handTrackerL_);
        ALOG("xrCreateHandTrackerEXT handTrackerR_=%llx", (long long)handTrackerR_);

        /// Setup skinning meshes for both hands
        if (xrGetHandMeshFB_) {
            for (int handIndex = 0; handIndex < 2; ++handIndex) {
                /// Alias everything for initialization
                const bool isLeft = (handIndex == 0);
                auto& handTracker = isLeft ? handTrackerL_ : handTrackerR_;
                auto& handRenderer = isLeft ? handRendererL_ : handRendererR_;
                auto* jointLocations = isLeft ? jointLocationsL_ : jointLocationsR_;

                /// two-call pattern for mesh data
                /// call 1 - figure out sizes

                /// mesh
                XrHandTrackingMeshFB mesh{XR_TYPE_HAND_TRACKING_MESH_FB};
                mesh.next = nullptr;
                /// mesh - skeleton
                mesh.jointCapacityInput = 0;
                mesh.jointCountOutput = 0;
                mesh.jointBindPoses = nullptr;
                mesh.jointRadii = nullptr;
                mesh.jointParents = nullptr;
                /// mesh - vertex
                mesh.vertexCapacityInput = 0;
                mesh.vertexCountOutput = 0;
                mesh.vertexPositions = nullptr;
                mesh.vertexNormals = nullptr;
                mesh.vertexUVs = nullptr;
                mesh.vertexBlendIndices = nullptr;
                mesh.vertexBlendWeights = nullptr;
                /// mesh - index
                mesh.indexCapacityInput = 0;
                mesh.indexCountOutput = 0;
                mesh.indices = nullptr;
                /// get mesh sizes
                OXR(xrGetHandMeshFB_(handTracker, &mesh));

                /// mesh storage - update sizes
                mesh.jointCapacityInput = mesh.jointCountOutput;
                mesh.vertexCapacityInput = mesh.vertexCountOutput;
                mesh.indexCapacityInput = mesh.indexCountOutput;
                /// skeleton
                std::vector<XrPosef> jointBindLocations;
                std::vector<XrHandJointEXT> parentData;
                std::vector<float> jointRadii;
                jointBindLocations.resize(mesh.jointCountOutput);
                parentData.resize(mesh.jointCountOutput);
                jointRadii.resize(mesh.jointCountOutput);
                mesh.jointBindPoses = jointBindLocations.data();
                mesh.jointParents = parentData.data();
                mesh.jointRadii = jointRadii.data();
                /// vertex
                std::vector<XrVector3f> vertexPositions;
                std::vector<XrVector3f> vertexNormals;
                std::vector<XrVector2f> vertexUVs;
                std::vector<XrVector4sFB> vertexBlendIndices;
                std::vector<XrVector4f> vertexBlendWeights;
                vertexPositions.resize(mesh.vertexCountOutput);
                vertexNormals.resize(mesh.vertexCountOutput);
                vertexUVs.resize(mesh.vertexCountOutput);
                vertexBlendIndices.resize(mesh.vertexCountOutput);
                vertexBlendWeights.resize(mesh.vertexCountOutput);
                mesh.vertexPositions = vertexPositions.data();
                mesh.vertexNormals = vertexNormals.data();
                mesh.vertexUVs = vertexUVs.data();
                mesh.vertexBlendIndices = vertexBlendIndices.data();
                mesh.vertexBlendWeights = vertexBlendWeights.data();
                /// index
                std::vector<int16_t> indices;
                indices.resize(mesh.indexCountOutput);
                mesh.indices = indices.data();

                /// Get mesh data
                OXR(xrGetHandMeshFB_(handTracker, &mesh));

                /// Init renderer
                handRenderer.Init(&mesh, true);

                /// Print hierarchy
                {
                    for (int i = 0; i < XR_HAND_JOINT_COUNT_EXT; ++i) {
                        const OVR::Posef pose = FromXrPosef(jointLocations[i].pose);
                        ALOG(
                            " { {%.6f, %.6f, %.6f},  {%.6f, %.6f, %.6f, %.6f} } // joint = %d, parent = %d",
                            pose.Translation.x,
                            pose.Translation.y,
                            pose.Translation.z,
                            pose.Rotation.x,
                            pose.Rotation.y,
                            pose.Rotation.z,
                            pose.Rotation.w,
                            i,
                            (int)parentData[i]);
                    }
                }
            }
        }
    }

    void UpdateHandsInformation(const OVRFW::ovrApplFrameIn& in) {
        /// Left hand
        XrHandTrackingScaleFB scaleL{XR_TYPE_HAND_TRACKING_SCALE_FB};
        scaleL.next = nullptr;
        scaleL.sensorOutput = 1.0f;
        scaleL.currentOutput = 1.0f;
        scaleL.overrideValueInput = 1.00f;
        scaleL.overrideHandScale = XR_FALSE; // XR_TRUE;
        XrHandTrackingAimStateFB aimStateL{XR_TYPE_HAND_TRACKING_AIM_STATE_FB};
        aimStateL.next = &scaleL;
        XrHandJointLocationsEXT locationsL{XR_TYPE_HAND_JOINT_LOCATIONS_EXT};
        locationsL.next = &aimStateL;
        locationsL.jointCount = XR_HAND_JOINT_COUNT_EXT;
        locationsL.jointLocations = jointLocationsL_;
        /// Right hand
        XrHandTrackingScaleFB scaleR{XR_TYPE_HAND_TRACKING_SCALE_FB};
        scaleR.next = nullptr;
        scaleR.sensorOutput = 1.0f;
        scaleR.currentOutput = 1.0f;
        scaleR.overrideValueInput = 1.00f;
        scaleR.overrideHandScale = XR_FALSE; // XR_TRUE;
        XrHandTrackingAimStateFB aimStateR{XR_TYPE_HAND_TRACKING_AIM_STATE_FB};
        aimStateR.next = &scaleR;
        XrHandJointLocationsEXT locationsR{XR_TYPE_HAND_JOINT_LOCATIONS_EXT};
        locationsR.next = &aimStateR;
        locationsR.jointCount = XR_HAND_JOINT_COUNT_EXT;
        locationsR.jointLocations = jointLocationsR_;

        XrHandJointsLocateInfoEXT locateInfoL{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT};
        locateInfoL.baseSpace = GetStageSpace();
        locateInfoL.time = ToXrTime(in.PredictedDisplayTime);
        OXR(xrLocateHandJointsEXT_(handTrackerL_, &locateInfoL, &locationsL));

        XrHandJointsLocateInfoEXT locateInfoR{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT};
        locateInfoR.baseSpace = GetStageSpace();
        locateInfoR.time = ToXrTime(in.PredictedDisplayTime);
        OXR(xrLocateHandJointsEXT_(handTrackerR_, &locateInfoR, &locationsR));

        std::vector<OVR::Posef> handJointsL;
        std::vector<OVR::Posef> handJointsR;

        handInFrameL_ = false;
        handInFrameR_ = false;

        XrInteractionProfileState ipState{XR_TYPE_INTERACTION_PROFILE_STATE};
        OXR(xrGetCurrentInteractionProfile(GetSession(), leftHandPath_, &ipState));
        if (locationsL.isActive &&
            (ipState.interactionProfile == 0 ||
             ipState.interactionProfile == msftHandInteractionProfile_)) {
            handTrackedL_ = true;
            handInFrameL_ = true;
            handRendererL_.Update(&jointLocationsL_[0]);
            const bool didPinch =
                (aimStateL.status & XR_HAND_TRACKING_AIM_INDEX_PINCHING_BIT_FB) != 0;
            ui_.AddHitTestRay(FromXrPosef(aimStateL.aimPose), didPinch && !lastFrameClickedL_);
            lastFrameClickedL_ = didPinch;
        }

        OXR(xrGetCurrentInteractionProfile(GetSession(), rightHandPath_, &ipState));
        if (locationsR.isActive &&
            (ipState.interactionProfile == 0 ||
             ipState.interactionProfile == msftHandInteractionProfile_)) {
            handTrackedR_ = true;
            handInFrameR_ = true;
            handRendererR_.Update(&jointLocationsR_[0]);
            const bool didPinch =
                (aimStateR.status & XR_HAND_TRACKING_AIM_INDEX_PINCHING_BIT_FB) != 0;

            ui_.AddHitTestRay(FromXrPosef(aimStateR.aimPose), didPinch && !lastFrameClickedR_);
            lastFrameClickedR_ = didPinch;
        }
    }

    void CreateSampleDescriptionPanel() {
        // Panel to provide sample description to the user for context
        auto descriptionLabel = ui_.AddLabel(
            static_cast<std::string>(sampleExplanation), {3.5_m, 1.5_m, -1.5_m}, {950.0f, 600.0f});
        // Align and size the description text for readability
        OVRFW::VRMenuFontParms fontParams{};
        fontParams.Scale = 0.5f;
        fontParams.AlignHoriz = OVRFW::HORIZONTAL_LEFT;
        descriptionLabel->SetFontParms(fontParams);
        descriptionLabel->SetTextLocalPosition({-0.65_m, 0, 0});

        // Tilt the description billboard 45 degrees towards the user
        descriptionLabel->SetLocalRotation(
            OVR::Quat<float>::FromRotationVector({0, OVR::DegreeToRad(-45.0f), 0}));
    }

    bool isExtensionAvailable(std::string desiredExtension) {
        // Enumerate the extensions supported by the runtime
        uint32_t extensionCount;
        OXR(xrEnumerateInstanceExtensionProperties(nullptr, 0, &extensionCount, nullptr));
        std::vector<XrExtensionProperties> extensions(
            extensionCount, {XR_TYPE_EXTENSION_PROPERTIES});
        OXR(xrEnumerateInstanceExtensionProperties(
            nullptr, extensionCount, &extensionCount, extensions.data()));

        // Check if the desired extension is supported
        for (const auto& extension : extensions) {
            if (desiredExtension == extension.extensionName) {
                return true;
            }
        }
        return false;
    }

    void setSimultaneousHandsControllersMode(bool enabled) {
        // Enable/disable simultaneous hands and controllers mode
        if (enabled) {
            XrSimultaneousHandsControllersTrackingResumeInfoMETAX1 resumeInfo = {
                XR_TYPE_SIMULTANEOUS_HANDS_CONTROLLERS_TRACKING_RESUME_INFO_METAX1};
            OXR(xrResumeSimultaneousHandsControllersTrackingMETAX1_(GetSession(), &resumeInfo));
        } else {
            XrSimultaneousHandsControllersTrackingPauseInfoMETAX1 pauseInfo = {
                XR_TYPE_SIMULTANEOUS_HANDS_CONTROLLERS_TRACKING_PAUSE_INFO_METAX1};
            OXR(xrPauseSimultaneousHandsControllersTrackingMETAX1_(GetSession(), &pauseInfo));
        }
    }

   private:
    // Extension status
    bool isDetachedControllersExtensionAvailable_ = false;
    bool isMSFTHandInteractionExtensionAvailable_ = false;
    bool isSimultaneousHandsControllersExtensionAvailable_ = false;

    // Control of the simultaneous hands and controllers mode
    XrResult (XRAPI_PTR *xrResumeSimultaneousHandsControllersTrackingMETAX1_)(XrSession session,
        const XrSimultaneousHandsControllersTrackingResumeInfoMETAX1* createInfo) = nullptr;
    XrResult (XRAPI_PTR *xrPauseSimultaneousHandsControllersTrackingMETAX1_)(XrSession session,
        const XrSimultaneousHandsControllersTrackingPauseInfoMETAX1* createInfo) = nullptr;

    // Controllers
    OVRFW::ControllerRenderer controllerRenderL_;
    OVRFW::ControllerRenderer controllerRenderR_;
    OVRFW::EnvironmentRenderer environmentRenderer_;
    OVRFW::SkyboxRenderer skyboxRenderer_;
    OVRFW::SimpleBeamRenderer cursorBeamRenderer_;
    OVRFW::TinyUI ui_;

    // Hands
    bool lastFrameClickedL_ = false;
    bool lastFrameClickedR_ = false;

    /// Hands - extension functions
    PFN_xrCreateHandTrackerEXT xrCreateHandTrackerEXT_ = nullptr;
    PFN_xrDestroyHandTrackerEXT xrDestroyHandTrackerEXT_ = nullptr;
    PFN_xrLocateHandJointsEXT xrLocateHandJointsEXT_ = nullptr;

    /// Hands - FB mesh rendering extensions
    PFN_xrGetHandMeshFB xrGetHandMeshFB_ = nullptr;

    /// Hands - tracker handles
    XrHandTrackerEXT handTrackerL_ = XR_NULL_HANDLE;
    XrHandTrackerEXT handTrackerR_ = XR_NULL_HANDLE;

    XrHandJointLocationEXT jointLocationsL_[XR_HAND_JOINT_COUNT_EXT];
    XrHandJointLocationEXT jointLocationsR_[XR_HAND_JOINT_COUNT_EXT];

    OVRFW::HandRenderer handRendererL_;
    OVRFW::HandRenderer handRendererR_;
    bool handTrackedL_ = false;
    bool handTrackedR_ = false;
    bool handInFrameL_ = false;
    bool handInFrameR_ = false;
    OVR::Vector4f jointColor_{0.4, 0.5, 0.2, 0.5};

    XrActionSet actionSetMenu_{XR_NULL_HANDLE};
    XrActionSet actionSetWorld_{XR_NULL_HANDLE};
    XrActionSet actionSetGestures_{XR_NULL_HANDLE};

    XrAction actionSelect_{XR_NULL_HANDLE};
    XrAction actionGrab_{XR_NULL_HANDLE};
    XrAction actionControllerGripPose_{XR_NULL_HANDLE};
    XrAction actionControllerAimPose_{XR_NULL_HANDLE};

    XrAction actionHandSelect_{XR_NULL_HANDLE};
    XrAction actionHandSqueeze_{XR_NULL_HANDLE};
    XrAction actionHandGripPose_{XR_NULL_HANDLE};
    XrAction actionHandAimPose_{XR_NULL_HANDLE};

    XrAction actionDetachedControllerGripPose_{XR_NULL_HANDLE};
    XrAction actionDetachedControllerAimPose_{XR_NULL_HANDLE};

    XrAction actionTrigger_{XR_NULL_HANDLE};
    XrAction actionThumbstick_{XR_NULL_HANDLE};
    XrAction actionThumbstickX_{XR_NULL_HANDLE};
    XrAction actionThumbstickY_{XR_NULL_HANDLE};
    OVRFW::VRMenuObject* interactionProfileTextTitle_{nullptr};
    OVRFW::VRMenuObject* interactionProfileText_{nullptr};

    // Reference spaces
    XrSpace spaceStage_{XR_NULL_HANDLE};
    XrSpace spaceLocal_{XR_NULL_HANDLE};

    // Space for controller poses
    XrSpace spaceControllerAimLeft_{XR_NULL_HANDLE};
    XrSpace spaceControllerAimRight_{XR_NULL_HANDLE};
    XrSpace spaceDetachedControllerAimLeft_{XR_NULL_HANDLE};
    XrSpace spaceDetachedControllerAimRight_{XR_NULL_HANDLE};
    XrSpace spaceControllerGripLeft_{XR_NULL_HANDLE};
    XrSpace spaceControllerGripRight_{XR_NULL_HANDLE};

    // Space for hand poses
    XrSpace spaceHandAimLeft_{XR_NULL_HANDLE};
    XrSpace spaceHandAimRight_{XR_NULL_HANDLE};
    XrSpace spaceHandGripLeft_{XR_NULL_HANDLE};
    XrSpace spaceHandGripRight_{XR_NULL_HANDLE};

    XrSpace spaceDetachedControllerGripLeft_{XR_NULL_HANDLE};
    XrSpace spaceDetachedControllerGripRight_{XR_NULL_HANDLE};

    // Updated every frame
    XrSpaceLocation aimLeftLocation_{XR_TYPE_SPACE_LOCATION};
    XrSpaceLocation aimRightLocation_{XR_TYPE_SPACE_LOCATION};
    XrSpaceLocation gripRightLocation_{XR_TYPE_SPACE_LOCATION};
    XrSpaceLocation gripLeftLocation_{XR_TYPE_SPACE_LOCATION};

    // Hands
    XrSpaceLocation handAimLeftLocation_{XR_TYPE_SPACE_LOCATION};
    XrSpaceLocation handAimRightLocation_{XR_TYPE_SPACE_LOCATION};
    XrSpaceLocation handGripRightLocation_{XR_TYPE_SPACE_LOCATION};
    XrSpaceLocation handGripLeftLocation_{XR_TYPE_SPACE_LOCATION};

    // Detached controllers
    XrSpaceLocation aimDetachedLeftLocation_{XR_TYPE_SPACE_LOCATION};
    XrSpaceLocation aimDetachedRightLocation_{XR_TYPE_SPACE_LOCATION};
    XrSpaceLocation gripDetachedLeftLocation_{XR_TYPE_SPACE_LOCATION};
    XrSpaceLocation gripDetachedRightLocation_{XR_TYPE_SPACE_LOCATION};

    // XrPaths for convenience
    XrPath leftHandPath_{XR_NULL_PATH};
    XrPath rightHandPath_{XR_NULL_PATH};
    XrPath leftDetachedControllerPath_{XR_NULL_PATH};
    XrPath rightDetachedControllerPath_{XR_NULL_PATH};

    // Interaction profiles
    XrPath msftHandInteractionProfile_ = XR_NULL_PATH;
    XrPath touchProInteractionProfile_ = XR_NULL_PATH;
    XrPath touchPlusInteractionProfile_ = XR_NULL_PATH;

    std::unordered_map<XrActionSet, ActionSetDisplayPanel> actionSetPanels_{};
};

ENTRY_POINT(XrHandsAndControllersSampleApp)
