// (c) Meta Platforms, Inc. and affiliates.

/*******************************************************************************

Sample app for FB_eye_tracking_social_fb

Please note that FB_eye_tracking_social_fb extension is intended
to drive animation of avatar eyes, for that purpose the runtimes
may filter the poses in ways that's suitable for
avatar eye interaction, but could be detrimental to other use cases.
That extension should not be used for other eye tracking purposes, for
interaction XR_EXT_eye_gaze_interaction should be used.

*******************************************************************************/

#include <cstdint>
#include <cstdio>

#include "XrApp.h"
#include <openxr/fb_eye_tracking_social.h>
#include <openxr/openxr.h>
#include <openxr/openxr_oculus_helpers.h>

#include "Input/TinyUI.h"

class XrEyesApp : public OVRFW::XrApp {
   public:
    XrEyesApp() : OVRFW::XrApp() {
        BackgroundColor = OVR::Vector4f(1.0f, 0.65f, 0.1f, 1.0f);
    }

    // Returns a list of OpenXr extensions needed for this app
    virtual std::vector<const char*> GetExtensions() override {
        std::vector<const char*> extensions = XrApp::GetExtensions();
        extensions.push_back(XR_FB_EYE_TRACKING_SOCIAL_EXTENSION_NAME);
        return extensions;
    }

    // Must return true if the application initializes successfully.
    virtual bool AppInit(const xrJava* context) override {
        if (false == ui_.Init(context, GetFileSys())) {
            ALOG("TinyUI::Init FAILED.");
            return false;
        }

        /// Build UI
        ui_.AddLabel("OpenXR Eyes Sample", {0.f, 1.5f, -2.0f}, {600.0f, 50.0f});
        leftRot_[0] = ui_.AddLabel("L ROT X", {-0.3f, 1.4f - 0.08f * 0, -2.f}, {300.f, 40.f});
        leftRot_[1] = ui_.AddLabel("L ROT Y", {-0.3f, 1.4f - 0.08f * 1, -2.f}, {300.f, 40.f});
        leftRot_[2] = ui_.AddLabel("L ROT Z", {-0.3f, 1.4f - 0.08f * 2, -2.f}, {300.f, 40.f});
        leftRot_[3] = ui_.AddLabel("L ROT W", {-0.3f, 1.4f - 0.08f * 3, -2.f}, {300.f, 40.f});
        leftDir_[0] = ui_.AddLabel("L DIR X", {-0.3f, 1.4f - 0.08f * 4, -2.f}, {300.f, 40.f});
        leftDir_[1] = ui_.AddLabel("L DIR Y", {-0.3f, 1.4f - 0.08f * 5, -2.f}, {300.f, 40.f});
        leftDir_[2] = ui_.AddLabel("L DIR Z", {-0.3f, 1.4f - 0.08f * 6, -2.f}, {300.f, 40.f});
        leftPos_[0] = ui_.AddLabel("L POS X", {-0.3f, 1.4f - 0.08f * 7, -2.f}, {300.f, 40.f});
        leftPos_[1] = ui_.AddLabel("L POS Y", {-0.3f, 1.4f - 0.08f * 8, -2.f}, {300.f, 40.f});
        leftPos_[2] = ui_.AddLabel("L POS Z", {-0.3f, 1.4f - 0.08f * 9, -2.f}, {300.f, 40.f});
        leftConf_ = ui_.AddLabel("L CONF", {-0.3f, 1.4f - 0.08f * 10, -2.f}, {300.f, 40.f});

        rightRot_[0] = ui_.AddLabel("R ROT X", {0.3f, 1.4f - 0.08f * 0, -2.f}, {300.f, 40.f});
        rightRot_[1] = ui_.AddLabel("R ROT Y", {0.3f, 1.4f - 0.08f * 1, -2.f}, {300.f, 40.f});
        rightRot_[2] = ui_.AddLabel("R ROT Z", {0.3f, 1.4f - 0.08f * 2, -2.f}, {300.f, 40.f});
        rightRot_[3] = ui_.AddLabel("R ROT W", {0.3f, 1.4f - 0.08f * 3, -2.f}, {300.f, 40.f});
        rightDir_[0] = ui_.AddLabel("R DIR X", {0.3f, 1.4f - 0.08f * 4, -2.f}, {300.f, 40.f});
        rightDir_[1] = ui_.AddLabel("R DIR Y", {0.3f, 1.4f - 0.08f * 5, -2.f}, {300.f, 40.f});
        rightDir_[2] = ui_.AddLabel("R DIR Z", {0.3f, 1.4f - 0.08f * 6, -2.f}, {300.f, 40.f});
        rightPos_[0] = ui_.AddLabel("R POS X", {0.3f, 1.4f - 0.08f * 7, -2.f}, {300.f, 40.f});
        rightPos_[1] = ui_.AddLabel("R POS Y", {0.3f, 1.4f - 0.08f * 8, -2.f}, {300.f, 40.f});
        rightPos_[2] = ui_.AddLabel("R POS Z", {0.3f, 1.4f - 0.08f * 9, -2.f}, {300.f, 40.f});
        rightConf_ = ui_.AddLabel("R CONF", {0.3f, 1.4f - 0.08f * 10, -2.f}, {300.f, 40.f});

        // Inspect eye tracking system properties
        XrSystemEyeTrackingPropertiesFB eyeTrackingSystemProperties{
            XR_TYPE_SYSTEM_EYE_TRACKING_PROPERTIES_FB};
        XrSystemProperties systemProperties{
            XR_TYPE_SYSTEM_PROPERTIES, &eyeTrackingSystemProperties};
        OXR(xrGetSystemProperties(GetInstance(), GetSystemId(), &systemProperties));
        if (!eyeTrackingSystemProperties.supportsEyeTracking) {
            // The system does not support eye tracking
            ALOG("xrGetSystemProperties XR_TYPE_SYSTEM_EYE_TRACKING_PROPERTIES_FB FAILED.");
            return false;
        } else {
            ALOG(
                "xrGetSystemProperties XR_TYPE_SYSTEM_EYE_TRACKING_PROPERTIES_FB OK - initiallizing eye tracking...");
        }

        /// Hook up extensions for eye tracking
        OXR(xrGetInstanceProcAddr(
            GetInstance(), "xrCreateEyeTrackerFB", (PFN_xrVoidFunction*)(&xrCreateEyeTrackerFB_)));
        OXR(xrGetInstanceProcAddr(
            GetInstance(),
            "xrDestroyEyeTrackerFB",
            (PFN_xrVoidFunction*)(&xrDestroyEyeTrackerFB_)));
        OXR(xrGetInstanceProcAddr(
            GetInstance(), "xrGetEyeGazesFB", (PFN_xrVoidFunction*)(&xrGetEyeGazesFB_)));

        return true;
    }

