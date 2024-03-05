#pragma once

#if defined(ANDROID)
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#define XR_USE_GRAPHICS_API_OPENGL_ES 1
#define XR_USE_PLATFORM_ANDROID 1
#else
#include "unknwn.h"
#include "Render/GlWrapperWin32.h"
#define XR_USE_GRAPHICS_API_OPENGL 1
#define XR_USE_PLATFORM_WIN32 1
#endif // defined(ANDROID)

#include <openxr/meta_environment_depth.h>
#include <openxr/openxr.h>
#include <openxr/openxr_oculus_helpers.h>
#include <openxr/openxr_platform.h>

#include "XrPassthroughOcclusionGl.h"

void OXR_CheckErrors(XrResult result, const char* function, bool failOnError);
#define OXR(func) OXR_CheckErrors(func, #func, true);

inline OVR::Matrix4f OvrFromXr(const XrMatrix4x4f& x) {
    return OVR::Matrix4f(
        x.m[0x0],
        x.m[0x1],
        x.m[0x2],
        x.m[0x3],
        x.m[0x4],
        x.m[0x5],
        x.m[0x6],
        x.m[0x7],
        x.m[0x8],
        x.m[0x9],
        x.m[0xa],
        x.m[0xb],
        x.m[0xc],
        x.m[0xd],
        x.m[0xe],
        x.m[0xf]);
}

inline OVR::Quatf OvrFromXr(const XrQuaternionf& q) {
    return OVR::Quatf(q.x, q.y, q.z, q.w);
}

inline OVR::Vector3f OvrFromXr(const XrVector3f& v) {
    return OVR::Vector3f(v.x, v.y, v.z);
}

inline OVR::Posef OvrFromXr(const XrPosef& p) {
    return OVR::Posef(OvrFromXr(p.orientation), OvrFromXr(p.position));
}

/*
================================================================================

Egl

================================================================================
*/

class Egl {
   public:
    Egl() = default;

    void CreateContext(const Egl* shareEgl);
    void DestroyContext();
#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
    EGLint MajorVersion = 0;
    EGLint MinorVersion = 0;
    EGLDisplay Display = 0;
    EGLConfig Config = 0;
    EGLSurface TinySurface = EGL_NO_SURFACE;
    EGLSurface MainSurface = EGL_NO_SURFACE;
    EGLContext Context = EGL_NO_CONTEXT;
#elif defined(XR_USE_GRAPHICS_API_OPENGL)
    HDC hDC = 0;
    HGLRC hGLRC = 0;
#endif
};

/*
================================================================================

App

================================================================================
*/

union CompositionLayerUnion {
    XrCompositionLayerProjection Projection;
    XrCompositionLayerQuad Quad;
    XrCompositionLayerCylinderKHR Cylinder;
    XrCompositionLayerCubeKHR Cube;
    XrCompositionLayerEquirectKHR Equirect;
    XrCompositionLayerPassthroughFB Passthrough;
};

class App {
   public:
    static constexpr int kNumEyes = 2;
    static constexpr int kMaxLayerCount = 16;

    App() = default;

    void HandleSessionStateChanges(XrSessionState state);
    void HandleXrEvents();

    Egl egl;

#if defined(XR_USE_PLATFORM_ANDROID)
    bool Resumed = false;
#endif // defined(XR_USE_PLATFORM_ANDROID)
    bool ShouldExit = false;
    bool Focused = false;

    XrInstance Instance = XR_NULL_HANDLE;
    XrSession Session = XR_NULL_HANDLE;
    XrViewConfigurationProperties ViewportConfig = {};
    XrViewConfigurationView ViewConfigurationView[kNumEyes] = {};
    XrSystemId SystemId = XR_NULL_SYSTEM_ID;
    XrSpace HeadSpace = XR_NULL_HANDLE;
    XrSpace LocalSpace = XR_NULL_HANDLE;
    XrSpace StageSpace = XR_NULL_HANDLE;
    bool SessionActive = false;

    int SwapInterval = 1;
    int CpuLevel = 2;
    int GpuLevel = 3;
    // These threads will be marked as performance threads.
    int MainThreadTid = 0;
    int RenderThreadTid = 0;
    CompositionLayerUnion Layers[kMaxLayerCount] = {};
    int LayerCount = 0;
    XrSwapchain ColorSwapchain = XR_NULL_HANDLE;
    uint32_t SwapchainLength = 0;

    // Environment Depth Provider.
    XrEnvironmentDepthProviderMETA EnvironmentDepthProvider = XR_NULL_HANDLE;
    XrEnvironmentDepthSwapchainMETA EnvironmentDepthSwapchain = XR_NULL_HANDLE;

    // Provided by XrPassthroughOcclusionGl, which is not aware of VrApi or OpenXR.
    AppRenderer appRenderer;
};
