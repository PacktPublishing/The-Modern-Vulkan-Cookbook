/************************************************************************************************
Filename    :   main.cpp
Content     :   Sample test app to test openxr XR_META_virtual_keyboard extension
Created     :   March 2022
Authors     :   Chiara Coetzee, Juan Pablo León, Robert Memmott, Brent Housen, Peter Chan
Language    :   C++
Copyright   :   Copyright (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.
************************************************************************************************/

#include <OVR_Math.h>
#include <XrApp.h>

#include <Input/ControllerRenderer.h>
#include <Input/HandRenderer.h>
#include <Input/TinyUI.h>
#include <Render/SimpleBeamRenderer.h>

#include "VirtualKeyboardModelRenderer.h"
#include "XrHandHelper.h"
#include "XrRenderModelHelper.h"
#include "XrVirtualKeyboardHelper.h"

enum struct InputHandedness { Unknown, Left, Right };

class XrVirtualKeyboardApp : public OVRFW::XrApp {
   public:
    XrVirtualKeyboardApp() = default;

    // Returns a list of OpenXr extensions needed for this app
    virtual std::vector<const char*> GetExtensions() override {
        std::vector<const char*> extensions = XrApp::GetExtensions();
        // Add virtual keyboard extensions
        for (const auto& kbdExtension : XrVirtualKeyboardHelper::RequiredExtensionNames()) {
            extensions.push_back(kbdExtension);
        }
        // Add hand extensions
        for (const auto& handExtension : XrHandHelper::RequiredExtensionNames()) {
            extensions.push_back(handExtension);
        }
        // Add render model extensions
        for (const auto& renderModelExtension : XrRenderModelHelper::RequiredExtensionNames()) {
            extensions.push_back(renderModelExtension);
        }

        // Log all extensions
        ALOG("XrVirtualKeyboardApp requesting extensions:");
        for (const auto& e : extensions) {
            ALOG("   --> %s", e);
        }

        return extensions;
    }

    // Must return true if the application initializes successfully
    virtual bool AppInit(const xrJava* context) override {
        if (false == ui_.Init(context, GetFileSys(), false)) {
            ALOG("TinyUI::Init FAILED.");
            return false;
        }

        // Virtual keyboard
        keyboardExtensionAvailable_ =
            ExtensionsArePresent(XrVirtualKeyboardHelper::RequiredExtensionNames());
        if (keyboardExtensionAvailable_) {
            virtualKeyboard_ = std::make_unique<XrVirtualKeyboardHelper>(GetInstance());
            OXR(virtualKeyboard_->GetLastError());
        }

        // Hand tracking
        handsExtensionAvailable_ = ExtensionsArePresent(XrHandHelper::RequiredExtensionNames());
        if (handsExtensionAvailable_) {
            handL_ = std::make_unique<XrHandHelper>(GetInstance(), true);
            OXR(handL_->GetLastError());
            handR_ = std::make_unique<XrHandHelper>(GetInstance(), false);
            OXR(handR_->GetLastError());
        }

        // Render model
        renderModelExtensionAvailable_ =
            ExtensionsArePresent(XrRenderModelHelper::RequiredExtensionNames());
        if (renderModelExtensionAvailable_) {
            renderModel_ = std::make_unique<XrRenderModelHelper>(GetInstance());
            OXR(renderModel_->GetLastError());
        }

        return true;
    }

    virtual void AppShutdown(const xrJava* context) override {
        renderModel_ = nullptr;
        handL_ = nullptr;
        handR_ = nullptr;
        virtualKeyboard_ = nullptr;

        uiInitialized_ = false;
        renderModelExtensionAvailable_ = false;
        handsExtensionAvailable_ = false;
        keyboardExtensionAvailable_ = false;

        ui_.Shutdown();
        OVRFW::XrApp::AppShutdown(context);
    }