    virtual void AppShutdown(const xrJava* context) override {
        if (xrDestroyEyeTrackerFB_) {
            OXR(xrDestroyEyeTrackerFB_(eyeTracker_));
        }

        /// unhook extensions for eye tracking
        xrCreateEyeTrackerFB_ = nullptr;
        xrDestroyEyeTrackerFB_ = nullptr;
        xrGetEyeGazesFB_ = nullptr;

        OVRFW::XrApp::AppShutdown(context);
        ui_.Shutdown();
    }

    virtual bool SessionInit() override {
        /// Disable scene navitgation
        GetScene().SetFootPos({0.0f, 0.0f, 0.0f});
        this->FreeMove = false;

        if (xrCreateEyeTrackerFB_) {
            XrEyeTrackerCreateInfoFB createInfo{XR_TYPE_EYE_TRACKER_CREATE_INFO_FB};
            OXR(xrCreateEyeTrackerFB_(GetSession(), &createInfo, &eyeTracker_));
            ALOG("xrCreateEyeTrackerFB eyeTracker_=%llx", (long long)eyeTracker_);
        }

        return true;
    }

    virtual void SessionEnd() override {
        if (xrDestroyEyeTrackerFB_) {
            OXR(xrDestroyEyeTrackerFB_(eyeTracker_));
        }
    }

    // Update state
    virtual void Update(const OVRFW::ovrApplFrameIn& in) override {
        ui_.HitTestDevices().clear();

        /// Eyes
        if (eyeTracker_ != XR_NULL_HANDLE) {
            XrEyeGazesFB eyeGazes{XR_TYPE_EYE_GAZES_FB};
            eyeGazes.next = nullptr;

            XrEyeGazesInfoFB gazesInfo{XR_TYPE_EYE_GAZES_INFO_FB};
            gazesInfo.baseSpace = GetStageSpace();
            gazesInfo.time = ToXrTime(in.PredictedDisplayTime);

            OXR(xrGetEyeGazesFB_(eyeTracker_, &gazesInfo, &eyeGazes));

            for (int eye = 0; eye < 2; ++eye) {
                if (eyeGazes.gaze[eye].isValid) {
                    const OVR::Quatf rot = OVR::Quatf(
                        eyeGazes.gaze[eye].gazePose.orientation.x,
                        eyeGazes.gaze[eye].gazePose.orientation.y,
                        eyeGazes.gaze[eye].gazePose.orientation.z,
                        eyeGazes.gaze[eye].gazePose.orientation.w);
                    gazeRot_[eye] = rot;
                    gazeDirection_[eye] = rot.Rotate(OVR::Vector3f(0.0f, 0.0f, -1.0f));
                    gazeOrigin_[eye] = OVR::Vector3f(
                        eyeGazes.gaze[eye].gazePose.position.x,
                        eyeGazes.gaze[eye].gazePose.position.y,
                        eyeGazes.gaze[eye].gazePose.position.z);
                    gazeConfidence_[eye] = eyeGazes.gaze[eye].gazeConfidence;
                }
            }

            updateLabels();
        }

        ui_.Update(in);
    }

