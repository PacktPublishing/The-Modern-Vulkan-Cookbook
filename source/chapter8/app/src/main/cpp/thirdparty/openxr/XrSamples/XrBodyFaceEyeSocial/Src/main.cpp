// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

/*******************************************************************************

Filename    :   Main.cpp
Content     :   Simple sample app to test body / face / eye social extensions
Created     :   September 2022
Authors     :   John Kearney

*******************************************************************************/

#include <cstdint>
#include <cstdio>
#include <array>

#include "XrApp.h"
#include <openxr/fb_eye_tracking_social.h>
#include <openxr/fb_face_tracking2.h>


#include "Input/SkeletonRenderer.h"
#include "Input/ControllerRenderer.h"
#include "Input/TinyUI.h"
#include "Input/AxisRenderer.h"
#include "Render/SimpleBeamRenderer.h"
#include "Render/GeometryRenderer.h"

class XrBodyFaceEyeSocialApp : public OVRFW::XrApp {
   public:
    static constexpr std::string_view sampleExplanation =
        "OpenXR Body / Face / Eye Social SDK Sample                        \n"
        "\n"
        "The extensions XR_FB_body_tracking; XR_FB_eye_tracking_social and \n"
        "XR_FB_face_tracking are designed to work together to support      \n"
        "querying devices for information associated with the body to allow\n"
        "to render an avatar of the user.                                  \n"
        "\n"
        "XR_FB_body_tracking allow applications to get poses of body joints\n"
        "XR_FB_face_tracking allows applications to get facial expressions.\n"
        "XR_FB_eye_tracking_social allows applications to get eye tracking \n"
        "information for social / avatar use-cases.                        \n";

    XrBodyFaceEyeSocialApp() : OVRFW::XrApp() {
        BackgroundColor = OVR::Vector4f(0.60f, 0.95f, 0.4f, 1.0f);
    }

    // Returns a list of OpenXr extensions needed for this app
    virtual std::vector<const char*> GetExtensions() override {
        std::vector<const char*> extensions = XrApp::GetExtensions();
        extensions.push_back(XR_FB_BODY_TRACKING_EXTENSION_NAME);
        extensions.push_back(XR_FB_EYE_TRACKING_SOCIAL_EXTENSION_NAME);
        extensions.push_back(XR_FB_FACE_TRACKING_EXTENSION_NAME);
        extensions.push_back(XR_FB_FACE_TRACKING2_EXTENSION_NAME);



        return extensions;
    }

    // Must return true if the application initializes successfully.
    virtual bool AppInit(const xrJava* context) override {
        if (false == ui_.Init(context, GetFileSys())) {
            ALOG("TinyUI::Init FAILED.");
            return false;
        }

        XrSystemBodyTrackingPropertiesFB bodyTrackingSystemProperties{
            XR_TYPE_SYSTEM_BODY_TRACKING_PROPERTIES_FB};
        XrSystemEyeTrackingPropertiesFB eyeTrackingSystemProperties{
            XR_TYPE_SYSTEM_EYE_TRACKING_PROPERTIES_FB};

        XrSystemFaceTrackingProperties2FB faceTrackingSystemProperties2 {
            XR_TYPE_SYSTEM_FACE_TRACKING_PROPERTIES2_FB};

        XrSystemFaceTrackingPropertiesFB faceTrackingSystemProperties{
            XR_TYPE_SYSTEM_FACE_TRACKING_PROPERTIES_FB, &faceTrackingSystemProperties2};

        eyeTrackingSystemProperties.next = &bodyTrackingSystemProperties;
        bodyTrackingSystemProperties.next = &faceTrackingSystemProperties;

        XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES};
        systemProperties.next = &eyeTrackingSystemProperties;

        OXR(xrGetSystemProperties(GetInstance(), GetSystemId(), &systemProperties));

