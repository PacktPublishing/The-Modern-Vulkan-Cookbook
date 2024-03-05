/************************************************************************************************
Filename    :   XrRenderModelHelper.cpp
Content     :   Helper inteface for XR_FB_render_model extensions
Created     :   October 2022
Authors     :   Peter Chan
Language    :   C++
Copyright   :   Copyright (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.
************************************************************************************************/

#include "XrRenderModelHelper.h"

#include <Misc/Log.h>

std::vector<const char*> XrRenderModelHelper::RequiredExtensionNames() {
    return {XR_FB_RENDER_MODEL_EXTENSION_NAME};
}

XrRenderModelHelper::XrRenderModelHelper(XrInstance instance) : XrHelper(instance) {
    /// Hook up extensions for device settings
    oxr(xrGetInstanceProcAddr(
        instance,
        "xrEnumerateRenderModelPathsFB",
        (PFN_xrVoidFunction*)(&xrEnumerateRenderModelPathsFB_)));
    oxr(xrGetInstanceProcAddr(
        instance,
        "xrGetRenderModelPropertiesFB",
        (PFN_xrVoidFunction*)(&xrGetRenderModelPropertiesFB_)));
    oxr(xrGetInstanceProcAddr(
        instance, "xrLoadRenderModelFB", (PFN_xrVoidFunction*)(&xrLoadRenderModelFB_)));
}

bool XrRenderModelHelper::SessionInit(XrSession session) {
    session_ = session;
    return true;
}

bool XrRenderModelHelper::SessionEnd() {
    session_ = XR_NULL_HANDLE;
    return true;
}

bool XrRenderModelHelper::Update(XrSpace currentSpace, XrTime predictedDisplayTime) {
    return true;
}

XrRenderModelKeyFB XrRenderModelHelper::TryGetRenderModelKey(const char* modelPath) {
    XrRenderModelKeyFB modelKey = XR_NULL_RENDER_MODEL_KEY_FB;
    if (xrGetRenderModelPropertiesFB_ == nullptr) {
        ALOGE("XrRenderModelHelper: no render model properties function");
        return modelKey;
    }

    LazyInitialize();

    auto iter = properties_.find(modelPath);
    if (iter == properties_.end()) {
        ALOGE("XrRenderModelHelper: model %s not available", modelPath);
        return modelKey;
    }

    XrRenderModelPropertiesFB& modelProp = iter->second;
    modelKey = modelProp.modelKey;
    if (modelKey == XR_NULL_RENDER_MODEL_KEY_FB) {
        // Get properties again to see if the model has become available
        XrPath xrPath;
        oxr(xrStringToPath(GetInstance(), modelPath, &xrPath));
        XrRenderModelPropertiesFB prop{XR_TYPE_RENDER_MODEL_PROPERTIES_FB};
        XrRenderModelCapabilitiesRequestFB capReq{XR_TYPE_RENDER_MODEL_CAPABILITIES_REQUEST_FB};
        capReq.flags = XR_RENDER_MODEL_SUPPORTS_GLTF_2_0_SUBSET_2_BIT_FB;
        prop.next = &capReq;
        XrResult result = xrGetRenderModelPropertiesFB_(session_, xrPath, &prop);
        if (result != XR_SUCCESS) {
            ALOGE("XrRenderModelHelper: failed to load model properties for %s", modelPath);
            return modelKey;
        }
        modelProp = prop; // Update cache
        modelKey = prop.modelKey;
        if (modelKey == XR_NULL_RENDER_MODEL_KEY_FB) {
            ALOGW("XrRenderModelHelper: model %s is still not available", modelPath);
        }
    }

    return modelKey;
}

