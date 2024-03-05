/************************************************************************************************
Filename    :   XrVirtualKeyboardHelper.cpp
Content     :   Helper inteface for openxr XR_META_virtual_keyboard extension
Created     :   April 2022
Authors     :   Juan Pablo León, Robert Memmott, Peter Chan, Brent Housen, Chiara Coetzee
Language    :   C++
Copyright   :   Copyright (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.
************************************************************************************************/

#include "XrVirtualKeyboardHelper.h"

#include <Misc/Log.h>

std::vector<const char*> XrVirtualKeyboardHelper::RequiredExtensionNames() {
    return {XR_META_VIRTUAL_KEYBOARD_EXTENSION_NAME};
}

XrVirtualKeyboardHelper::XrVirtualKeyboardHelper(XrInstance instance) : XrHelper(instance) {
    /// Hook up extensions for keyboard tracking
    oxr(xrGetInstanceProcAddr(
        instance,
        "xrCreateVirtualKeyboardMETA",
        (PFN_xrVoidFunction*)(&xrCreateVirtualKeyboardMETA_)));
    oxr(xrGetInstanceProcAddr(
        instance,
        "xrDestroyVirtualKeyboardMETA",
        (PFN_xrVoidFunction*)(&xrDestroyVirtualKeyboardMETA_)));
    oxr(xrGetInstanceProcAddr(
        instance,
        "xrCreateVirtualKeyboardSpaceMETA",
        (PFN_xrVoidFunction*)(&xrCreateVirtualKeyboardSpaceMETA_)));
    oxr(xrGetInstanceProcAddr(
        instance,
        "xrSuggestVirtualKeyboardLocationMETA",
        (PFN_xrVoidFunction*)(&xrSuggestVirtualKeyboardLocationMETA_)));
    oxr(xrGetInstanceProcAddr(
        instance,
        "xrGetVirtualKeyboardScaleMETA",
        (PFN_xrVoidFunction*)(&xrGetVirtualKeyboardScaleMETA_)));
    oxr(xrGetInstanceProcAddr(
        instance,
        "xrSetVirtualKeyboardModelVisibilityMETA",
        (PFN_xrVoidFunction*)(&xrSetVirtualKeyboardModelVisibilityMETA_)));
    oxr(xrGetInstanceProcAddr(
        instance,
        "xrGetVirtualKeyboardModelAnimationStatesMETA",
        (PFN_xrVoidFunction*)(&xrGetVirtualKeyboardModelAnimationStatesMETA_)));
    oxr(xrGetInstanceProcAddr(
        instance,
        "xrGetVirtualKeyboardDirtyTexturesMETA",
        (PFN_xrVoidFunction*)(&xrGetVirtualKeyboardDirtyTexturesMETA_)));
    oxr(xrGetInstanceProcAddr(
        instance,
        "xrGetVirtualKeyboardTextureDataMETA",
        (PFN_xrVoidFunction*)(&xrGetVirtualKeyboardTextureDataMETA_)));
    oxr(xrGetInstanceProcAddr(
        instance,
        "xrSendVirtualKeyboardInputMETA",
        (PFN_xrVoidFunction*)(&xrSendVirtualKeyboardInputMETA_)));
    oxr(xrGetInstanceProcAddr(
        instance,
        "xrChangeVirtualKeyboardTextContextMETA",
        (PFN_xrVoidFunction*)(&xrChangeVirtualKeyboardTextContext_)));
}

bool XrVirtualKeyboardHelper::SessionInit(XrSession session) {
    session_ = session;
    return true;
}

bool XrVirtualKeyboardHelper::SessionEnd() {
    session_ = XR_NULL_HANDLE;
    return true;
}

bool XrVirtualKeyboardHelper::Update(XrSpace currentSpace, XrTime predictedDisplayTime) {
    return true;
}

bool XrVirtualKeyboardHelper::IsSupported() const {
    XrSystemGetInfo systemGetInfo{XR_TYPE_SYSTEM_GET_INFO};
    systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

    XrSystemId systemId;
    auto result = xrGetSystem(GetInstance(), &systemGetInfo, &systemId);
    if (result != XR_SUCCESS) {
        ALOGE("Failed to get system.");
        return false;
    }

    XrSystemProperties systemProperties{XR_TYPE_SYSTEM_PROPERTIES};
    XrSystemVirtualKeyboardPropertiesMETA virtualKeyboardProps{
        XR_TYPE_SYSTEM_VIRTUAL_KEYBOARD_PROPERTIES_META};
    systemProperties.next = &virtualKeyboardProps;
    result = xrGetSystemProperties(GetInstance(), systemId, &systemProperties);
    if (result != XR_SUCCESS) {
        ALOGE("Failed to get system properties.");
        return false;
    }

    return virtualKeyboardProps.supportsVirtualKeyboard != XR_FALSE;
}

bool XrVirtualKeyboardHelper::HasVirtualKeyboard() const {
    return keyboardHandle_ != XR_NULL_HANDLE;
}

