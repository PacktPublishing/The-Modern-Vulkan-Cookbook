/************************************************************************************************
Filename    :   ModelRenderer.h
Content     :   A one stop for models from the render model extension
Created     :   April 2021
Authors     :   Federico Schliemann
Copyright   :   Copyright (c) Facebook Technologies, LLC and its affiliates. All rights reserved.
************************************************************************************************/
#pragma once

#include <memory>
#include <string>
#include <vector>

/// Sample Framework
#include "Misc/Log.h"
#include "Model/SceneView.h"
#include "Render/GlProgram.h"
#include "Render/SurfaceRender.h"
#include "OVR_FileSys.h"
#include "OVR_Math.h"

#if defined(ANDROID)
#define XR_USE_GRAPHICS_API_OPENGL_ES 1
#define XR_USE_PLATFORM_ANDROID 1
#else
#include "unknwn.h"
#define XR_USE_GRAPHICS_API_OPENGL 1
#define XR_USE_PLATFORM_WIN32 1
#endif

#include <openxr/openxr.h>
#include <openxr/openxr_oculus_helpers.h>
#include <openxr/openxr_platform.h>

namespace OVRFW {

class ModelRenderer {
   public:
    ModelRenderer() = default;
    ~ModelRenderer() = default;

    enum UpdateOffset { UpdateOffset_None, UpdateOffset_Grip };

    bool Init(std::vector<uint8_t>& modelBuffer);
    void Shutdown();
    void Update(const OVR::Posef& pose, UpdateOffset updateOffset = UpdateOffset_None);
    void Render(std::vector<ovrDrawSurface>& surfaceList);
    bool IsInitialized() const { return Initialized;}

   public:
    OVR::Vector3f SpecularLightDirection;
    OVR::Vector3f SpecularLightColor;
    OVR::Vector3f AmbientLightColor;
    bool UseSolidTexture = true;
    float Opacity = 1.0f;

   private:
    bool Initialized = false;
    float AlphaBlendFactor = 1.0f;
    GlProgram ProgRenderModel;
    ModelFile* RenderModel = nullptr;
    GlTexture RenderModelTextureSolid;
    OVR::Matrix4f Transform;
    OVR::Posef GripPose = OVR::Posef::Identity();
};

} // namespace OVRFW