std::vector<uint8_t> XrRenderModelHelper::LoadRenderModel(XrRenderModelKeyFB modelKey) {
    std::vector<uint8_t> buffer;
    if (xrLoadRenderModelFB_ == nullptr) {
        ALOGE("XrRenderModelHelper: no render model load model function");
        return buffer;
    }

    LazyInitialize();

    if (modelKey == XR_NULL_RENDER_MODEL_KEY_FB) {
        ALOGE("XrRenderModelHelper: Invalid modelKey %u", modelKey);
        return buffer;
    }

    XrRenderModelLoadInfoFB loadInfo = {XR_TYPE_RENDER_MODEL_LOAD_INFO_FB};
    loadInfo.modelKey = modelKey;
    XrRenderModelBufferFB rmb{XR_TYPE_RENDER_MODEL_BUFFER_FB};
    if (!oxr(xrLoadRenderModelFB_(session_, &loadInfo, &rmb))) {
        ALOGE("XrRenderModelHelper: FAILED to load modelKey %u on pass 1", modelKey);
        return buffer;
    }

    ALOG("XrRenderModelHelper: loading modelKey %u size %u", modelKey, rmb.bufferCountOutput);

    buffer.resize(rmb.bufferCountOutput);
    rmb.buffer = buffer.data();
    rmb.bufferCapacityInput = rmb.bufferCountOutput;
    if (!oxr(xrLoadRenderModelFB_(session_, &loadInfo, &rmb))) {
        ALOGE("XrRenderModelHelper: FAILED to load modelKey %u on pass 2", modelKey);
        buffer.resize(0);
        return buffer;
    }

    ALOG("XrRenderModelHelper: loaded modelKey %u buffer size is %u", modelKey, buffer.size());
    return buffer;
}

void XrRenderModelHelper::LazyInitialize() {
    if (isInitialized_) {
        return;
    }

    if (xrEnumerateRenderModelPathsFB_ == nullptr || xrGetRenderModelPropertiesFB_ == nullptr) {
        ALOGE("XrRenderModelHelper: no render model extension functions");
        return;
    }

    // The application *must* call xrEnumerateRenderModelPathsFB to enumerate the valid render model
    // paths that are supported by the runtime before calling xrGetRenderModelPropertiesFB.
    // https://registry.khronos.org/OpenXR/specs/1.0/html/xrspec.html#xrEnumerateRenderModelPathsFB
    uint32_t pathCount = 0;
    oxr(xrEnumerateRenderModelPathsFB_(session_, pathCount, &pathCount, nullptr));
    if (pathCount > 0) {
        ALOG("XrRenderModelHelper: found %u models", pathCount);

        std::vector<XrRenderModelPathInfoFB> pathInfos(pathCount, {XR_TYPE_RENDER_MODEL_PATH_INFO_FB});
        oxr(xrEnumerateRenderModelPathsFB_(session_, pathCount, &pathCount, pathInfos.data()));

        // Try to get properties for each model
        for (const auto& info : pathInfos) {
            char pathString[XR_MAX_PATH_LENGTH];
            uint32_t countOutput = 0;
            oxr(xrPathToString(
                GetInstance(), info.path, XR_MAX_PATH_LENGTH, &countOutput, pathString));
            XrRenderModelPropertiesFB prop{XR_TYPE_RENDER_MODEL_PROPERTIES_FB};
            XrRenderModelCapabilitiesRequestFB capReq{XR_TYPE_RENDER_MODEL_CAPABILITIES_REQUEST_FB};
            capReq.flags = XR_RENDER_MODEL_SUPPORTS_GLTF_2_0_SUBSET_2_BIT_FB;
            prop.next = &capReq;
            if (oxr(xrGetRenderModelPropertiesFB_(session_, info.path, &prop))) {
                ALOG(
                    "XrRenderModelHelper: found properties for %s,  vendorId = %u, modelName = %s, modelKey = %u, modelVersion = %u",
                    pathString,
                    prop.vendorId,
                    prop.modelName,
                    prop.modelKey,
                    prop.modelVersion);
                properties_[pathString] = prop;
            } else {
                ALOGE("XrRenderModelHelper: FAILED to load model properties for %s", pathString);
            }
        }
    }

    isInitialized_ = true;
}