        if (!bodyTrackingSystemProperties.supportsBodyTracking) {
            // The system does not support body tracking
            ALOG("xrGetSystemProperties XR_TYPE_SYSTEM_BODY_TRACKING_PROPERTIES_FB FAILED.");
        } else {
            ALOG(
                "xrGetSystemProperties XR_TYPE_SYSTEM_BODY_TRACKING_PROPERTIES_FB OK - initializing body tracking...");
            /// Hook up extensions for body tracking
            OXR(xrGetInstanceProcAddr(
                GetInstance(),
                "xrCreateBodyTrackerFB",
                (PFN_xrVoidFunction*)(&xrCreateBodyTrackerFB_)));
            OXR(xrGetInstanceProcAddr(
                GetInstance(),
                "xrDestroyBodyTrackerFB",
                (PFN_xrVoidFunction*)(&xrDestroyBodyTrackerFB_)));
            OXR(xrGetInstanceProcAddr(
                GetInstance(),
                "xrLocateBodyJointsFB",
                (PFN_xrVoidFunction*)(&xrLocateBodyJointsFB_)));
            OXR(xrGetInstanceProcAddr(
                GetInstance(), "xrGetBodySkeletonFB", (PFN_xrVoidFunction*)(&xrGetSkeletonFB_)));
        }

        if (!eyeTrackingSystemProperties.supportsEyeTracking) {
            // The system does not support eye tracking
            ALOG("xrGetSystemProperties XR_TYPE_SYSTEM_EYE_TRACKING_PROPERTIES_FB FAILED.");
        } else {
            ALOG(
                "xrGetSystemProperties XR_TYPE_SYSTEM_EYE_TRACKING_PROPERTIES_FB OK - initializing eye tracking...");
            /// Hook up extensions for eye tracking
            OXR(xrGetInstanceProcAddr(
                GetInstance(),
                "xrCreateEyeTrackerFB",
                (PFN_xrVoidFunction*)(&xrCreateEyeTrackerFB_)));
            OXR(xrGetInstanceProcAddr(
                GetInstance(),
                "xrDestroyEyeTrackerFB",
                (PFN_xrVoidFunction*)(&xrDestroyEyeTrackerFB_)));
            OXR(xrGetInstanceProcAddr(
                GetInstance(), "xrGetEyeGazesFB", (PFN_xrVoidFunction*)(&xrGetEyeGazesFB_)));
        }

         if (faceTrackingSystemProperties2.supportsAudioFaceTracking || faceTrackingSystemProperties2.supportsVisualFaceTracking) {
            ALOG(
                "xrGetSystemProperties XR_TYPE_SYSTEM_FACE_TRACKING_PROPERTIES2_FB OK - tongue and audio-driven face tracking are supported.");
            /// Hook up extensions for face tracking (v2)
            OXR(xrGetInstanceProcAddr(
                GetInstance(),
                "xrCreateFaceTracker2FB",
                (PFN_xrVoidFunction*)(&xrCreateFaceTracker2FB_)));
            OXR(xrGetInstanceProcAddr(
                GetInstance(),
                "xrDestroyFaceTracker2FB",
                (PFN_xrVoidFunction*)(&xrDestroyFaceTracker2FB_)));
            OXR(xrGetInstanceProcAddr(
                GetInstance(),
                "xrGetFaceExpressionWeights2FB",
                (PFN_xrVoidFunction*)(&xrGetFaceExpressionWeights2FB_)));
        } else if (faceTrackingSystemProperties.supportsFaceTracking) {
            ALOG(
                "xrGetSystemProperties XR_TYPE_SYSTEM_FACE_TRACKING_PROPERTIES_FB OK - initializing face tracking...");
            /// Hook up extensions for face tracking
            OXR(xrGetInstanceProcAddr(
                GetInstance(),
                "xrCreateFaceTrackerFB",
                (PFN_xrVoidFunction*)(&xrCreateFaceTrackerFB_)));
            OXR(xrGetInstanceProcAddr(
                GetInstance(),
                "xrDestroyFaceTrackerFB",
                (PFN_xrVoidFunction*)(&xrDestroyFaceTrackerFB_)));
            OXR(xrGetInstanceProcAddr(
                GetInstance(),
                "xrGetFaceExpressionWeightsFB",
                (PFN_xrVoidFunction*)(&xrGetFaceExpressionWeightsFB_)));
        } else {
            // The system does not support face tracking
            ALOG("xrGetSystemProperties XR_TYPE_SYSTEM_FACE_TRACKING_PROPERTIES_FB FAILED.");
        }

