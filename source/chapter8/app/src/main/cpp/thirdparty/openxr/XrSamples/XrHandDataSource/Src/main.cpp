/*******************************************************************************

Filename    :   Main.cpp
Content     :   Simple test app to test openxr hand tracking usage extension
Language    :   C++
Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.

*******************************************************************************/

#include <openxr/openxr.h>
#include <cstdint>
#include <cstdio>
#include <iomanip>
#include <sstream>

#include "Model/ModelDef.h"
#include "Model/ModelFile.h"
#include "Model/ModelFileLoading.h"
#include "OVR_FileSys.h"
#include "XrApp.h"

#include "Input/HandRenderer.h"
#include "Input/TinyUI.h"
#include "ModelRenderer.h"
#include "EnvironmentRenderer.h"
#include "SkyboxRenderer.h"
#include "Render/GeometryRenderer.h"
#include "Render/SimpleBeamRenderer.h"

/// add logging
#define XRLOG ALOG

#include "xr_hand_helper.h"
#include "xr_render_model_helper.h"

class XrHandDataSourceApp : public OVRFW::XrApp {
   public:
    XrHandDataSourceApp() : OVRFW::XrApp() {
        BackgroundColor = OVR::Vector4f(0.3372549f, 0.345098f, 0.4f, 0.3686274f);
    }

    // Returns a list of OpenXr extensions needed for this app
    virtual std::vector<const char*> GetExtensions() override {
        std::vector<const char*> extensions = XrApp::GetExtensions();
        /// add hand extensions
        for (const auto& handExtension : XrHandHelper::RequiredExtensionNames()) {
            extensions.push_back(handExtension);
        }
        /// add render model extensions
        for (const auto& renderModelExtension : XrRenderModelHelper::RequiredExtensionNames()) {
            extensions.push_back(renderModelExtension);
        }

        /// add composition alpha blend
        extensions.push_back(XR_FB_COMPOSITION_LAYER_ALPHA_BLEND_EXTENSION_NAME);

        /// log all extensions
        ALOG("XrHandDataSourceApp requesting extensions:");
        for (const auto& e : extensions) {
            ALOG("   --> %s", e);
        }

        return extensions;
    }

    // Must return true if the application initializes successfully.
    virtual bool AppInit(const xrJava* context) override {
        if (false == ui_.Init(context, GetFileSys())) {
            ALOG("TinyUI::Init FAILED.");
            return false;
        }

        /// hand tracking
        handL_ = std::make_unique<XrHandHelper>(GetInstance(), true);
        OXR(handL_->GetLastError());
        handR_ = std::make_unique<XrHandHelper>(GetInstance(), false);
        OXR(handR_->GetLastError());

        /// render model
        renderModelLeft_ = std::make_unique<XrRenderModelHelper>(GetInstance());
        OXR(renderModelLeft_->GetLastError());
        renderModelRight_ = std::make_unique<XrRenderModelHelper>(GetInstance());
        OXR(renderModelRight_->GetLastError());

        /// Build UI
        bigText_ = ui_.AddLabel("Open XR Hand Data Source Sample", {0.1f, -0.25f, -2.0f}, {1300.0f, 100.0f});

        renderTrackedRemoteButton_ =
            ui_.AddButton("Render Tracked Remote", {-0.5f, 0.25f, -2.0f}, {500.0f, 100.0f}, [=]() {
                renderTrackedRemotes_ = !renderTrackedRemotes_;
                if (renderTrackedRemotes_) {
                    renderTrackedRemoteButton_->SetText("Stop Rendering Tracked Remotes");
                } else {
                    renderTrackedRemoteButton_->SetText("Render Tracked Remote");
                }
            });

        controllerHandDataTypeButton_ =
            ui_.AddButton("Set Hand Type Natural", {-0.5f, 0.5f, -2.0f}, {500.0f, 100.0f}, [=]() {
                handDataTypeNatural_ = !handDataTypeNatural_;
                if (handDataTypeNatural_) {
                    controllerHandDataTypeButton_->SetText("Set Hand Type Controller");
                } else {
                    controllerHandDataTypeButton_->SetText("Set Hand Type to Natural");
                }
            });

        auto fileSys = std::unique_ptr<OVRFW::ovrFileSys>(OVRFW::ovrFileSys::Create(*context));

        if( fileSys ) {
            std::string environmentPath = "apk:///assets/SmallRoom.gltf.ovrscene";
            environmentRenderer_.Init(environmentPath, fileSys.get());
            std::string skyboxPath = "apk:///assets/Skybox.gltf.ovrscene";
            skyboxRenderer.Init(skyboxPath, fileSys.get());
        }

        return true;
    }