    // Render eye buffers while running
    virtual void Render(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out) override {
        /// Render UI
        ui_.Render(in, out);
    }

   public:
    /// Eyes - extension functions
    PFN_xrCreateEyeTrackerFB xrCreateEyeTrackerFB_ = nullptr;
    PFN_xrDestroyEyeTrackerFB xrDestroyEyeTrackerFB_ = nullptr;
    PFN_xrGetEyeGazesFB xrGetEyeGazesFB_ = nullptr;
    /// Eyes - tracker handles
    XrEyeTrackerFB eyeTracker_ = XR_NULL_HANDLE;

    /// Eyes - data buffers
    OVR::Quatf gazeRot_[2] = {};
    OVR::Vector3f gazeDirection_[2] = {};
    OVR::Vector3f gazeOrigin_[2] = {};
    float gazeConfidence_[2] = {};

   private:
    OVRFW::TinyUI ui_;

    OVRFW::VRMenuObject* leftRot_[4];
    OVRFW::VRMenuObject* leftDir_[3];
    OVRFW::VRMenuObject* leftPos_[3];
    OVRFW::VRMenuObject* leftConf_;

    OVRFW::VRMenuObject* rightRot_[4];
    OVRFW::VRMenuObject* rightDir_[3];
    OVRFW::VRMenuObject* rightPos_[3];
    OVRFW::VRMenuObject* rightConf_;

    void updateLabels() {
        char buf[100];

        snprintf(buf, sizeof(buf), "%s:%.2f", "L ROT X", gazeRot_[0].x);
        leftRot_[0]->SetText(buf);
        snprintf(buf, sizeof(buf), "%s:%.2f", "L ROT Y", gazeRot_[0].y);
        leftRot_[1]->SetText(buf);
        snprintf(buf, sizeof(buf), "%s:%.2f", "L ROT Z", gazeRot_[0].z);
        leftRot_[2]->SetText(buf);
        snprintf(buf, sizeof(buf), "%s:%.2f", "L ROT W", gazeRot_[0].w);
        leftRot_[3]->SetText(buf);
        snprintf(buf, sizeof(buf), "%s:%.2f", "L DIR X", gazeDirection_[0].x);
        leftDir_[0]->SetText(buf);
        snprintf(buf, sizeof(buf), "%s:%.2f", "L DIR Y", gazeDirection_[0].y);
        leftDir_[1]->SetText(buf);
        snprintf(buf, sizeof(buf), "%s:%.2f", "L DIR Z", gazeDirection_[0].z);
        leftDir_[2]->SetText(buf);
        snprintf(buf, sizeof(buf), "%s:%.2f", "L POS X", gazeOrigin_[0].x);
        leftPos_[0]->SetText(buf);
        snprintf(buf, sizeof(buf), "%s:%.2f", "L POS Y", gazeOrigin_[0].y);
        leftPos_[1]->SetText(buf);
        snprintf(buf, sizeof(buf), "%s:%.2f", "L POS Z", gazeOrigin_[0].z);
        leftPos_[2]->SetText(buf);
        snprintf(buf, sizeof(buf), "%s:%.2f", "L CONF", gazeConfidence_[0]);
        leftConf_->SetText(buf);

        snprintf(buf, sizeof(buf), "%s:%.2f", "R ROT X", gazeRot_[1].x);
        rightRot_[0]->SetText(buf);
        snprintf(buf, sizeof(buf), "%s:%.2f", "R ROT Y", gazeRot_[1].y);
        rightRot_[1]->SetText(buf);
        snprintf(buf, sizeof(buf), "%s:%.2f", "R ROT Z", gazeRot_[1].z);
        rightRot_[2]->SetText(buf);
        snprintf(buf, sizeof(buf), "%s:%.2f", "R ROT W", gazeRot_[1].w);
        rightRot_[3]->SetText(buf);
        snprintf(buf, sizeof(buf), "%s:%.2f", "R DIR X", gazeDirection_[1].x);
        rightDir_[0]->SetText(buf);
        snprintf(buf, sizeof(buf), "%s:%.2f", "R DIR Y", gazeDirection_[1].y);
        rightDir_[1]->SetText(buf);
        snprintf(buf, sizeof(buf), "%s:%.2f", "R DIR Z", gazeDirection_[1].z);
        rightDir_[2]->SetText(buf);
        snprintf(buf, sizeof(buf), "%s:%.2f", "R POS X", gazeOrigin_[1].x);
        rightPos_[0]->SetText(buf);
        snprintf(buf, sizeof(buf), "%s:%.2f", "R POS Y", gazeOrigin_[1].y);
        rightPos_[1]->SetText(buf);
        snprintf(buf, sizeof(buf), "%s:%.2f", "R POS Z", gazeOrigin_[1].z);
        rightPos_[2]->SetText(buf);
        snprintf(buf, sizeof(buf), "%s:%.2f", "R CONF", gazeConfidence_[1]);
        rightConf_->SetText(buf);
    }
};

ENTRY_POINT(XrEyesApp)