        return true;
    }

    virtual void AppShutdown(const xrJava* context) override {
        /// unhook extensions for body tracking
        xrCreateBodyTrackerFB_ = nullptr;
        xrDestroyBodyTrackerFB_ = nullptr;
        xrLocateBodyJointsFB_ = nullptr;
        xrGetSkeletonFB_ = nullptr;

        /// unhook extensions for eye tracking
        xrCreateEyeTrackerFB_ = nullptr;
        xrDestroyEyeTrackerFB_ = nullptr;
        xrGetEyeGazesFB_ = nullptr;

        /// unhook extensions for face tracking
        xrCreateFaceTrackerFB_ = nullptr;
        xrDestroyFaceTrackerFB_ = nullptr;
        xrGetFaceExpressionWeightsFB_ = nullptr;

        /// unhook extensions for face tracking (v2)
        xrCreateFaceTracker2FB_ = nullptr;
        xrDestroyFaceTracker2FB_ = nullptr;
        xrGetFaceExpressionWeights2FB_ = nullptr;

        OVRFW::XrApp::AppShutdown(context);
        ui_.Shutdown();
    }

    virtual bool SessionInit() override {
        CreateSampleDescriptionPanel();

        /// Disable scene navitgation
        GetScene().SetFootPos({0.0f, 0.0f, 0.0f});
        this->FreeMove = false;
        /// Init session bound objects
        if (false == controllerRenderL_.Init(true)) {
            ALOG("AppInit::Init L controller renderer FAILED.");
            return false;
        }
        if (false == controllerRenderR_.Init(false)) {
            ALOG("AppInit::Init R controller renderer FAILED.");
            return false;
        }
        beamRenderer_.Init(GetFileSys(), nullptr, OVR::Vector4f(1.0f), 1.0f);

        {
            // We want to draw our body in front of us so that we can see what we look like
            XrQuaternionf bodyOrientation{};
            XrVector3f upAxis{0, 1, 0};
            XrQuaternionf_CreateFromAxisAngle(&bodyOrientation, &upAxis, 120 * MATH_PI / 180);

            XrReferenceSpaceCreateInfo spaceCreateInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
            spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_STAGE;
            spaceCreateInfo.poseInReferenceSpace.position.z = 1.0f;
            spaceCreateInfo.poseInReferenceSpace.orientation = bodyOrientation;
            OXR(xrCreateReferenceSpace(Session, &spaceCreateInfo, &bodySpace));
        }

        if (xrCreateBodyTrackerFB_) {
            XrBodyTrackerCreateInfoFB createInfo{XR_TYPE_BODY_TRACKER_CREATE_INFO_FB};
            createInfo.bodyJointSet = XR_BODY_JOINT_SET_DEFAULT_FB;
            OXR(xrCreateBodyTrackerFB_(GetSession(), &createInfo, &bodyTracker_));
            ALOG("xrCreateBodyTrackerFB bodyTracker_=%llx", (long long)bodyTracker_);
        }

        if (xrCreateEyeTrackerFB_) {
            XrEyeTrackerCreateInfoFB createInfo{XR_TYPE_EYE_TRACKER_CREATE_INFO_FB};
            OXR(xrCreateEyeTrackerFB_(GetSession(), &createInfo, &eyeTracker_));
            ALOG("xrCreateEyeTrackerFB eyeTracker_=%llx", (long long)eyeTracker_);
        }

        if (xrCreateFaceTracker2FB_) {
            // Request face tracking data from visual or audio data source
            XrFaceTrackerCreateInfo2FB createInfo{XR_TYPE_FACE_TRACKER_CREATE_INFO2_FB};
            createInfo.faceExpressionSet = XR_FACE_EXPRESSION_SET2_DEFAULT_FB;
            createInfo.requestedDataSourceCount = 2;
            XrFaceTrackingDataSource2FB dataSources[2] = {
                XR_FACE_TRACKING_DATA_SOURCE2_VISUAL_FB,
                XR_FACE_TRACKING_DATA_SOURCE2_AUDIO_FB};
            createInfo.requestedDataSources = dataSources;
            OXR(xrCreateFaceTracker2FB_(GetSession(), &createInfo, &faceTracker2_));
            ALOG("xrCreateFaceTracker2FB faceTracker2_=%llx", (long long)faceTracker_);
        } else if (xrCreateFaceTrackerFB_) {
            XrFaceTrackerCreateInfoFB createInfo{XR_TYPE_FACE_TRACKER_CREATE_INFO_FB};
            OXR(xrCreateFaceTrackerFB_(GetSession(), &createInfo, &faceTracker_));
            ALOG("xrCreateFaceTrackerFB faceTracker_=%llx", (long long)faceTracker_);
        }

        /// Body rendering
        axisRenderer_.Init();

        // Skip root and hips
        bodySkeletonRenderers.resize(XR_BODY_JOINT_COUNT_FB - 2);

        eyeRenderers.resize(2);
        for (auto& gr : eyeRenderers) {
            gr.Init(OVRFW::BuildTesselatedConeDescriptor(0.02f, 0.03f, 7, 7, 0.01, 0.01));
            gr.DiffuseColor = eyeColor_;
        }

        // Label for mouth
        mouthLabel_ = ui_.AddLabel(
            EmojiExpressionString[(size_t)emojiExpression_], {2.0f, 0.5f, -1.5f}, {250.0f, 100.0f});
        mouthLabel_->SetLocalRotation(
            OVR::Quat<float>::FromRotationVector({0, OVR::DegreeToRad(-45.0f), 0}));

        return true;
    }

    virtual void SessionEnd() override {
        if (xrDestroyBodyTrackerFB_) {
            OXR(xrDestroyBodyTrackerFB_(bodyTracker_));
        }
        if (xrDestroyEyeTrackerFB_) {
            OXR(xrDestroyEyeTrackerFB_(eyeTracker_));
        }
        if (xrDestroyFaceTracker2FB_) {
            OXR(xrDestroyFaceTracker2FB_(faceTracker2_));
        } else if (xrDestroyFaceTrackerFB_) {
            OXR(xrDestroyFaceTrackerFB_(faceTracker_));
        }

        controllerRenderL_.Shutdown();
        controllerRenderR_.Shutdown();
        ui_.Shutdown();
        beamRenderer_.Shutdown();
        axisRenderer_.Shutdown();

        for (auto& gr : bodySkeletonRenderers) {
            gr.Shutdown();
        }
        for (auto& gr : eyeRenderers) {
            gr.Shutdown();
        }
    }

    // Update state
    virtual void Update(const OVRFW::ovrApplFrameIn& in) override {
        auto isValid = [](XrSpaceLocationFlags flags) -> bool {
            constexpr XrSpaceLocationFlags isValid =
                XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT;
            return (flags & isValid) != 0;
        };

        ui_.HitTestDevices().clear();

        /// Body
        if (bodyTracker_ != XR_NULL_HANDLE) {
            XrBodyJointLocationsFB locations{XR_TYPE_BODY_JOINT_LOCATIONS_FB};
            locations.jointCount = XR_BODY_JOINT_COUNT_FB;
            locations.jointLocations = jointLocations_;

            XrBodySkeletonFB skeleton{XR_TYPE_BODY_SKELETON_FB};
            skeleton.jointCount = XR_BODY_JOINT_COUNT_FB;
            skeleton.joints = skeletonJoints_;

            XrBodyJointsLocateInfoFB locateInfo{XR_TYPE_BODY_JOINTS_LOCATE_INFO_FB};
            locateInfo.baseSpace = bodySpace;
            locateInfo.time = ToXrTime(in.PredictedDisplayTime);

            OXR(xrLocateBodyJointsFB_(bodyTracker_, &locateInfo, &locations));

            if (locations.skeletonChangedCount != skeletonChangeCount_) {
                skeletonChangeCount_ = locations.skeletonChangedCount;
                ALOG("BodySkeleton: skeleton proportions have changed.");

                OXR(xrGetSkeletonFB_(bodyTracker_, &skeleton));
                // Skip root and hips
                for (int i = 2; i < XR_BODY_JOINT_COUNT_FB; ++i) {
                    const auto fromJoint = jointLocations_[skeleton.joints[i].parentJoint];
                    const auto toJoint = jointLocations_[skeleton.joints[i].joint];

                    // calculation length and orientation between this two points?
                    const auto p0 = FromXrVector3f(fromJoint.pose.position);
                    const auto p1 = FromXrVector3f(toJoint.pose.position);
                    const auto d = (p1 - p0);

                    const float h = d.Length();

                    // Skip root and hips
                    OVRFW::GeometryRenderer& gr = bodySkeletonRenderers[i - 2];
                    gr.Shutdown();
                    gr.Init(OVRFW::BuildTesselatedCapsuleDescriptor(0.01f, h, 7, 7));
                    gr.DiffuseColor = jointColor_;
                }
            }

            std::vector<OVR::Posef> bodyJoints;
            if (locations.isActive) {
                // Tracked joints and computed joints can all be valid (or not)
                bodyTracked_ = true;

                for (int i = 0; i < XR_BODY_JOINT_COUNT_FB; ++i) {
                    if (isValid(jointLocations_[i].locationFlags)) {
                        bodyJoints.push_back(FromXrPosef(jointLocations_[i].pose));
                    }
                }

                // Display hierarchy
                if (skeletonChangeCount_ != 0) {
                    // Skip root and hips
                    for (int i = 2; i < XR_BODY_JOINT_COUNT_FB; ++i) {
                        const auto fromJoint = jointLocations_[skeleton.joints[i].parentJoint];
                        const auto toJoint = jointLocations_[skeleton.joints[i].joint];

                        if (isValid(fromJoint.locationFlags) && isValid(toJoint.locationFlags)) {
                            // calculation length and orientation between this two points?
                            const auto p0 = FromXrVector3f(fromJoint.pose.position);
                            const auto p1 = FromXrVector3f(toJoint.pose.position);
                            const auto d = (p1 - p0);

                            const OVR::Quatf look = OVR::Quatf::LookRotation(d.Normalized(), {0, 1, 0});
                            /// apply inverse scale here
                            const float h = d.Length();
                            const OVR::Vector3f start = p0 + look.Rotate(OVR::Vector3f(0, 0, -h / 2));

                            // Skip root and hips
                            OVRFW::GeometryRenderer& gr = bodySkeletonRenderers[i - 2];
                            gr.SetScale({1, 1, 1});
                            gr.SetPose(OVR::Posef(look, start));
                            gr.Update();
                        }
                    }
                }
            }

            axisRenderer_.Update(bodyJoints);
        }

        /// Head position
        XrSpaceLocation viewLocation{XR_TYPE_SPACE_LOCATION};
        OXR(xrLocateSpace(HeadSpace, bodySpace, ToXrTime(in.PredictedDisplayTime), &viewLocation));

        /// Eyes
        if (eyeTracker_ != XR_NULL_HANDLE) {
            if ((viewLocation.locationFlags & XR_SPACE_LOCATION_POSITION_VALID_BIT) != 0 &&
                (viewLocation.locationFlags & XR_SPACE_LOCATION_ORIENTATION_VALID_BIT) != 0) {
                // NOTE:
                // The returned eye poses (position and rotation) in `XrEyeGazesFB` struct may be
                // from an older timestamp than the `time` requested in the `XrEyeGazesInfoFB` or
                // the latest head/body pose. We can see the timestamp of the (output) eye poses
                // in the (output) `time` field of the `XrEyeGazesFB` struct.
                //
                // The simpliest way of synchronising the eye positions with the head/body poses
                // (and therefore avoiding jitter in the eye poses during head/body movements) is
                // to request (the eye) poses in the head (VIEW) space and transform the result
                // to the head/body space later.
                OVR::Posef headPose = FromXrPosef(viewLocation.pose);

                XrEyeGazesFB eyeGazes{XR_TYPE_EYE_GAZES_FB};

                XrEyeGazesInfoFB gazesInfo{XR_TYPE_EYE_GAZES_INFO_FB};
                gazesInfo.baseSpace = HeadSpace;
                gazesInfo.time = ToXrTime(in.PredictedDisplayTime);

                OXR(xrGetEyeGazesFB_(eyeTracker_, &gazesInfo, &eyeGazes));

                static_assert(std::size(eyeGazes.gaze) == 2);
                for (size_t eye = 0; eye < std::size(eyeGazes.gaze); ++eye) {
                    const OVR::Posef pose = headPose * FromXrPosef(eyeGazes.gaze[eye].gazePose);
                    OVRFW::GeometryRenderer& gr = eyeRenderers[eye];
                    gr.SetScale({1, 1, 1});
                    gr.SetPose(pose);
                    gr.Update();
                }
            }
        }

        // NOTE - The following logic is used only to showcase how face tracking works.
        // It is NOT intended to be used as a reliable mechanism to detect facial expressions.
        auto simpleExpression =
            [](const XrFaceExpressionWeightsFB& expressionWeights) -> EmojiExpression {
            if (expressionWeights.weights[XR_FACE_EXPRESSION_LIP_CORNER_PULLER_L_FB] > 0.5 &&
                expressionWeights.weights[XR_FACE_EXPRESSION_LIP_CORNER_PULLER_R_FB] > 0.5) {
                return EmojiExpression::Smile;
            } else if (
                expressionWeights.weights[XR_FACE_EXPRESSION_LIP_PUCKER_L_FB] > 0.25 &&
                expressionWeights.weights[XR_FACE_EXPRESSION_LIP_PUCKER_R_FB] > 0.25) {
                return EmojiExpression::Kiss;
            } else {
                return EmojiExpression::Neutral;
            }
        };

        // NOTE - The following logic is used only to showcase how face tracking works.
        // It is NOT intended to be used as a reliable mechanism to detect facial expressions.
        auto simpleExpression2 =
            [](const XrFaceExpressionWeights2FB& expressionWeights) -> EmojiExpression {
            // If source is audio and any of the weights are non-zero, then audio-driven expression.
            if (expressionWeights.dataSource == XR_FACE_TRACKING_DATA_SOURCE2_AUDIO_FB) {
                for (int i = 0; i < XR_FACE_EXPRESSION2_COUNT_FB; ++i) {
                    if (expressionWeights.weights[i] > 0.01) {
                        return EmojiExpression::AudioDriven;
                    }
                }
                return EmojiExpression::Neutral;
            } else if (expressionWeights.weights[XR_FACE_EXPRESSION2_LIP_CORNER_PULLER_L_FB] > 0.5 &&
                expressionWeights.weights[XR_FACE_EXPRESSION2_LIP_CORNER_PULLER_R_FB] > 0.5) {
                return EmojiExpression::Smile;
            } else if (
                expressionWeights.weights[XR_FACE_EXPRESSION2_LIP_PUCKER_L_FB] > 0.25 &&
                expressionWeights.weights[XR_FACE_EXPRESSION2_LIP_PUCKER_R_FB] > 0.25) {
                return EmojiExpression::Kiss;
             } else if (expressionWeights.weights[XR_FACE_EXPRESSION2_TONGUE_OUT_FB] > 0.5) {
                return EmojiExpression::TongueOut;
            } else {
                return EmojiExpression::Neutral;
            }
        };

        /// Face
       if (faceTracker2_ != XR_NULL_HANDLE) {
            float weights_[XR_FACE_EXPRESSION2_COUNT_FB] = {};
            float confidence_[XR_FACE_CONFIDENCE2_COUNT_FB] = {};

            XrFaceExpressionWeights2FB expressionWeights{XR_TYPE_FACE_EXPRESSION_WEIGHTS2_FB};
            expressionWeights.next = nullptr;
            expressionWeights.weights = weights_;
            expressionWeights.confidences = confidence_;
            expressionWeights.weightCount = XR_FACE_EXPRESSION2_COUNT_FB;
            expressionWeights.confidenceCount = XR_FACE_CONFIDENCE2_COUNT_FB;

            XrFaceExpressionInfo2FB expressionInfo{XR_TYPE_FACE_EXPRESSION_INFO2_FB};
            expressionInfo.time = ToXrTime(in.PredictedDisplayTime);


            {
                OXR(xrGetFaceExpressionWeights2FB_(faceTracker2_, &expressionInfo, &expressionWeights));
            }

            emojiExpression_ = simpleExpression2(expressionWeights);

            mouthLabel_->SetText(EmojiExpressionString[(size_t)emojiExpression_]);
        } else if (faceTracker_ != XR_NULL_HANDLE) {
            float weights_[XR_FACE_EXPRESSION_COUNT_FB] = {};
            float confidence_[XR_FACE_CONFIDENCE_COUNT_FB] = {};

            XrFaceExpressionWeightsFB expressionWeights{XR_TYPE_FACE_EXPRESSION_WEIGHTS_FB};
            expressionWeights.next = nullptr;
            expressionWeights.weights = weights_;
            expressionWeights.confidences = confidence_;
            expressionWeights.weightCount = XR_FACE_EXPRESSION_COUNT_FB;
            expressionWeights.confidenceCount = XR_FACE_CONFIDENCE_COUNT_FB;

            XrFaceExpressionInfoFB expressionInfo{XR_TYPE_FACE_EXPRESSION_INFO_FB};
            expressionInfo.time = ToXrTime(in.PredictedDisplayTime);


            {
                OXR(xrGetFaceExpressionWeightsFB_(faceTracker_, &expressionInfo, &expressionWeights));
            }

            emojiExpression_ = simpleExpression(expressionWeights);

            mouthLabel_->SetText(EmojiExpressionString[(size_t)emojiExpression_]);
        }

        if (in.LeftRemoteTracked) {
            controllerRenderL_.Update(in.LeftRemotePose);
            const bool didPinch = in.LeftRemoteIndexTrigger > 0.5f;
            ui_.AddHitTestRay(in.LeftRemotePointPose, didPinch);
        }
        if (in.RightRemoteTracked) {
            controllerRenderR_.Update(in.RightRemotePose);
            const bool didPinch = in.RightRemoteIndexTrigger > 0.5f;
            ui_.AddHitTestRay(in.RightRemotePointPose, didPinch);
        }

        ui_.Update(in);
        beamRenderer_.Update(in, ui_.HitTestDevices());
    }

    // Render eye buffers while running
    virtual void Render(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out) override {
        /// Render UI
        ui_.Render(in, out);

        /// Render controllers
        if (in.LeftRemoteTracked) {
            controllerRenderL_.Render(out.Surfaces);
        }
        if (in.RightRemoteTracked) {
            controllerRenderR_.Render(out.Surfaces);
        }

        /// Render body
        if (bodyTracked_) {
            axisRenderer_.Render(OVR::Matrix4f(), in, out);

            for (auto& gr : bodySkeletonRenderers) {
                gr.Render(out.Surfaces);
            }
        }

        /// Render eyes
        axisRenderer_.Render(OVR::Matrix4f(), in, out);

        for (auto& gr : eyeRenderers) {
            gr.Render(out.Surfaces);
        }

        /// Render beams
        beamRenderer_.Render(in, out);
    }

    void CreateSampleDescriptionPanel() {
        // Panel to provide sample description to the user for context
        auto descriptionLabel = ui_.AddLabel(
            static_cast<std::string>(sampleExplanation), {2.0f, 1.5f, -1.5f}, {950.0f, 600.0f});

        // Align and size the description text for readability
        OVRFW::VRMenuFontParms fontParams{};
        fontParams.Scale = 0.5f;
        fontParams.AlignHoriz = OVRFW::HORIZONTAL_LEFT;
        descriptionLabel->SetFontParms(fontParams);
        descriptionLabel->SetTextLocalPosition({-0.65f, 0, 0});

        // Tilt the description billboard 45 degrees towards the user
        descriptionLabel->SetLocalRotation(
            OVR::Quat<float>::FromRotationVector({0, OVR::DegreeToRad(-45.0f), 0}));
    }

   public:
    /// Body - extension functions
    PFN_xrCreateBodyTrackerFB xrCreateBodyTrackerFB_ = nullptr;
    PFN_xrDestroyBodyTrackerFB xrDestroyBodyTrackerFB_ = nullptr;
    PFN_xrLocateBodyJointsFB xrLocateBodyJointsFB_ = nullptr;
    PFN_xrGetBodySkeletonFB xrGetSkeletonFB_ = nullptr;

    /// Body - tracker handle
    XrBodyTrackerFB bodyTracker_ = XR_NULL_HANDLE;

    /// Body - data buffers
    XrBodyJointLocationFB jointLocations_[XR_BODY_JOINT_COUNT_FB];
    XrBodySkeletonJointFB skeletonJoints_[XR_BODY_JOINT_COUNT_FB];
    XrSpace bodySpace = XR_NULL_HANDLE;

    /// Eyes - extension functions
    PFN_xrCreateEyeTrackerFB xrCreateEyeTrackerFB_ = nullptr;
    PFN_xrDestroyEyeTrackerFB xrDestroyEyeTrackerFB_ = nullptr;
    PFN_xrGetEyeGazesFB xrGetEyeGazesFB_ = nullptr;

    /// Eyes - tracker handle
    XrEyeTrackerFB eyeTracker_ = XR_NULL_HANDLE;

    /// Face - extension functions
    PFN_xrCreateFaceTrackerFB xrCreateFaceTrackerFB_ = nullptr;
    PFN_xrDestroyFaceTrackerFB xrDestroyFaceTrackerFB_ = nullptr;
    PFN_xrGetFaceExpressionWeightsFB xrGetFaceExpressionWeightsFB_ = nullptr;

    /// Face (v2) - extension functions
    PFN_xrCreateFaceTracker2FB xrCreateFaceTracker2FB_ = nullptr;
    PFN_xrDestroyFaceTracker2FB xrDestroyFaceTracker2FB_ = nullptr;
    PFN_xrGetFaceExpressionWeights2FB xrGetFaceExpressionWeights2FB_ = nullptr;

    /// Face - tracker handle
    XrFaceTrackerFB faceTracker_ = XR_NULL_HANDLE;
    XrFaceTracker2FB faceTracker2_ = XR_NULL_HANDLE;

    /// Face data
    enum class EmojiExpression {
        Neutral = 0,
        Smile = 1,
        Kiss = 2,
        TongueOut = 3,
        AudioDriven = 4,
        COUNT
    };

    std::array<const char*, static_cast<size_t>(EmojiExpression::COUNT)> EmojiExpressionString{
        "Neutral Expression",
        "Smile Expression",
        "Kiss Expression",
        "Tongue Out Expression",
        "Audio-driven Expression",
    };

    EmojiExpression emojiExpression_ = EmojiExpression::Neutral;

   private:
    OVRFW::ControllerRenderer controllerRenderL_;
    OVRFW::ControllerRenderer controllerRenderR_;
    OVRFW::TinyUI ui_;
    OVRFW::SimpleBeamRenderer beamRenderer_;
    std::vector<OVRFW::ovrBeamRenderer::handle_t> beams_;
    OVRFW::ovrAxisRenderer axisRenderer_;

    OVR::Vector4f jointColor_{0.4, 0.5, 0.2, 0.5};
    OVR::Vector4f eyeColor_{0.3, 0.2, 0.4, 1.};
    std::vector<OVRFW::GeometryRenderer> bodySkeletonRenderers;
    std::vector<OVRFW::GeometryRenderer> eyeRenderers;
    OVRFW::VRMenuObject* mouthLabel_;

    bool bodyTracked_ = false;
    uint32_t skeletonChangeCount_ = 0;

};

ENTRY_POINT(XrBodyFaceEyeSocialApp)
