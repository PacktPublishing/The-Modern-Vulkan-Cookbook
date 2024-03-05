/************************************************************************************************
Filename    :   EnvironmentRenderer.h
Content     :   A variant of ModelRenderer suited for rendering gltf scenes with vertex color based fog
Created     :   July 2023
Authors     :   Alexander Borsboom
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

class EnvironmentRenderer {
   public:
    EnvironmentRenderer() = default;
    ~EnvironmentRenderer() = default;

    bool Init(std::vector<uint8_t>& modelBuffer);
    bool Init(std::string modelPath, OVRFW::ovrFileSys* fileSys);
    void Shutdown();
    void Render(std::vector<ovrDrawSurface>& surfaceList);
    bool IsInitialized() const { return Initialized;}

   public:
    OVR::Vector3f SpecularLightDirection;
    OVR::Vector3f SpecularLightColor;
    OVR::Vector3f AmbientLightColor;
    OVR::Vector3f FogColor;

   private:
    bool Initialized = false;
    GlProgram ProgRenderModel;
    ModelFile* RenderModel = nullptr;
    GlTexture RenderModelTextureSolid;
    OVR::Matrix4f Transform;
    OVR::Size<float>* FogStrengths;
};

} // namespace OVRFW