bool XrVirtualKeyboardHelper::CreateVirtualKeyboard(XrVirtualKeyboardCreateInfoMETA* createInfo) {
    if (session_ == XR_NULL_HANDLE || GetInstance() == XR_NULL_HANDLE || createInfo == nullptr) {
        // Session needs to be initialized, and the method available
        return false;
    }
    if (keyboardHandle_ != XR_NULL_HANDLE) {
        // Only one at the time
        return false;
    }
    if (xrCreateVirtualKeyboardMETA_) {
        return oxr(xrCreateVirtualKeyboardMETA_(session_, createInfo, &keyboardHandle_));
    }
    return false;
}

bool XrVirtualKeyboardHelper::CreateVirtualKeyboardSpace(
    const XrVirtualKeyboardSpaceCreateInfoMETA* locationInfo) {
    if (session_ == XR_NULL_HANDLE || GetInstance() == XR_NULL_HANDLE ||
        keyboardHandle_ == XR_NULL_HANDLE) {
        // Session needs to be initialized, and the method available
        return false;
    }
    if (xrCreateVirtualKeyboardSpaceMETA_) {
        return oxr(xrCreateVirtualKeyboardSpaceMETA_(session_, keyboardHandle_, locationInfo, &space_));
    }
    return false;
}

bool XrVirtualKeyboardHelper::DestroyVirtualKeyboard() {
    if (session_ == XR_NULL_HANDLE || GetInstance() == XR_NULL_HANDLE) {
        // Session needs to be initialized, and the method available
        return false;
    }
    if (keyboardHandle_ == XR_NULL_HANDLE) {
        // No need to destroy an unexisting keyboard
        return true;
    }
    if (xrDestroyVirtualKeyboardMETA_) {
        bool result = oxr(xrDestroyVirtualKeyboardMETA_(keyboardHandle_));
        if (result) {
            keyboardHandle_ = XR_NULL_HANDLE;
        }
        return result;
    }
    return false;
}

bool XrVirtualKeyboardHelper::SuggestVirtualKeyboardLocation(
    const XrVirtualKeyboardLocationInfoMETA* locationInfo) {
    if (session_ == XR_NULL_HANDLE || GetInstance() == XR_NULL_HANDLE ||
        keyboardHandle_ == XR_NULL_HANDLE) {
        // Session needs to be initialized, and the method available
        return false;
    }
    if (xrSuggestVirtualKeyboardLocationMETA_) {
        return oxr(xrSuggestVirtualKeyboardLocationMETA_(keyboardHandle_, locationInfo));
    }
    return false;
}

bool XrVirtualKeyboardHelper::GetVirtualKeyboardLocation(
    XrSpace baseSpace,
    XrTime time,
    VirtualKeyboardLocation* keyboardLocation) {
    if (session_ == XR_NULL_HANDLE || GetInstance() == XR_NULL_HANDLE ||
        keyboardHandle_ == XR_NULL_HANDLE) {
        // Session needs to be initialized, and the method available
        return false;
    }
    if (space_ == XR_NULL_HANDLE) {
        ALOGE("Keyboard space is null handle. Call SuggestVirtualKeyboardLocation.");
        return false;
    }

    if (xrGetVirtualKeyboardScaleMETA_) {
        XrSpaceLocation location{XR_TYPE_SPACE_LOCATION};
        auto result = xrLocateSpace(space_, baseSpace, time, &location);
        if (result != XR_SUCCESS) {
            ALOGE("Failed to locate virtual keyboard: %i", result);
            return false;
        };
        keyboardLocation->pose = location.pose;
        result = xrGetVirtualKeyboardScaleMETA_(keyboardHandle_, &keyboardLocation->scale);
        if (result != XR_SUCCESS) {
            ALOGE("Failed to get virtual keyboard scale: %i", result);
            return false;
        };
        return true;
    }
    return false;
}

bool XrVirtualKeyboardHelper::ShowModel(bool visible) {
    if (session_ == XR_NULL_HANDLE || GetInstance() == XR_NULL_HANDLE ||
        keyboardHandle_ == XR_NULL_HANDLE || xrSetVirtualKeyboardModelVisibilityMETA_ == nullptr) {
        // Session needs to be initialized, and the method available
        return false;
    }

    XrVirtualKeyboardModelVisibilitySetInfoMETA modelVisibility{
        XR_TYPE_VIRTUAL_KEYBOARD_MODEL_VISIBILITY_SET_INFO_META};
    modelVisibility.visible = visible ? XR_TRUE : XR_FALSE;
    bool result = oxr(xrSetVirtualKeyboardModelVisibilityMETA_(keyboardHandle_, &modelVisibility));
    if (!result) {
        return false;
    }

    return result;
}

