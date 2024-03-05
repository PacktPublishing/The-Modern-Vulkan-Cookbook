// (c) Meta Platforms, Inc. and affiliates.

/*******************************************************************************

Sample app for FB_body_tracking

*******************************************************************************/

#include <cstdint>
#include <cstdio>

#include "XrApp.h"

#include "Input/SkeletonRenderer.h"
#include "Input/ControllerRenderer.h"
#include "Input/TinyUI.h"
#include "Input/AxisRenderer.h"
#include "Render/SimpleBeamRenderer.h"

#include <openxr/meta_body_tracking_full_body.h>
#include <openxr/meta_body_tracking_fidelity.h>
#include <openxr/meta_body_tracking_calibration.h>

class XrBodyApp : public OVRFW::XrApp {
   public:
    XrBodyApp() : OVRFW::XrApp() {
        BackgroundColor = OVR::Vector4f(0.60f, 0.95f, 0.4f, 1.0f);
    }

    // Returns a list of OpenXr extensions needed for this app
    virtual std::vector<const char*> GetExtensions() override {
        std::vector<const char*> extensions = XrApp::GetExtensions();
        extensions.push_back(XR_FB_BODY_TRACKING_EXTENSION_NAME);
        extensions.push_back(XR_META_BODY_TRACKING_FIDELITY_EXTENSION_NAME);
        extensions.push_back(XR_META_BODY_TRACKING_FULL_BODY_EXTENSION_NAME);
        extensions.push_back(XR_META_BODY_TRACKING_CALIBRATION_EXTENSION_NAME);
        return extensions;
    }

