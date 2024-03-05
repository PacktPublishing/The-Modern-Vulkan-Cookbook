/************************************************************************************************
Filename    :   XrRenderModelHelper.h
Content     :   Helper inteface for openxr XR_FB_render_model extensions
Created     :   October 2022
Authors     :   Peter Chan
Language    :   C++
Copyright   :   Copyright (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.
************************************************************************************************/

#pragma once

#include <map>

#include "XrHelper.h"

class XrRenderModelHelper : public XrHelper {
   public:
    static std::vector<const char*> RequiredExtensionNames();

   public:
    explicit XrRenderModelHelper(XrInstance instance);

    /// XrHelper Interface
    bool SessionInit(XrSession session) override;
    bool SessionEnd() override;
    bool Update(XrSpace currentSpace, XrTime predictedDisplayTime) override;

    XrRenderModelKeyFB TryGetRenderModelKey(const char* modelPath);
    std::vector<uint8_t> LoadRenderModel(XrRenderModelKeyFB modelKey);

   private:
    void LazyInitialize();

    XrSession session_ = XR_NULL_HANDLE;

    PFN_xrEnumerateRenderModelPathsFB xrEnumerateRenderModelPathsFB_ = nullptr;
    PFN_xrGetRenderModelPropertiesFB xrGetRenderModelPropertiesFB_ = nullptr;
    PFN_xrLoadRenderModelFB xrLoadRenderModelFB_ = nullptr;

    std::map<std::string, XrRenderModelPropertiesFB> properties_;

    bool isInitialized_ = false;
};