    virtual bool SessionInit() override {
        // Use LocalSpace instead of Stage Space
        CurrentSpace = LocalSpace;

        // Disable scene navigation
        GetScene().SetFootPos({0.0f, 0.0f, 0.0f});
        FreeMove = false;

        // Init session bound objects
        if (false == controllerRenderL_.Init(true)) {
            ALOG("SessionInit::Init L controller renderer FAILED.");
            return false;
        }
        if (false == controllerRenderR_.Init(false)) {
            ALOG("SessionInit::Init R controller renderer FAILED.");
            return false;
        }
        beamRenderer_.Init(GetFileSys(), nullptr, OVR::Vector4f(1.0f), 1.0f);
        particleSystem_.Init(10, nullptr, OVRFW::ovrParticleSystem::GetDefaultGpuState(), false);

        if (keyboardExtensionAvailable_) {
            virtualKeyboard_->SessionInit(GetSession());

            XrVirtualKeyboardCreateInfoMETA createInfo{XR_TYPE_VIRTUAL_KEYBOARD_CREATE_INFO_META};
            bool success = virtualKeyboard_->CreateVirtualKeyboard(&createInfo);
            if (!success) {
                OXR(virtualKeyboard_->GetLastError());
            }

            // Create a default space to locate the keyboard
            if (success) {
                XrVirtualKeyboardSpaceCreateInfoMETA spaceCreateInfo{
                    XR_TYPE_VIRTUAL_KEYBOARD_SPACE_CREATE_INFO_META};
                spaceCreateInfo.space = GetLocalSpace();
                spaceCreateInfo.poseInSpace = ToXrPosef(OVR::Posef::Identity());
                success = virtualKeyboard_->CreateVirtualKeyboardSpace(&spaceCreateInfo);
                if (!success) {
                    OXR(virtualKeyboard_->GetLastError());
                }
            }
        }

        if (handsExtensionAvailable_) {
            handL_->SessionInit(GetSession());
            handR_->SessionInit(GetSession());
            handRendererL_.Init(&handL_->Mesh(), handL_->IsLeft());
            handRendererR_.Init(&handR_->Mesh(), handR_->IsLeft());
        }

        if (renderModelExtensionAvailable_) {
            renderModel_->SessionInit(GetSession());
        }

        return true;
    }

    virtual void SessionEnd() override {
        if (renderModelExtensionAvailable_) {
            renderModel_->SessionEnd();
        }
        if (handsExtensionAvailable_) {
            handL_->SessionEnd();
            handR_->SessionEnd();
            handRendererL_.Shutdown();
            handRendererR_.Shutdown();
        }
        if (keyboardExtensionAvailable_) {
            virtualKeyboard_->DestroyVirtualKeyboard();
            virtualKeyboard_->SessionEnd();
        }

        keyboardModelRenderer_.Shutdown();
        controllerRenderL_.Shutdown();
        controllerRenderR_.Shutdown();
        particleSystem_.Shutdown();
        beamRenderer_.Shutdown();
    }