bool XrVirtualKeyboardHelper::GetModelAnimationStates(
    XrVirtualKeyboardModelAnimationStatesMETA& modelAnimationStates) {
    if (session_ == XR_NULL_HANDLE || GetInstance() == XR_NULL_HANDLE ||
        keyboardHandle_ == XR_NULL_HANDLE ||
        xrGetVirtualKeyboardModelAnimationStatesMETA_ == nullptr) {
        // Session needs to be initialized, and the method available
        return false;
    }

    modelAnimationStates = {XR_TYPE_VIRTUAL_KEYBOARD_MODEL_ANIMATION_STATES_META};
    modelAnimationStates.stateCapacityInput = 0;
    bool result =
        oxr(xrGetVirtualKeyboardModelAnimationStatesMETA_(keyboardHandle_, &modelAnimationStates));
    if (!result) {
        return false;
    }

    // If there are no animations, we are done
    if (modelAnimationStates.stateCountOutput == 0) {
        return true;
    }

    animationStatesBuffer_.resize(
        modelAnimationStates.stateCountOutput, {XR_TYPE_VIRTUAL_KEYBOARD_ANIMATION_STATE_META});
    modelAnimationStates.stateCapacityInput = modelAnimationStates.stateCountOutput;
    modelAnimationStates.states = animationStatesBuffer_.data();
    result =
        oxr(xrGetVirtualKeyboardModelAnimationStatesMETA_(keyboardHandle_, &modelAnimationStates));
    return result;
}

bool XrVirtualKeyboardHelper::GetDirtyTextures(std::vector<uint64_t>& textureIds) {
    if (session_ == XR_NULL_HANDLE || GetInstance() == XR_NULL_HANDLE ||
        keyboardHandle_ == XR_NULL_HANDLE || xrGetVirtualKeyboardDirtyTexturesMETA_ == nullptr) {
        return false;
    }

    uint32_t textureIdCountOutput = 0;
    bool result = oxr(xrGetVirtualKeyboardDirtyTexturesMETA_(keyboardHandle_, 0, &textureIdCountOutput, nullptr));
    if (!result) {
        return false;
    }

    // If nothing is dirty, we are done
    if (textureIdCountOutput == 0) {
        return true;
    }

    textureIds.resize(textureIdCountOutput);
    result = oxr(xrGetVirtualKeyboardDirtyTexturesMETA_(keyboardHandle_, textureIdCountOutput, &textureIdCountOutput, textureIds.data()));
    return result;
}

bool XrVirtualKeyboardHelper::GetTextureData(
    uint64_t textureId,
    XrVirtualKeyboardTextureDataMETA& textureData) {
    if (session_ == XR_NULL_HANDLE || GetInstance() == XR_NULL_HANDLE ||
        keyboardHandle_ == XR_NULL_HANDLE || xrGetVirtualKeyboardTextureDataMETA_ == nullptr) {
        // Session needs to be initialized, and the method available
        return false;
    }

    textureData = {XR_TYPE_VIRTUAL_KEYBOARD_TEXTURE_DATA_META};
    textureData.bufferCapacityInput = 0;
    bool result =
        oxr(xrGetVirtualKeyboardTextureDataMETA_(keyboardHandle_, textureId, &textureData));
    if (!result) {
        return false;
    }

    // No data available, try again later
    if (textureData.bufferCountOutput == 0) {
        return false;
    }

    textureDataBuffer_.resize(textureData.bufferCountOutput);
    textureData.bufferCapacityInput = textureData.bufferCountOutput;
    textureData.buffer = textureDataBuffer_.data();
    result = oxr(xrGetVirtualKeyboardTextureDataMETA_(keyboardHandle_, textureId, &textureData));
    return result;
}

bool XrVirtualKeyboardHelper::SendVirtualKeyboardInput(
    XrSpace space,
    XrVirtualKeyboardInputSourceMETA source,
    const XrPosef& pointerPose,
    bool pressed,
    XrPosef* interactorRootPose) {
    if (session_ == XR_NULL_HANDLE || GetInstance() == XR_NULL_HANDLE ||
        keyboardHandle_ == XR_NULL_HANDLE) {
        // Session needs to be initialized, and the method available
        return false;
    }
    if (xrSendVirtualKeyboardInputMETA_) {
        XrVirtualKeyboardInputInfoMETA info{XR_TYPE_VIRTUAL_KEYBOARD_INPUT_INFO_META};
        info.inputSource = source;
        info.inputSpace = space;
        info.inputPoseInSpace = pointerPose;
        if (pressed) {
            info.inputState |= XR_VIRTUAL_KEYBOARD_INPUT_STATE_PRESSED_BIT_META;
        }
        return oxr(xrSendVirtualKeyboardInputMETA_(keyboardHandle_, &info, interactorRootPose));
    }
    return false;
}

bool XrVirtualKeyboardHelper::UpdateTextContext(const std::string& textContext) {
    if (session_ == XR_NULL_HANDLE || GetInstance() == XR_NULL_HANDLE ||
        keyboardHandle_ == XR_NULL_HANDLE) {
        // Session needs to be initialized, and the method available
        return false;
    }
    if (xrChangeVirtualKeyboardTextContext_) {
        XrVirtualKeyboardTextContextChangeInfoMETA changeInfo{
            XR_TYPE_VIRTUAL_KEYBOARD_TEXT_CONTEXT_CHANGE_INFO_META};
        changeInfo.textContext = textContext.c_str();
        return oxr(xrChangeVirtualKeyboardTextContext_(keyboardHandle_, &changeInfo));
    }
    return false;
}