    virtual void AppShutdown(const xrJava* context) override {
        handL_ = nullptr;
        handR_ = nullptr;
        renderModelLeft_ = nullptr;
        renderModelRight_ = nullptr;

        OVRFW::XrApp::AppShutdown(context);
        ui_.Shutdown();
    }

    virtual bool SessionInit() override {
        /// Use LocalSpace instead of Stage Space.
        CurrentSpace = LocalSpace;
        /// Disable scene navigation
        GetScene().SetFootPos({0.0f, 0.0f, 0.0f});
        this->FreeMove = false;
        beamRenderer_.Init(GetFileSys(), nullptr, OVR::Vector4f(1.0f), 1.0f);

        /// hands
        handL_->SessionInit(GetSession());
        handR_->SessionInit(GetSession());
        /// render model
        renderModelLeft_->SessionInit(GetSession());
        renderModelRight_->SessionInit(GetSession());

        /// rendering;
        handRendererL_.Init(&handL_->Mesh(), handL_->IsLeft());
        handRendererR_.Init(&handR_->Mesh(), handR_->IsLeft());

        return true;
    }

    virtual void SessionEnd() override {
        /// hands
        handL_->SessionEnd();
        handR_->SessionEnd();
        /// render model
        renderModelLeft_->SessionEnd();
        renderModelRight_->SessionEnd();

        controllerRenderL_.Shutdown();
        controllerRenderR_.Shutdown();
        skyboxRenderer.Shutdown();
        environmentRenderer_.Shutdown();
        beamRenderer_.Shutdown();
        handRendererL_.Shutdown();
        handRendererR_.Shutdown();
    }

    // Update state
    virtual void Update(const OVRFW::ovrApplFrameIn& in) override {
        XrSpace currentSpace = GetCurrentSpace();
        XrTime predictedDisplayTime = ToXrTime(in.PredictedDisplayTime);

        /// render model
        renderModelLeft_->Update(currentSpace, predictedDisplayTime);
        renderModelRight_->Update(currentSpace, predictedDisplayTime);
        /// hands
        handL_->SetHandDataTypeNatural(handDataTypeNatural_);
        handL_->Update(currentSpace, predictedDisplayTime);
        handR_->SetHandDataTypeNatural(handDataTypeNatural_);
        handR_->Update(currentSpace, predictedDisplayTime);

        bool renderLeftController = renderTrackedRemotes_;
        if (handL_->AreLocationsActive()) {
            handRendererL_.Update(handL_->Joints(), handL_->RenderScale());
            if(!handL_->OnController()){
                renderLeftController = false;
            }
        }
        if(renderLeftController){
            if(controllerRenderL_.IsInitialized()){
                //OVR::Posef translatedPose = in.LeftRemotePointPose;
                //translatedPose.Translation = translatedPose.Apply(OVR::Vector3f(0.f,0.f,0.055f));
                controllerRenderL_.Update(
                    in.LeftRemotePose, OVRFW::ModelRenderer::UpdateOffset_Grip);
            } else {
                std::vector<uint8_t> controllerBufferl;
                std::string strToCheck = "/model_fb/controller/left";
                controllerBufferl = renderModelLeft_->LoadRenderModel(strToCheck);
                if (controllerBufferl.size() > 0) {
                    ALOG("### Left Controller Render Model Size: %u", (uint32_t)controllerBufferl.size());
                    controllerRenderL_.Init(controllerBufferl);
                    controllerRenderL_.UseSolidTexture = true;
                    controllerRenderL_.Opacity = 1.0f;
                } else {
                    ALOG("### Failed to Load Left Controller Render Model");
                }
            }
        }

        bool renderRightController = renderTrackedRemotes_;
        if (handR_->AreLocationsActive()) {
            handRendererR_.Update(handR_->Joints(), handR_->RenderScale());
            if(!handR_->OnController()){
                renderRightController = false;
            }
        }
        if(renderRightController) {
            if(controllerRenderR_.IsInitialized()){
                //OVR::Posef translatedPose = in.RightRemotePointPose;
                //in.RightRemotePointPose;
                //translatedPose.Translation = translatedPose.Apply(OVR::Vector3f(0.f,0.f,0.055f));
                controllerRenderR_.Update(
                    in.RightRemotePose, OVRFW::ModelRenderer::UpdateOffset_Grip);
            } else {
                std::vector<uint8_t> controllerBufferr;
                std::string strToCheck = "/model_fb/controller/right";
                controllerBufferr = renderModelRight_->LoadRenderModel(strToCheck);
                if (controllerBufferr.size() > 0) {
                    controllerRenderR_.Init(controllerBufferr);
                    controllerRenderR_.UseSolidTexture = true;
                    controllerRenderR_.Opacity = 1.0f;
                }
            }
        }

        /// UI
        ui_.HitTestDevices().clear();
        if (handDataTypeNatural_ && handR_->AreLocationsActive()) {
            ui_.AddHitTestRay(FromXrPosef(handR_->AimPose()), handR_->IndexPinching());
        } else if (in.RightRemoteTracked) {
            const bool didPinch = in.RightRemoteIndexTrigger > 0.25f;
            ui_.AddHitTestRay(in.RightRemotePointPose, didPinch);
        }

        if (handDataTypeNatural_ && handL_->AreLocationsActive()) {
            ui_.AddHitTestRay(FromXrPosef(handL_->AimPose()), handL_->IndexPinching());
        } else if (in.LeftRemoteTracked) {
            const bool didPinch = in.LeftRemoteIndexTrigger > 0.25f;
            ui_.AddHitTestRay(in.LeftRemotePointPose, didPinch);
        }

        ui_.Update(in);
        beamRenderer_.Update(in, ui_.HitTestDevices());
    }

