/*******************************************************************************

Filename    :   Main.cpp
Content     :   Simple test app to test filter settings
Created     :
Authors     :   Federico Schliemann
Language    :   C++
Copyright:  Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*******************************************************************************/

#include <cstdint>
#include <cstdio>

#include "XrApp.h"

#include "Input/SkeletonRenderer.h"
#include "Input/ControllerRenderer.h"
#include "Input/TinyUI.h"
#include "Render/SimpleBeamRenderer.h"

class XrAppBaseApp : public OVRFW::XrApp {
   public:
    XrAppBaseApp() : OVRFW::XrApp() {
        BackgroundColor = OVR::Vector4f(1.0f, 0.65f, 0.1f, 1.0f);
    }

    // Return a list of OpenXR extensions needed for this app
    virtual std::vector<const char*> GetExtensions() override {
        std::vector<const char*> extensions = XrApp::GetExtensions();

        // Add required extensions here:
        // extensions.push_back(XR_META_EXTENSION_NAME);
        return extensions;
    }

    // Must return true if the application initializes successfully.
    virtual bool AppInit(const xrJava* context) override {
        if (false == ui_.Init(context, GetFileSys())) {
            ALOG("TinyUI::Init FAILED.");
            return false;
        }
        /// Build UI
        ui_.AddLabel("Text", {0.1f, 1.25f, -2.0f}, {1300.0f, 100.0f});
        ui_.AddButton("Button 1", {-1.0f, 2.0f, -2.0f}, {200.0f, 100.0f}, [=]() {
            BackgroundColor = OVR::Vector4f(0.0f, 0.65f, 0.1f, 1.0f);
        });
        ui_.AddButton("Button 2", {-1.0f, 2.25f, -2.0f}, {200.0f, 100.0f}, [=]() {
            BackgroundColor = OVR::Vector4f(0.0f, 0.25f, 1.0f, 1.0f);
        });
        ui_.AddSlider("Red  ", {1.0f, 2.25f, -2.0f}, &(BackgroundColor.x), 1.0f, 0.05f, 0.f, 1.f);
        ui_.AddSlider("Green", {1.0f, 2.00f, -2.0f}, &(BackgroundColor.y), 1.0f, 0.05f, 0.f, 1.f);
        ui_.AddSlider("Blue ", {1.0f, 1.75f, -2.0f}, &(BackgroundColor.z), 1.0f, 0.05f, 0.f, 1.f);

        return true;
    }

    virtual void AppShutdown(const xrJava* context) override {
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
        return true;
    }

    virtual void SessionEnd() override {
        controllerRenderL_.Shutdown();
        controllerRenderR_.Shutdown();
        beamRenderer_.Shutdown();
    }

    // Update state
    virtual void Update(const OVRFW::ovrApplFrameIn& in) override {
        ui_.HitTestDevices().clear();

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

        /// Render beams last, since they render with transparency (alpha blending)
        beamRenderer_.Render(in, out);
    }

   private:
    OVRFW::ControllerRenderer controllerRenderL_;
    OVRFW::ControllerRenderer controllerRenderR_;
    OVRFW::TinyUI ui_;
    OVRFW::SimpleBeamRenderer beamRenderer_;
    std::vector<OVRFW::ovrBeamRenderer::handle_t> beams_;
};

ENTRY_POINT(XrAppBaseApp)