    // Must return true if the application initializes successfully.
    virtual bool AppInit(const xrJava* context) override {
        if (false == ui_.Init(context, GetFileSys())) {
            ALOG("TinyUI::Init FAILED.");
            return false;
        }
        /// Build UI
        ui_.AddLabel("OpenXR Body Sample", {0.1f, 1.25f, -2.0f}, {1300.0f, 100.0f});

        // Inspect body tracking system properties
        XrSystemBodyTrackingPropertiesFB bodyTrackingSystemProperties{
            XR_TYPE_SYSTEM_BODY_TRACKING_PROPERTIES_FB};
        XrSystemProperties systemProperties{
            XR_TYPE_SYSTEM_PROPERTIES, &bodyTrackingSystemProperties};
        XrSystemPropertiesBodyTrackingFullBodyMETA fullBodyTrackingSystemProperties{
            XR_TYPE_SYSTEM_PROPERTIES_BODY_TRACKING_FULL_BODY_META, nullptr};
        XrSystemPropertiesBodyTrackingCalibrationMETA calibrationSystemProperties{
            XR_TYPE_SYSTEM_PROPERTIES_BODY_TRACKING_CALIBRATION_META, &fullBodyTrackingSystemProperties};
        XrSystemPropertiesBodyTrackingFidelityMETA fidelitySystemProperties{
            XR_TYPE_SYSTEM_PROPERTIES_BODY_TRACKING_FIDELITY_META, &calibrationSystemProperties};
        bodyTrackingSystemProperties.next = &fidelitySystemProperties;
        OXR(xrGetSystemProperties(GetInstance(), GetSystemId(), &systemProperties));
        if (!bodyTrackingSystemProperties.supportsBodyTracking) {
            // The system does not support body tracking
            ALOG("xrGetSystemProperties XR_TYPE_SYSTEM_BODY_TRACKING_PROPERTIES_FB FAILED.");
            return false;
        } else {
            ALOG(
                "xrGetSystemProperties XR_TYPE_SYSTEM_BODY_TRACKING_PROPERTIES_FB OK - initializing body tracking...");
        }
        if (!fullBodyTrackingSystemProperties.supportsFullBodyTracking) {
            // The system does not support full body tracking.
            ALOG("xrGetSystemProperties XR_TYPE_SYSTEM_PROPERTIES_BODY_TRACKING_FULL_BODY_META FAILED.");
        } else {
            supportsFullBody_ = true;
        }

        if (!fidelitySystemProperties.supportsBodyTrackingFidelity) {
            // The system does not support body tracking fidelity.
            ALOG("xrGetSystemProperties XR_TYPE_SYSTEM_PROPERTIES_BODY_TRACKING_FIDELITY_META FAILED.");
        } else {
            supportsFidelity_ = true;
        }

        if (!calibrationSystemProperties.supportsHeightOverride) {
            // The system does not support body tracking calibration overrides.
            ALOG("xrGetSystemProperties XR_TYPE_SYSTEM_PROPERTIES_BODY_TRACKING_CALIBRATION_META FAILED.");
        } else {
            supportsCalibrationOverride_ = true;
        }

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
            GetInstance(), "xrLocateBodyJointsFB", (PFN_xrVoidFunction*)(&xrLocateBodyJointsFB_)));
        OXR(xrGetInstanceProcAddr(
            GetInstance(), "xrGetBodySkeletonFB", (PFN_xrVoidFunction*)(&xrGetSkeletonFB_)));

        if (supportsFidelity_) {
            OXR(xrGetInstanceProcAddr(
                GetInstance(), "xrRequestBodyTrackingFidelityMETA", (PFN_xrVoidFunction*)(&xrRequestBodyTrackingFidelityMETA_)));
        }

        if (supportsCalibrationOverride_) {
            OXR(xrGetInstanceProcAddr(
                GetInstance(), "xrSuggestBodyTrackingCalibrationOverrideMETA", (PFN_xrVoidFunction*)(&xrSuggestBodyTrackingCalibrationOverrideMETA_)));
        }
        return true;
    }

    virtual void AppShutdown(const xrJava* context) override {
        /// unhook extensions for body tracking
        xrCreateBodyTrackerFB_ = nullptr;
        xrDestroyBodyTrackerFB_ = nullptr;
        xrLocateBodyJointsFB_ = nullptr;
        xrGetSkeletonFB_ = nullptr;

        xrRequestBodyTrackingFidelityMETA_ = nullptr;
        xrSuggestBodyTrackingCalibrationOverrideMETA_ = nullptr;

        OVRFW::XrApp::AppShutdown(context);
        ui_.Shutdown();
    }

    virtual bool SessionInit() override {
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

        /// Body Trackers
        if (xrCreateBodyTrackerFB_) {
            XrBodyTrackerCreateInfoFB createInfo{XR_TYPE_BODY_TRACKER_CREATE_INFO_FB};
            createInfo.bodyJointSet = supportsFullBody_ ? XR_BODY_JOINT_SET_FULL_BODY_META : XR_BODY_JOINT_SET_DEFAULT_FB;
            OXR(xrCreateBodyTrackerFB_(GetSession(), &createInfo, &bodyTracker_));

            if (supportsFidelity_) {
                OXR(xrRequestBodyTrackingFidelityMETA_(bodyTracker_, XrBodyTrackingFidelityMETA::XR_BODY_TRACKING_FIDELITY_HIGH_META));
            }

            if (supportsCalibrationOverride_) {
                XrBodyTrackingCalibrationInfoMETA calibrationInfo{XR_TYPE_BODY_TRACKING_CALIBRATION_INFO_META};
                calibrationInfo.bodyHeight = 2.0f;
                OXR(xrSuggestBodyTrackingCalibrationOverrideMETA_(bodyTracker_, &calibrationInfo));
            }
            ALOG("xrCreateBodyTrackerFB bodyTracker_=%llx", (long long)bodyTracker_);
        }

        /// Body rendering
        axisRenderer_.Init();

        return true;
    }

    virtual void SessionEnd() override {
        /// Body Tracker
        if (xrDestroyBodyTrackerFB_) {
            OXR(xrDestroyBodyTrackerFB_(bodyTracker_));
        }
        controllerRenderL_.Shutdown();
        controllerRenderR_.Shutdown();
        beamRenderer_.Shutdown();
        axisRenderer_.Shutdown();
    }

    // Update state
    virtual void Update(const OVRFW::ovrApplFrameIn& in) override {
        ui_.HitTestDevices().clear();

        /// Body
        if (bodyTracker_ != XR_NULL_HANDLE) {
            XrBodyJointLocationsFB locations{XR_TYPE_BODY_JOINT_LOCATIONS_FB};
            locations.next = nullptr;
            locations.jointCount = supportsFullBody_ ? static_cast<int>(XR_FULL_BODY_JOINT_COUNT_META) : static_cast<int>(XR_BODY_JOINT_COUNT_FB);

            XrBodyTrackingCalibrationStatusMETA calibrationStatus{XR_TYPE_BODY_TRACKING_CALIBRATION_STATUS_META};
            locations.next = &calibrationStatus;

            XrBodyTrackingFidelityStatusMETA fidelityStatus{XR_TYPE_BODY_TRACKING_FIDELITY_STATUS_META};
            calibrationStatus.next = &fidelityStatus;

            locations.jointLocations = jointLocations_;

            XrBodyJointsLocateInfoFB locateInfo{XR_TYPE_BODY_JOINTS_LOCATE_INFO_FB};
            locateInfo.baseSpace = GetStageSpace();
            locateInfo.time = ToXrTime(in.PredictedDisplayTime);

            OXR(xrLocateBodyJointsFB_(bodyTracker_, &locateInfo, &locations));

            if (supportsCalibrationOverride_ && calibrationStatus.status != XR_BODY_TRACKING_CALIBRATION_STATE_VALID_META) {
                // Ignore body tracking, don't render it for example.
                ALOGV("Body tracking calibration is not yet valid");
            }

            // Application logic:
            if (supportsFidelity_ && fidelityStatus.fidelity != XR_BODY_TRACKING_FIDELITY_HIGH_META) {
                // Body tracking may be less accurate. Scoring may be adjusted for example.
                ALOGV("Body tracking fidelity is low");
            }

            XrBodySkeletonFB skeleton{XR_TYPE_BODY_SKELETON_FB};
            skeleton.next = nullptr;
            skeleton.jointCount = locations.jointCount;
            skeleton.joints = skeletonJoints_;

            OXR(xrGetSkeletonFB_(bodyTracker_, &skeleton));

            // Determine which joints are actually tracked
            XrSpaceLocationFlags isTracked =
                XR_SPACE_LOCATION_ORIENTATION_TRACKED_BIT | XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
            (void)isTracked;

            // Tracked joints and computed joints can all be valid
            XrSpaceLocationFlags isValid =
                XR_SPACE_LOCATION_ORIENTATION_VALID_BIT | XR_SPACE_LOCATION_POSITION_VALID_BIT;

            bodyTracked_ = locations.isActive;

            std::vector<OVR::Posef> bodyJoints;
            if (locations.isActive) {
                for (uint32_t i = 0; i < locations.jointCount; ++i) {
                    if (!displaySkeleton_) {
                        if ((jointLocations_[i].locationFlags & isValid)) {
                            bodyJoints.push_back(FromXrPosef(jointLocations_[i].pose));
                        }
                    } else {
                        // Please note that skeleton is intended only for retargeting,
                        // not for rendering! Here it's rendered only for giving you
                        // an opportunity see the data by eyes.
                        bodyJoints.push_back(FromXrPosef(skeleton.joints[i].pose));
                    }

                }
                // The skeletonChangedCount parameter is changed each time when
                // estimated body size/proportions are recalculated. The application
                // could watch that and tune an avatar when it's detected.
                if (locations.skeletonChangedCount != skeletonChangeCount_) {
                    skeletonChangeCount_ = locations.skeletonChangedCount;
                    ALOG("BodySkeleton: skeleton proportions have changed.");
                }
            }
            axisRenderer_.Update(bodyJoints);
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

        /// Render body axes
        if (bodyTracked_) {
            axisRenderer_.Render(OVR::Matrix4f(), in, out);
        }

        /// Render beams
        beamRenderer_.Render(in, out);
    }

   public:
    /// Bodys - extension functions
    PFN_xrCreateBodyTrackerFB xrCreateBodyTrackerFB_ = nullptr;
    PFN_xrDestroyBodyTrackerFB xrDestroyBodyTrackerFB_ = nullptr;
    PFN_xrLocateBodyJointsFB xrLocateBodyJointsFB_ = nullptr;
    PFN_xrGetBodySkeletonFB xrGetSkeletonFB_ = nullptr;
    PFN_xrRequestBodyTrackingFidelityMETA xrRequestBodyTrackingFidelityMETA_ = nullptr;
    PFN_xrSuggestBodyTrackingCalibrationOverrideMETA xrSuggestBodyTrackingCalibrationOverrideMETA_ = nullptr;
    /// Bodys - tracker bodyles
    XrBodyTrackerFB bodyTracker_ = XR_NULL_HANDLE;
    /// Bodys - data buffers

    XrBodyJointLocationFB jointLocations_[XR_FULL_BODY_JOINT_COUNT_META];
    XrBodySkeletonJointFB skeletonJoints_[XR_FULL_BODY_JOINT_COUNT_META];

   private:
    bool supportsFullBody_ = false;
    bool supportsFidelity_ = false;
    bool supportsCalibrationOverride_ = false;

    OVRFW::ControllerRenderer controllerRenderL_;
    OVRFW::ControllerRenderer controllerRenderR_;
    OVRFW::TinyUI ui_;
    OVRFW::SimpleBeamRenderer beamRenderer_;
    std::vector<OVRFW::ovrBeamRenderer::handle_t> beams_;
    OVRFW::ovrAxisRenderer axisRenderer_;
    bool bodyTracked_ = false;
    // Manual update to test getSkeleton (displays T-pose)
    // Default behaviour is to test getPose (dislays user movement)
    bool displaySkeleton_ = false;
    uint32_t skeletonChangeCount_ = 0;
};

ENTRY_POINT(XrBodyApp)