    virtual void HandleXrEvents() override {
        XrEventDataBuffer eventDataBuffer = {};

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
                case XR_TYPE_EVENT_DATA_VIRTUAL_KEYBOARD_COMMIT_TEXT_META: {
                    const XrEventDataVirtualKeyboardCommitTextMETA* commit_text_event =
                        (XrEventDataVirtualKeyboardCommitTextMETA*)(baseEventHeader);
                    OnCommitText(commit_text_event->text);
                } break;
                case XR_TYPE_EVENT_DATA_VIRTUAL_KEYBOARD_BACKSPACE_META: {
                    OnBackspace();
                } break;
                case XR_TYPE_EVENT_DATA_VIRTUAL_KEYBOARD_ENTER_META: {
                    OnEnter();
                } break;
                case XR_TYPE_EVENT_DATA_VIRTUAL_KEYBOARD_SHOWN_META: {
                    OnKeyboardShown();
                } break;
                case XR_TYPE_EVENT_DATA_VIRTUAL_KEYBOARD_HIDDEN_META: {
                    OnKeyboardHidden();
                } break;
                case XR_TYPE_EVENT_DATA_EVENTS_LOST:
                    ALOGV("xrPollEvent: received XR_TYPE_EVENT_DATA_EVENTS_LOST event");
                    break;
                case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                    ALOGV("xrPollEvent: received XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING event");
                    break;
                case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
                    ALOGV(
                        "xrPollEvent: received XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED event");
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
                        case XR_SESSION_STATE_FOCUSED: {
                            Focused = true;

                            // Try to load the keyboard model if it's not already loaded
                            if (!keyboardModelRenderer_.IsModelLoaded() &&
                                renderModelExtensionAvailable_) {
                                if (modelKey_ == XR_NULL_RENDER_MODEL_KEY_FB) {
                                    // This path should be documented in the OpenXR spec here:
                                    // https://registry.khronos.org/OpenXR/specs/1.0/man/html/openxr.html#XrRenderModelPathInfoFB
                                    // under "Possible Render Model Paths".
                                    modelKey_ = renderModel_->TryGetRenderModelKey(
                                        "/model_meta/keyboard/virtual");
                                    if (modelKey_ == XR_NULL_RENDER_MODEL_KEY_FB) {
                                        ALOGE("Failed to get virtual keyboard render model key");
                                    }
                                }
                                if (modelKey_ != XR_NULL_RENDER_MODEL_KEY_FB) {
                                    std::vector<uint8_t> buffer =
                                        renderModel_->LoadRenderModel(modelKey_);
                                    ALOG("Model buffer.size() = %zu", buffer.size());
                                    if (keyboardModelRenderer_.Init(buffer)) {
                                        keyboardModelRenderer_.Update(
                                            currentPose_, OVR::Vector3f(currentScale_));
                                        ShowKeyboard();
                                    } else {
                                        ALOGE("Failed to load virtual keyboard render model");
                                    }
                                }
                            }
                        } break;
                        case XR_SESSION_STATE_VISIBLE:
                            Focused = false;
                            break;
                        case XR_SESSION_STATE_READY:
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

    virtual void Update(const OVRFW::ovrApplFrameIn& in) override {
        InitializeUI();

        XrSpace currentSpace = GetCurrentSpace();
        XrTime predictedDisplayTime = ToXrTime(in.PredictedDisplayTime);

        // hands
        if (handsExtensionAvailable_) {
            handL_->Update(currentSpace, predictedDisplayTime);
            handR_->Update(currentSpace, predictedDisplayTime);
        }

        // Keyboard
        if (keyboardExtensionAvailable_) {
            virtualKeyboard_->Update(currentSpace, predictedDisplayTime);
        }

        // Render model
        if (renderModelExtensionAvailable_) {
            renderModel_->Update(currentSpace, predictedDisplayTime);
        }

        if (in.Clicked(in.kButtonA)) {
            if (!isShowingKeyboard_) {
                ShowKeyboard();
            } else {
                HideKeyboard();
            }
        }

        UpdateUIHitTests(in);

        // Update controller poses (which may be adjusted after keyboard interaction)
        leftAdjustedRemotePose_ = in.LeftRemotePose;
        rightAdjustedRemotePose_ = in.RightRemotePose;

        if (isShowingKeyboard_) {
            UpdateKeyboardInteractions(in);
            UpdateKeyboardMoving(in);
        }

        // Hands
        if (handsExtensionAvailable_) {
            if (handL_->AreLocationsActive()) {
                handRendererL_.Update(handL_->Joints(), handL_->RenderScale());
            }
            if (handR_->AreLocationsActive()) {
                handRendererR_.Update(handR_->Joints(), handR_->RenderScale());
            }
        }

        // Controllers
        if (in.LeftRemoteTracked) {
            controllerRenderL_.Update(leftAdjustedRemotePose_);
        }
        if (in.RightRemoteTracked) {
            controllerRenderR_.Update(rightAdjustedRemotePose_);
        }

        if (keyboardModelRenderer_.IsModelLoaded()) {
            std::vector<uint64_t> textureIds;
            virtualKeyboard_->GetDirtyTextures(textureIds);
            for (const uint64_t textureId : textureIds) {
                XrVirtualKeyboardTextureDataMETA textureData{};
                if (virtualKeyboard_->GetTextureData(textureId, textureData)) {
                    keyboardModelRenderer_.UpdateTexture(
                        textureId,
                        textureData.buffer,
                        textureData.textureWidth,
                        textureData.textureHeight);
                }
            }

            XrVirtualKeyboardModelAnimationStatesMETA modelAnimationStates;
            virtualKeyboard_->GetModelAnimationStates(modelAnimationStates);
            for (uint32_t i = 0; i < modelAnimationStates.stateCountOutput; ++i) {
                const auto& animationState = modelAnimationStates.states[i];
                keyboardModelRenderer_.SetAnimationState(
                    animationState.animationIndex, animationState.fraction);
            }
            keyboardModelRenderer_.UpdateSurfaceGeo();
        }
    }

    virtual void Render(const OVRFW::ovrApplFrameIn& in, OVRFW::ovrRendererOutput& out) override {
        ui_.Render(in, out);

        if (isShowingKeyboard_) {
            keyboardModelRenderer_.Render(out.Surfaces);
        }

        if (handsExtensionAvailable_ && handL_->AreLocationsActive() && handL_->IsPositionValid()) {
            handRendererL_.Render(out.Surfaces);
        } else if (in.LeftRemoteTracked) {
            controllerRenderL_.Render(out.Surfaces);
        }

        if (handsExtensionAvailable_ && handR_->AreLocationsActive() && handR_->IsPositionValid()) {
            handRendererR_.Render(out.Surfaces);
        } else if (in.RightRemoteTracked) {
            controllerRenderR_.Render(out.Surfaces);
        }

        // Render beams last for proper blending
        particleSystem_.Frame(in, nullptr, out.FrameMatrices.CenterView);
        particleSystem_.RenderEyeView(
            out.FrameMatrices.CenterView, out.FrameMatrices.EyeProjection[0], out.Surfaces);
        beamRenderer_.Render(in, out);
    }

   private:
    enum class HitTestRayDeviceNums {
        LeftHand,
        LeftRemote,
        RightHand,
        RightRemote,
    };

    void ShowKeyboard() {
        if (!virtualKeyboard_->HasVirtualKeyboard()) {
            return;
        }

        if (!virtualKeyboard_->ShowModel(true)) {
            eventLog_->SetText("Failed to show keyboard");
            return;
        }

        SetKeyboardLocation(locationType_);

        // Should call this whenever the keyboard is created or when the text focus changes
        virtualKeyboard_->UpdateTextContext(textInputBuffer_);
    }

    void SetKeyboardLocation(XrVirtualKeyboardLocationTypeMETA locationType) {
        // Update keyboard location based on the location type
        XrVirtualKeyboardLocationInfoMETA locationInfo{XR_TYPE_VIRTUAL_KEYBOARD_LOCATION_INFO_META};
        locationInfo.space = GetLocalSpace();
        locationInfo.locationType = locationType;
        if (!virtualKeyboard_->SuggestVirtualKeyboardLocation(&locationInfo)) {
            eventLog_->SetText("Failed to update keyboard location & scale.");
            return;
        }

        // Remember the current location type
        locationType_ = locationType;

        // Sync local location/scale
        VirtualKeyboardLocation location;
        if (!virtualKeyboard_->GetVirtualKeyboardLocation(
                GetLocalSpace(), ToXrTime(OVRFW::GetTimeInSeconds()), &location)) {
            eventLog_->SetText("Failed to sync keyboard location & scale.");
            return;
        }
        currentPose_ = FromXrPosef(location.pose);
        currentScale_ = location.scale;
    }

    void HideKeyboard() {
        virtualKeyboard_->ShowModel(false);
    }

    bool ExtensionsArePresent(const std::vector<const char*>& extensionList) const {
        const auto extensionProperties = GetXrExtensionProperties();
        bool foundAllExtensions = true;
        for (const auto& extension : extensionList) {
            bool foundExtension = false;
            for (const auto& extensionProperty : extensionProperties) {
                if (!strcmp(extension, extensionProperty.extensionName)) {
                    foundExtension = true;
                    break;
                }
            }
            if (!foundExtension) {
                foundAllExtensions = false;
                break;
            }
        }
        return foundAllExtensions;
    }

    void InitializeUI() {
        if (uiInitialized_) {
            return;
        }
        uiInitialized_ = true;

        keyboardHitTest_ = ui_.AddLabel("", {0.0f, 0.0f, 0.0f}, {100.0f, 100.0f});
        keyboardHitTest_->SetColor({0, 0, 0, 0});
        keyboardHitTest_->AddFlags(OVRFW::VRMENUOBJECT_FLAG_NO_DEPTH_MASK);

        eventLog_ = ui_.AddLabel("", {0.0f, 0.5f, -1.5f}, {600.0f, 50.0f});

        // virtual keyboard
        if (!keyboardExtensionAvailable_) {
            eventLog_->SetText("Virtual Keyboard extension unavailable.");
            return;
        }
        if (!virtualKeyboard_->IsSupported()) {
            eventLog_->SetText("Virtual Keyboard not supported.");
            return;
        }
        if (!virtualKeyboard_->HasVirtualKeyboard()) {
            eventLog_->SetText("Virtual Keyboard creation failed");
            return;
        }

        // Build UI
        textInput_ = ui_.AddLabel("", {0.0f, 0.1f, -1.5f}, {600.0f, 320.0f});
        OVRFW::VRMenuFontParms fontParms = textInput_->GetFontParms();
        fontParms.AlignHoriz = OVRFW::HORIZONTAL_LEFT;
        fontParms.AlignVert = OVRFW::VERTICAL_BASELINE;
        fontParms.WrapWidth = 1.1f;
        fontParms.MaxLines = 10;
        textInput_->SetFontParms(fontParms);
        textInput_->SetTextLocalPosition({-0.55f, 0.25f, 0.0f});

        // Keyboard visibility controls
        showKeyboardButton_ = ui_.AddButton(
            "Show Keyboard", {-0.3f, 0.9f, -1.5f}, {300.0f, 50.0f}, [this]() { ShowKeyboard(); });
        hideKeyboardButton_ = ui_.AddButton(
            "Hide Keyboard", {-0.3f, 0.8f, -1.5f}, {300.0f, 50.0f}, [this]() { HideKeyboard(); });

        // Keyboard location controls
        enableMoveKeyboardButton_ =
            ui_.AddButton("Move", {0.1f, 0.9f, -1.5f}, {100.0f, 50.0f}, [this]() {
                if (isShowingKeyboard_) {
                    isMovingKeyboard_ = true;
                    keyboardMoveDistance_ = 0.0f;
                }
            });
        showNearKeyboardButton_ =
            ui_.AddButton("Near", {0.3f, 0.9f, -1.5f}, {100.0f, 50.0f}, [this]() {
                SetKeyboardLocation(XR_VIRTUAL_KEYBOARD_LOCATION_TYPE_DIRECT_META);
            });
        showFarKeyboardButton_ =
            ui_.AddButton("Far", {0.5f, 0.9f, -1.5f}, {100.0f, 50.0f}, [this]() {
                SetKeyboardLocation(XR_VIRTUAL_KEYBOARD_LOCATION_TYPE_FAR_META);
            });

        // Clear text
        clearTextButton_ =
            ui_.AddButton("Clear Text", {0.3f, 0.8f, -1.5f}, {300.0f, 50.0f}, [this]() {
                textInputBuffer_.clear();
                textInput_->SetText(textInputBuffer_.c_str());
                eventLog_->SetText("Text Cleared");
                virtualKeyboard_->UpdateTextContext(textInputBuffer_);
            });

        ui_.SetUnhandledClickHandler([this]() { isMovingKeyboard_ = false; });
    }

    void EnableButton(OVRFW::VRMenuObject* button) {
        button->SetSurfaceColor(0, ui_.BackgroundColor);
        button->RemoveFlags(OVRFW::VRMENUOBJECT_DONT_HIT_ALL);
    }

    void DisableButton(OVRFW::VRMenuObject* button) {
        button->SetSurfaceColor(0, {0.1f, 0.1f, 0.1f, 1.0f});
        button->AddFlags(OVRFW::VRMENUOBJECT_DONT_HIT_ALL);
    }

    void DetermineHandedness(const OVRFW::ovrApplFrameIn& in) {
        if ((handsExtensionAvailable_ && handL_->AreLocationsActive()) || in.LeftRemoteTracked) {
            if (currentHandedness_ == InputHandedness::Unknown ||
                (handsExtensionAvailable_ && handL_->IndexPinching()) ||
                in.LeftRemoteIndexTrigger > 0.25f) {
                currentHandedness_ = InputHandedness::Left;
            }
        } else if (currentHandedness_ == InputHandedness::Left) {
            currentHandedness_ = InputHandedness::Unknown;
        }
        if ((handsExtensionAvailable_ && handR_->AreLocationsActive()) || in.RightRemoteTracked) {
            if (currentHandedness_ == InputHandedness::Unknown ||
                (handsExtensionAvailable_ && handR_->IndexPinching()) ||
                in.RightRemoteIndexTrigger > 0.25f) {
                currentHandedness_ = InputHandedness::Right;
            }
        } else if (currentHandedness_ == InputHandedness::Right) {
            currentHandedness_ = InputHandedness::Unknown;
        }
    }

    void UpdateKeyboardPosition(
        const OVR::Posef activePointerPose,
        OVR::Vector2f distanceScaleMod,
        bool shouldFlip) {
        if (keyboardMoveDistance_ == 0.0f) {
            keyboardMoveDistance_ =
                currentPose_.Translation.Distance(activePointerPose.Translation);
        }

        float distanceScaleModDeadzone = 0.2f;

        if (std::abs(distanceScaleMod.y) > distanceScaleModDeadzone) {
            auto distanceMod = (distanceScaleMod.y > 0)
                ? distanceScaleMod.y - distanceScaleModDeadzone
                : distanceScaleMod.y + distanceScaleModDeadzone;
            keyboardMoveDistance_ *= 1.0f + distanceMod * 0.01f;
            keyboardMoveDistance_ = OVR::OVRMath_Clamp(keyboardMoveDistance_, 0.1f, 100.0f);
        }

        auto pointFromPointerPose = activePointerPose.Translation +
            activePointerPose.Rotation.Normalized() * OVR::Vector3f(0, 0, -1) *
                keyboardMoveDistance_;

        // Account for left hand input activePointerPose being flipped
        auto targetRotation = (shouldFlip)
            ? activePointerPose.Rotation * OVR::Quatf({0, 0, 1}, MATH_FLOAT_PI)
            : activePointerPose.Rotation;
        auto targetPose = OVR::Posef(targetRotation, pointFromPointerPose);

        float newScale = currentScale_;

        if (std::abs(distanceScaleMod.x) > distanceScaleModDeadzone) {
            auto scaleMod = (distanceScaleMod.x > 0)
                ? distanceScaleMod.x - distanceScaleModDeadzone
                : distanceScaleMod.x + distanceScaleModDeadzone;
            newScale = currentScale_ *= 1.0f + scaleMod * 0.01f;
            newScale = OVR::OVRMath_Clamp(newScale, 0.4f, 2.0f);
        }

        XrVirtualKeyboardLocationInfoMETA locationInfo{XR_TYPE_VIRTUAL_KEYBOARD_LOCATION_INFO_META};
        locationInfo.locationType = XR_VIRTUAL_KEYBOARD_LOCATION_TYPE_CUSTOM_META;
        locationInfo.space = GetLocalSpace();
        locationInfo.poseInSpace = ToXrPosef(targetPose);
        locationInfo.scale = newScale;

        virtualKeyboard_->SuggestVirtualKeyboardLocation(&locationInfo);
    }

    OVRFW::ovrParticleSystem::handle_t AddParticle(
        const OVRFW::ovrApplFrameIn& in,
        const OVR::Vector3f& position) {
        return particleSystem_.AddParticle(
            in,
            position,
            0.0f,
            OVR::Vector3f(0.0f),
            OVR::Vector3f(0.0f),
            beamRenderer_.PointerParticleColor,
            OVRFW::ovrEaseFunc::NONE,
            0.0f,
            0.03f,
            0.1f,
            0);
    }

    void UpdateUIHitTests(const OVRFW::ovrApplFrameIn& in) {
        ui_.HitTestDevices().clear();
        particleSystem_.RemoveParticle(leftControllerPoint_);
        particleSystem_.RemoveParticle(rightControllerPoint_);

        // The controller actions are still triggered with hand tracking
        if (handsExtensionAvailable_ && handL_->IsPositionValid()) {
            UpdateRemoteTrackedUIHitTest(
                FromXrPosef(handL_->AimPose()),
                handL_->IndexPinching() ? 1.0f : 0.0f,
                handL_.get(),
                HitTestRayDeviceNums::LeftHand);
        } else if (in.LeftRemoteTracked) {
            UpdateRemoteTrackedUIHitTest(
                in.LeftRemotePointPose,
                in.LeftRemoteIndexTrigger,
                handL_.get(),
                HitTestRayDeviceNums::LeftRemote);
            leftControllerPoint_ = AddParticle(in, in.LeftRemotePointPose.Translation);
        }

        if (handsExtensionAvailable_ && handR_->IsPositionValid()) {
            UpdateRemoteTrackedUIHitTest(
                FromXrPosef(handR_->AimPose()),
                handR_->IndexPinching() ? 1.0f : 0.0f,
                handR_.get(),
                HitTestRayDeviceNums::RightHand);
        } else if (in.RightRemoteTracked) {
            UpdateRemoteTrackedUIHitTest(
                in.RightRemotePointPose,
                in.RightRemoteIndexTrigger,
                handR_.get(),
                HitTestRayDeviceNums::RightRemote);
            rightControllerPoint_ = AddParticle(in, in.RightRemotePointPose.Translation);
        }

        ui_.Update(in);
        beamRenderer_.Update(in, ui_.HitTestDevices());
    }

    void UpdateRemoteTrackedUIHitTest(
        const OVR::Posef& remotePose,
        float remoteIndexTrigger,
        XrHandHelper* hand,
        HitTestRayDeviceNums device) {
        if (isShowingKeyboard_) {
            const bool controllerNearKeyboard =
                keyboardModelRenderer_.IsPointNearKeyboard(remotePose.Translation);
            const bool handNearKeyboard = (hand != nullptr) && hand->IsPositionValid() &&
                keyboardModelRenderer_.IsPointNearKeyboard(
                    FromXrVector3f(hand->GetScaledJointPose(XR_HAND_JOINT_INDEX_TIP_EXT).position));
            if (controllerNearKeyboard || handNearKeyboard) {
                // Don't interact with UI if controller/hand near keyboard
                return;
            }
        }

        const bool didPinch = remoteIndexTrigger > 0.25f;
        ui_.AddHitTestRay(remotePose, didPinch, static_cast<int>(device));
    }

    void UpdateHandInteraction(InputHandedness handedness, XrHandHelper& hand) {
        const auto rayInputSource = (handedness == InputHandedness::Left)
            ? XR_VIRTUAL_KEYBOARD_INPUT_SOURCE_HAND_RAY_LEFT_META
            : XR_VIRTUAL_KEYBOARD_INPUT_SOURCE_HAND_RAY_RIGHT_META;
        const auto directInputSource = (handedness == InputHandedness::Left)
            ? XR_VIRTUAL_KEYBOARD_INPUT_SOURCE_HAND_DIRECT_INDEX_TIP_LEFT_META
            : XR_VIRTUAL_KEYBOARD_INPUT_SOURCE_HAND_DIRECT_INDEX_TIP_RIGHT_META;

        const XrPosef& xrAimPose = hand.AimPose();
        const XrPosef& xrTouchPose = hand.GetScaledJointPose(XR_HAND_JOINT_INDEX_TIP_EXT);
        const bool didPinch = hand.IndexPinching();
        XrPosef interactorRootPose = hand.WristRootPose();

        // Send both ray and direct input and let the runtime decide which best to use
        const auto result =
            virtualKeyboard_->SendVirtualKeyboardInput(
                GetLocalSpace(), rayInputSource, xrAimPose, didPinch, &interactorRootPose) &&
            virtualKeyboard_->SendVirtualKeyboardInput(
                GetLocalSpace(), directInputSource, xrTouchPose, didPinch, &interactorRootPose);

        // Handle poke limiting
        if (result) {
            hand.ModifyWristRoot(interactorRootPose);
        }
    }

    void UpdateControllerInteraction(
        InputHandedness handedness,
        float remoteIndexTrigger,
        const OVR::Posef& remotePointPose,
        const OVR::Posef& remotePose,
        OVR::Posef& adjustedRemotePose) {
        const auto rayInputSource = (handedness == InputHandedness::Left)
            ? XR_VIRTUAL_KEYBOARD_INPUT_SOURCE_CONTROLLER_RAY_LEFT_META
            : XR_VIRTUAL_KEYBOARD_INPUT_SOURCE_CONTROLLER_RAY_RIGHT_META;

        const auto directInputSource = (handedness == InputHandedness::Left)
            ? XR_VIRTUAL_KEYBOARD_INPUT_SOURCE_CONTROLLER_DIRECT_LEFT_META
            : XR_VIRTUAL_KEYBOARD_INPUT_SOURCE_CONTROLLER_DIRECT_RIGHT_META;

        const auto& xrAimPose = ToXrPosef(remotePointPose);
        const auto& xrTouchPose = ToXrPosef(remotePointPose);
        const bool didPinch = remoteIndexTrigger > 0.25f;
        XrPosef interactorRootPose = ToXrPosef(remotePose);

        // Send both ray and direct input and let the runtime decide which best to use
        auto result =
            virtualKeyboard_->SendVirtualKeyboardInput(
                GetLocalSpace(), rayInputSource, xrAimPose, didPinch, &interactorRootPose) &&
            virtualKeyboard_->SendVirtualKeyboardInput(
                GetLocalSpace(), directInputSource, xrTouchPose, didPinch, &interactorRootPose);

        // Handle poke limiting
        if (result) {
            adjustedRemotePose = FromXrPosef(interactorRootPose);
        }
    }

    void UpdateKeyboardInteractions(const OVRFW::ovrApplFrameIn& in) {
        if (keyboardExtensionAvailable_ && !isMovingKeyboard_) {
            if (handL_->AreLocationsActive()) {
                UpdateHandInteraction(InputHandedness::Left, *handL_);
            } else if (in.LeftRemoteTracked) {
                UpdateControllerInteraction(
                    InputHandedness::Left,
                    in.LeftRemoteIndexTrigger,
                    in.LeftRemotePointPose,
                    in.LeftRemotePose,
                    leftAdjustedRemotePose_);
            }

            if (handR_->AreLocationsActive()) {
                UpdateHandInteraction(InputHandedness::Right, *handR_);
            } else if (in.RightRemoteTracked) {
                UpdateControllerInteraction(
                    InputHandedness::Right,
                    in.RightRemoteIndexTrigger,
                    in.RightRemotePointPose,
                    in.RightRemotePose,
                    rightAdjustedRemotePose_);
            }
        }
    }

    void UpdateKeyboardMoving(const OVRFW::ovrApplFrameIn& in) {
        DetermineHandedness(in);
        if (isMovingKeyboard_) {
            OVR::Posef activePointerPose = OVR::Posef::Identity();
            OVR::Vector2f distanceScaleMod = OVR::Vector2f::ZERO;
            bool shouldFlip = false;

            if (currentHandedness_ == InputHandedness::Left) {
                if (handsExtensionAvailable_ && handL_->AreLocationsActive()) {
                    activePointerPose = FromXrPosef(handL_->AimPose());
                    shouldFlip = true;
                } else if (in.LeftRemoteTracked) {
                    activePointerPose = in.LeftRemotePointPose;
                    distanceScaleMod = in.LeftRemoteJoystick;
                }
            } else {
                if (handsExtensionAvailable_ && handR_->AreLocationsActive()) {
                    activePointerPose = FromXrPosef(handR_->AimPose());
                } else if (in.RightRemoteTracked) {
                    activePointerPose = in.RightRemotePointPose;
                    distanceScaleMod = in.RightRemoteJoystick;
                }
            }
            UpdateKeyboardPosition(activePointerPose, distanceScaleMod, shouldFlip);
        }

        // Query and update location before render
        VirtualKeyboardLocation location;
        if (virtualKeyboard_->GetVirtualKeyboardLocation(
                GetLocalSpace(), ToXrTime(in.PredictedDisplayTime), &location)) {
            currentPose_ = FromXrPosef(location.pose);
            currentScale_ = location.scale;

            if (keyboardModelRenderer_.IsModelLoaded()) {
                keyboardModelRenderer_.Update(currentPose_, OVR::Vector3f(currentScale_));
                auto bounds = keyboardModelRenderer_.GetCollisionBounds();
                keyboardHitTest_->SetLocalPose(currentPose_);
                auto keyboardSize = bounds.GetSize() * currentScale_;
                if (keyboardSize_ != keyboardSize) {
                    keyboardSize_ = keyboardSize;
                    keyboardHitTest_->SetSurfaceDims(
                        0,
                        {keyboardSize_.x * OVRFW::VRMenuObject::TEXELS_PER_METER,
                         keyboardSize_.y * OVRFW::VRMenuObject::TEXELS_PER_METER});
                    keyboardHitTest_->RegenerateSurfaceGeometry(0, false);
                }
            }
        }
    }

    void OnCommitText(const std::string& text) {
        ALOGV("VIRTUALKEYBOARD Text committed: %s", text.c_str());
        textInputBuffer_.append(text);
        textInput_->SetText(textInputBuffer_.c_str());
        eventLog_->SetText("Text Committed: %s", text.c_str());
    }

    void OnBackspace() {
        ALOGV("VIRTUALKEYBOARD Backspace");
        if (!textInputBuffer_.empty()) {
            textInputBuffer_.pop_back();
            textInput_->SetText(textInputBuffer_.c_str());
        }
        eventLog_->SetText("Backspace Pressed");
    }

    void OnEnter() {
        ALOGV("VIRTUALKEYBOARD Enter");
        textInputBuffer_.append("\n");
        textInput_->SetText(textInputBuffer_.c_str());
        eventLog_->SetText("Enter Pressed");
    }

    void OnKeyboardShown() {
        ALOGV("VIRTUALKEYBOARD Shown");
        isShowingKeyboard_ = true;
        DisableButton(showKeyboardButton_);
        EnableButton(hideKeyboardButton_);
        EnableButton(enableMoveKeyboardButton_);
        EnableButton(showNearKeyboardButton_);
        EnableButton(showFarKeyboardButton_);
        keyboardHitTest_->SetVisible(true);
        eventLog_->SetText("Keyboard Shown");
    }

    void OnKeyboardHidden() {
        ALOGV("VIRTUALKEYBOARD Hidden");
        isShowingKeyboard_ = false;
        EnableButton(showKeyboardButton_);
        DisableButton(hideKeyboardButton_);
        DisableButton(enableMoveKeyboardButton_);
        DisableButton(showNearKeyboardButton_);
        DisableButton(showFarKeyboardButton_);
        keyboardHitTest_->SetVisible(false);
        eventLog_->SetText("Keyboard Hidden");
    }

   private:
    bool keyboardExtensionAvailable_ = false;
    bool handsExtensionAvailable_ = false;
    bool renderModelExtensionAvailable_ = false;
    bool uiInitialized_ = false;

    // hands - xr interface
    std::unique_ptr<XrHandHelper> handL_;
    std::unique_ptr<XrHandHelper> handR_;
    // hands/controllers - rendering
    OVRFW::HandRenderer handRendererL_;
    OVRFW::HandRenderer handRendererR_;
    OVRFW::ControllerRenderer controllerRenderL_;
    OVRFW::ControllerRenderer controllerRenderR_;

    // keyboard - xr interface
    std::unique_ptr<XrVirtualKeyboardHelper> virtualKeyboard_;
    // render model - xr interface
    std::unique_ptr<XrRenderModelHelper> renderModel_;
    // keyboard - rendering
    VirtualKeyboardModelRenderer keyboardModelRenderer_;
    XrRenderModelKeyFB modelKey_ = XR_NULL_RENDER_MODEL_KEY_FB;

    // UI
    OVRFW::TinyUI ui_;
    OVRFW::SimpleBeamRenderer beamRenderer_;
    std::vector<OVRFW::ovrBeamRenderer::handle_t> beams_;
    OVRFW::ovrParticleSystem particleSystem_;
    OVRFW::ovrParticleSystem::handle_t leftControllerPoint_;
    OVRFW::ovrParticleSystem::handle_t rightControllerPoint_;

    OVRFW::VRMenuObject* textInput_ = nullptr;
    std::string textInputBuffer_;
    OVRFW::VRMenuObject* eventLog_ = nullptr;

    OVRFW::VRMenuObject* keyboardHitTest_ = nullptr;
    OVR::Vector3f keyboardSize_ = OVR::Vector3f::ZERO;

    OVRFW::VRMenuObject* showKeyboardButton_ = nullptr;
    OVRFW::VRMenuObject* hideKeyboardButton_ = nullptr;
    bool isShowingKeyboard_ = false;

    OVRFW::VRMenuObject* enableMoveKeyboardButton_ = nullptr;
    OVRFW::VRMenuObject* showNearKeyboardButton_ = nullptr;
    OVRFW::VRMenuObject* showFarKeyboardButton_ = nullptr;
    bool isMovingKeyboard_ = false;
    float keyboardMoveDistance_ = 0.0f;
    XrVirtualKeyboardLocationTypeMETA locationType_ = XR_VIRTUAL_KEYBOARD_LOCATION_TYPE_DIRECT_META;

    OVRFW::VRMenuObject* clearTextButton_ = nullptr;

    InputHandedness currentHandedness_ = InputHandedness::Unknown;

    OVR::Posef currentPose_ = OVR::Posef::Identity();
    float currentScale_ = 1.0f;
    OVR::Posef leftAdjustedRemotePose_ = OVR::Posef::Identity();
    OVR::Posef rightAdjustedRemotePose_ = OVR::Posef::Identity();
};

ENTRY_POINT(XrVirtualKeyboardApp)