    // Render eye buffers while running
    virtual void Render(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out) override {
        // Render the environment first, to place behind all other surfaces.
        skyboxRenderer.Render(out.Surfaces);
        environmentRenderer_.Render(out.Surfaces);

        /// Render UI
        ui_.Render(in, out);

        /// Render beams
        beamRenderer_.Render(in, out);

        if (handL_->AreLocationsActive() && handL_->IsPositionValid()) {
            /// Render solid Hands
            handRendererL_.Solidity = 1.0f;
            handRendererL_.Render(out.Surfaces);
        }

        if (in.LeftRemoteTracked && renderTrackedRemotes_) {
            // Only render controller when hands are not rendered
            // Note: Hand tracking can drive controller positions as well.
            controllerRenderL_.Render(out.Surfaces);
        }

        if (handR_->AreLocationsActive() && handR_->IsPositionValid()) {
            /// Render solid Hands
            handRendererR_.Solidity = 1.0f;
            handRendererR_.Render(out.Surfaces);
        }

        if (in.RightRemoteTracked && renderTrackedRemotes_) {
            // Only render controller when hands are not rendered
            // Note: Hand tracking can drive controller positions as well.
            controllerRenderR_.Render(out.Surfaces);
        }
    }

   public:
   private:
    OVRFW::ModelRenderer controllerRenderL_;
    OVRFW::ModelRenderer controllerRenderR_;
    OVRFW::EnvironmentRenderer environmentRenderer_;
    OVRFW::SkyboxRenderer skyboxRenderer;

    OVRFW::TinyUI ui_;
    OVRFW::SimpleBeamRenderer beamRenderer_;
    std::vector<OVRFW::ovrBeamRenderer::handle_t> beams_;

    /// hands - xr interface
    std::unique_ptr<XrHandHelper> handL_;
    std::unique_ptr<XrHandHelper> handR_;
    /// hands - rendering
    OVRFW::HandRenderer handRendererL_;
    OVRFW::HandRenderer handRendererR_;

    /// render model - xr interface
    std::unique_ptr<XrRenderModelHelper> renderModelLeft_;
    std::unique_ptr<XrRenderModelHelper> renderModelRight_;

    // gui and state
    bool renderTrackedRemotes_ = false;
    bool handDataTypeNatural_ = false;
    OVRFW::VRMenuObject* renderTrackedRemoteButton_ = nullptr;
    OVRFW::VRMenuObject* controllerHandDataTypeButton_ = nullptr;

    // info text;
    OVRFW::VRMenuObject* bigText_ = nullptr;
};

ENTRY_POINT(XrHandDataSourceApp)
