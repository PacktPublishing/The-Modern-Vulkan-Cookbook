/************************************************************************************

Filename  : XrPassthroughOcclusion.cpp
Content   : This sample uses the Android NativeActivity class.

Copyright : Copyright (c) Meta Platforms, Inc. and affiliates. All rights reserved.

*************************************************************************************/

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <vector>

#if defined(ANDROID)
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h> // for prctl( PR_SET_NAME )
#include <android/log.h>
#include <android/native_window_jni.h> // for native window JNI
#include <android_native_app_glue.h>
#else
#include <thread>
#endif // defined(ANDROID)

#include "XrPassthroughOcclusion.h"
#include "XrPassthroughOcclusionInput.h"
#include "XrPassthroughOcclusionGl.h"

using namespace OVR;

#if !defined(EGL_OPENGL_ES3_BIT_KHR)
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif

#define OVR_LOG_TAG "XrPassthroughOcclusion"

#if !defined(XR_USE_GRAPHICS_API_OPENGL_ES) && !defined(XR_USE_GRAPHICS_API_OPENGL)
#error A graphics backend must be defined!
#elif defined(XR_USE_GRAPHICS_API_OPENGL_ES) && defined(XR_USE_GRAPHICS_API_OPENGL)
#error Only one graphics backend shall be defined!
#endif

#if defined(ANDROID)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, OVR_LOG_TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, OVR_LOG_TAG, __VA_ARGS__)
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, OVR_LOG_TAG, __VA_ARGS__)
#else
#define ALOGE(...)       \
    printf("ERROR: ");   \
    printf(__VA_ARGS__); \
    printf("\n")
#define ALOGV(...)       \
    printf("VERBOSE: "); \
    printf(__VA_ARGS__); \
    printf("\n")
#endif

namespace {

constexpr int kNumEyes = 2;

// The GL driver on Quest devices does not resolve depth for multisampled buffers.
// Must either use VK or avoid using multisampling.
constexpr int kNumMultiSamples = 1;

// Near and far plane values. Must be consistent between the inputs to the
// projection matrix computation and depth submission.
constexpr float kProjectionNearZ = 0.1f;
constexpr float kProjectionFarZ = 10.0f;

} // namespace

/*
================================================================================

OpenXR Utility Functions

================================================================================
*/

XrInstance instance;
void OXR_CheckErrors(XrResult result, const char* function, bool failOnError) {
    if (XR_FAILED(result)) {
        char errorBuffer[XR_MAX_RESULT_STRING_SIZE];
        xrResultToString(instance, result, errorBuffer);
        if (failOnError) {
            ALOGE("OpenXR error: %s: %s\n", function, errorBuffer);
            std::abort();
        } else {
            ALOGV("OpenXR error: %s: %s\n", function, errorBuffer);
        }
    }
}

// Utility function which appends an OpenXR structure to the end of the `next` chain
// of another OpenXR structure.
template <typename TChain, typename TAppend>
void appendToNextChain(TChain& chainRoot, TAppend& toAppend) {
    XrBaseOutStructure* node = reinterpret_cast<XrBaseOutStructure*>(&chainRoot);
    XrBaseOutStructure* toAppendPtr = reinterpret_cast<XrBaseOutStructure*>(&toAppend);
    while (node->next != nullptr) {
        node = node->next;
    }
    node->next = toAppendPtr;
}

#define DECL_PFN(pfn) PFN_##pfn pfn = nullptr
#define INIT_PFN(pfn) OXR(xrGetInstanceProcAddr(instance, #pfn, (PFN_xrVoidFunction*)(&pfn)))

DECL_PFN(xrCreatePassthroughFB);
DECL_PFN(xrDestroyPassthroughFB);
DECL_PFN(xrPassthroughStartFB);
DECL_PFN(xrPassthroughPauseFB);
DECL_PFN(xrCreatePassthroughLayerFB);
DECL_PFN(xrDestroyPassthroughLayerFB);
DECL_PFN(xrPassthroughLayerSetStyleFB);
DECL_PFN(xrPassthroughLayerPauseFB);
DECL_PFN(xrPassthroughLayerResumeFB);

DECL_PFN(xrCreateEnvironmentDepthProviderMETA);
DECL_PFN(xrDestroyEnvironmentDepthProviderMETA);
DECL_PFN(xrStartEnvironmentDepthProviderMETA);
DECL_PFN(xrStopEnvironmentDepthProviderMETA);
DECL_PFN(xrCreateEnvironmentDepthSwapchainMETA);
DECL_PFN(xrDestroyEnvironmentDepthSwapchainMETA);
DECL_PFN(xrEnumerateEnvironmentDepthSwapchainImagesMETA);
DECL_PFN(xrGetEnvironmentDepthSwapchainStateMETA);
DECL_PFN(xrAcquireEnvironmentDepthImageMETA);
DECL_PFN(xrSetEnvironmentDepthHandRemovalMETA);

/*
================================================================================

Environment Depth View Conversion Functions

================================================================================
*/

OVR::Matrix3f MakePinholeProjectionMatrix(const XrFovf& cameraFovAngles) {
    const float tanLeft = std::tan(-cameraFovAngles.angleLeft);
    const float tanRight = std::tan(cameraFovAngles.angleRight);
    const float tanUp = std::tan(cameraFovAngles.angleUp);
    const float tanDown = std::tan(-cameraFovAngles.angleDown);

    const float tanAngleWidth = tanRight + tanLeft;
    const float tanAngleHeight = tanUp + tanDown;

    OVR::Matrix3f m = OVR::Matrix3f::Identity();
    m(0, 0) = 1.0f / tanAngleWidth;
    m(1, 1) = 1.0f / tanAngleHeight;
    m(0, 2) = -tanLeft / tanAngleWidth;
    m(1, 2) = -tanDown / tanAngleHeight;
    m(2, 2) = -1.0;

    return m;
}

OVR::Matrix3f MakePinholeUnprojectionMatrix(const XrFovf& cameraFovAngles) {
    const float tanLeft = std::tan(-cameraFovAngles.angleLeft);
    const float tanRight = std::tan(cameraFovAngles.angleRight);
    const float tanUp = std::tan(cameraFovAngles.angleUp);
    const float tanDown = std::tan(-cameraFovAngles.angleDown);

    OVR::Matrix3f m = OVR::Matrix3f::Identity();
    m(0, 0) = tanRight + tanLeft;
    m(1, 1) = tanUp + tanDown;
    m(0, 2) = -tanLeft;
    m(1, 2) = -tanDown;
    m(2, 2) = -1.0;

    return m;
}

OVR::Matrix3f MakeQuadFromNormalizedCoordTransform(const OVR::Vector2f& quadSize) {
    OVR::Matrix3f T_Quad_Normalized;
    T_Quad_Normalized(0, 0) = quadSize.x;
    T_Quad_Normalized(1, 1) = quadSize.y;
    T_Quad_Normalized(2, 2) = 1.0;
    return T_Quad_Normalized;
}

OVR::Matrix3f MakeNormalizedFromQuadCoordTransform(const OVR::Vector2f& quadSize) {
    OVR::Matrix3f T_Normalized_Quad;
    T_Normalized_Quad(0, 0) = 1.0 / quadSize.x;
    T_Normalized_Quad(1, 1) = 1.0 / quadSize.y;
    T_Normalized_Quad(2, 2) = 1.0;
    return T_Normalized_Quad;
}

OVR::Quatf OvrFromXrQuaternion(const XrQuaternionf& xrOrientation) {
    return OVR::Quatf(xrOrientation.x, xrOrientation.y, xrOrientation.z, xrOrientation.w);
}

OVR::Matrix3f MakeDestFromSourceMapping(
    const OVR::Vector2f& destSize,
    const XrFovf& destFov,
    const XrPosef& xfLocalFromDestCamera,
    const OVR::Vector2f& sourceSize,
    const XrFovf& sourceFov,
    const XrPosef& xfLocalFromSourceCamera) {
    // Unprojection of points in source image to bearing vectors in the camera.
    const OVR::Matrix3f T_SourceCamera_SourceNormCoord = MakePinholeUnprojectionMatrix(sourceFov);

    // Projection of points from the dest camera to the image.
    const OVR::Matrix3f T_DestNormCoord_DestCamera = MakePinholeProjectionMatrix(destFov);

    // Construct quaternions from rotation components of the two transformation from views.
    const OVR::Quatf Q_LocalFromDestCamera = OvrFromXrQuaternion(xfLocalFromDestCamera.orientation);
    const OVR::Quatf Q_LocalFromSourceCamera =
        OvrFromXrQuaternion(xfLocalFromSourceCamera.orientation);

    // Rotation between the views.
    const OVR::Matrix3f R_DestCamera_SourceCamera =
        OVR::Matrix3f(Q_LocalFromDestCamera).Transposed() * OVR::Matrix3f(Q_LocalFromSourceCamera);

    // Map [0, 1]x[0, 1] to [0, width]x[0, height].
    const OVR::Matrix3f T_DestCoord_DestNormCoord = MakeQuadFromNormalizedCoordTransform(destSize);

    // Map [0, width]x[0, height] to [0, 1]x[0, 1].
    const OVR::Matrix3f T_SourceNormCoord_SourceCoord =
        MakeNormalizedFromQuadCoordTransform(sourceSize);

    // Put it all together.
    const OVR::Matrix3f T_DestCoord_SourceCoord = T_DestCoord_DestNormCoord *
        T_DestNormCoord_DestCamera * R_DestCamera_SourceCamera * T_SourceCamera_SourceNormCoord *
        T_SourceNormCoord_SourceCoord;
    return T_DestCoord_SourceCoord;
}

/*
================================================================================

Egl Utility Functions

================================================================================
*/

#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
static const char* EglErrorString(const EGLint error) {
    switch (error) {
        case EGL_SUCCESS:
            return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED:
            return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS:
            return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC:
            return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE:
            return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONTEXT:
            return "EGL_BAD_CONTEXT";
        case EGL_BAD_CONFIG:
            return "EGL_BAD_CONFIG";
        case EGL_BAD_CURRENT_SURFACE:
            return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY:
            return "EGL_BAD_DISPLAY";
        case EGL_BAD_SURFACE:
            return "EGL_BAD_SURFACE";
        case EGL_BAD_MATCH:
            return "EGL_BAD_MATCH";
        case EGL_BAD_PARAMETER:
            return "EGL_BAD_PARAMETER";
        case EGL_BAD_NATIVE_PIXMAP:
            return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW:
            return "EGL_BAD_NATIVE_WINDOW";
        case EGL_CONTEXT_LOST:
            return "EGL_CONTEXT_LOST";
        default:
            return "unknown";
    }
}

void Egl::CreateContext(const Egl* shareEgl) {
    if (Display != 0) {
        return;
    }

    Display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    ALOGV("        eglInitialize( Display, &MajorVersion, &MinorVersion )");
    eglInitialize(Display, &MajorVersion, &MinorVersion);
    // Do NOT use eglChooseConfig, because the Android EGL code pushes in multisample
    // flags in eglChooseConfig if the user has selected the "force 4x MSAA" option in
    // settings, and that is completely wasted for our warp target.
    const int MAX_CONFIGS = 1024;
    EGLConfig configs[MAX_CONFIGS];
    EGLint numConfigs = 0;
    if (eglGetConfigs(Display, configs, MAX_CONFIGS, &numConfigs) == EGL_FALSE) {
        ALOGE("        eglGetConfigs() failed: %s", EglErrorString(eglGetError()));
        return;
    }
    const EGLint configAttribs[] = {
        EGL_RED_SIZE,
        8,
        EGL_GREEN_SIZE,
        8,
        EGL_BLUE_SIZE,
        8,
        EGL_ALPHA_SIZE,
        8, // need alpha for the multi-pass timewarp compositor
        EGL_DEPTH_SIZE,
        0,
        EGL_STENCIL_SIZE,
        0,
        EGL_SAMPLES,
        0,
        EGL_NONE};
    Config = 0;
    for (int i = 0; i < numConfigs; i++) {
        EGLint value = 0;

        eglGetConfigAttrib(Display, configs[i], EGL_RENDERABLE_TYPE, &value);
        if ((value & EGL_OPENGL_ES3_BIT_KHR) != EGL_OPENGL_ES3_BIT_KHR) {
            continue;
        }

        // The pbuffer config also needs to be compatible with normal window rendering
        // so it can share textures with the window context.
        eglGetConfigAttrib(Display, configs[i], EGL_SURFACE_TYPE, &value);
        if ((value & (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) != (EGL_WINDOW_BIT | EGL_PBUFFER_BIT)) {
            continue;
        }

        int j = 0;
        for (; configAttribs[j] != EGL_NONE; j += 2) {
            eglGetConfigAttrib(Display, configs[i], configAttribs[j], &value);
            if (value != configAttribs[j + 1]) {
                break;
            }
        }
        if (configAttribs[j] == EGL_NONE) {
            Config = configs[i];
            break;
        }
    }
    if (Config == 0) {
        ALOGE("        eglChooseConfig() failed: %s", EglErrorString(eglGetError()));
        return;
    }
    EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
    ALOGV("        Context = eglCreateContext( Display, Config, EGL_NO_CONTEXT, contextAttribs )");
    Context = eglCreateContext(
        Display,
        Config,
        (shareEgl != nullptr) ? shareEgl->Context : EGL_NO_CONTEXT,
        contextAttribs);
    if (Context == EGL_NO_CONTEXT) {
        ALOGE("        eglCreateContext() failed: %s", EglErrorString(eglGetError()));
        return;
    }
    const EGLint surfaceAttribs[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};
    ALOGV("        TinySurface = eglCreatePbufferSurface( Display, Config, surfaceAttribs )");
    TinySurface = eglCreatePbufferSurface(Display, Config, surfaceAttribs);
    if (TinySurface == EGL_NO_SURFACE) {
        ALOGE("        eglCreatePbufferSurface() failed: %s", EglErrorString(eglGetError()));
        eglDestroyContext(Display, Context);
        Context = EGL_NO_CONTEXT;
        return;
    }
    ALOGV("        eglMakeCurrent( Display, TinySurface, TinySurface, Context )");
    if (eglMakeCurrent(Display, TinySurface, TinySurface, Context) == EGL_FALSE) {
        ALOGE("        eglMakeCurrent() failed: %s", EglErrorString(eglGetError()));
        eglDestroySurface(Display, TinySurface);
        eglDestroyContext(Display, Context);
        Context = EGL_NO_CONTEXT;
        return;
    }
}

void Egl::DestroyContext() {
    if (Display != 0) {
        ALOGE("        eglMakeCurrent( Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT )");
        if (eglMakeCurrent(Display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT) == EGL_FALSE) {
            ALOGE("        eglMakeCurrent() failed: %s", EglErrorString(eglGetError()));
        }
    }
    if (Context != EGL_NO_CONTEXT) {
        ALOGE("        eglDestroyContext( Display, Context )");
        if (eglDestroyContext(Display, Context) == EGL_FALSE) {
            ALOGE("        eglDestroyContext() failed: %s", EglErrorString(eglGetError()));
        }
        Context = EGL_NO_CONTEXT;
    }
    if (TinySurface != EGL_NO_SURFACE) {
        ALOGE("        eglDestroySurface( Display, TinySurface )");
        if (eglDestroySurface(Display, TinySurface) == EGL_FALSE) {
            ALOGE("        eglDestroySurface() failed: %s", EglErrorString(eglGetError()));
        }
        TinySurface = EGL_NO_SURFACE;
    }
    if (Display != 0) {
        ALOGE("        eglTerminate( Display )");
        if (eglTerminate(Display) == EGL_FALSE) {
            ALOGE("        eglTerminate() failed: %s", EglErrorString(eglGetError()));
        }
        Display = 0;
    }
}

#elif defined(XR_USE_GRAPHICS_API_OPENGL)

#if defined(WIN32)
// Favor the high performance NVIDIA or AMD GPUs
extern "C" {
// http://developer.download.nvidia.com/devzone/devcenter/gamegraphics/files/OptimusRenderingPolicies.pdf
__declspec(dllexport) DWORD NvOptimusEnablement = 0x00000001;
// https://gpuopen.com/learn/amdpowerxpressrequesthighperformance/
__declspec(dllexport) DWORD AmdPowerXpressRequestHighPerformance = 0x00000001;
}
#endif //  defined(WIN32)

void Egl::CreateContext(const Egl*) {
    ovrGl_CreateContext_Windows(&hDC, &hGLRC);
}

void Egl::DestroyContext() {
    ovrGl_DestroyContext_Windows();
}

#endif

void App::HandleSessionStateChanges(XrSessionState state) {
    if (state == XR_SESSION_STATE_READY) {
#if defined(XR_USE_PLATFORM_ANDROID)
        assert(Resumed);
#endif // defined(XR_USE_PLATFORM_ANDROID)
        assert(SessionActive == false);

        XrSessionBeginInfo sessionBeginInfo = {XR_TYPE_SESSION_BEGIN_INFO};
        sessionBeginInfo.primaryViewConfigurationType = ViewportConfig.viewConfigurationType;

        XrResult result;
        OXR(result = xrBeginSession(Session, &sessionBeginInfo));

        SessionActive = (result == XR_SUCCESS);

#if defined(XR_USE_PLATFORM_ANDROID)
        // Set session state once we have entered VR mode and have a valid session object.
        if (SessionActive) {
            XrPerfSettingsLevelEXT cpuPerfLevel = XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT;
            switch (CpuLevel) {
                case 0:
                    cpuPerfLevel = XR_PERF_SETTINGS_LEVEL_POWER_SAVINGS_EXT;
                    break;
                case 1:
                    cpuPerfLevel = XR_PERF_SETTINGS_LEVEL_SUSTAINED_LOW_EXT;
                    break;
                case 2:
                    cpuPerfLevel = XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT;
                    break;
                case 3:
                    cpuPerfLevel = XR_PERF_SETTINGS_LEVEL_BOOST_EXT;
                    break;
                default:
                    ALOGE("Invalid CPU level %d", CpuLevel);
                    break;
            }

            XrPerfSettingsLevelEXT gpuPerfLevel = XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT;
            switch (GpuLevel) {
                case 0:
                    gpuPerfLevel = XR_PERF_SETTINGS_LEVEL_POWER_SAVINGS_EXT;
                    break;
                case 1:
                    gpuPerfLevel = XR_PERF_SETTINGS_LEVEL_SUSTAINED_LOW_EXT;
                    break;
                case 2:
                    gpuPerfLevel = XR_PERF_SETTINGS_LEVEL_SUSTAINED_HIGH_EXT;
                    break;
                case 3:
                    gpuPerfLevel = XR_PERF_SETTINGS_LEVEL_BOOST_EXT;
                    break;
                default:
                    ALOGE("Invalid GPU level %d", GpuLevel);
                    break;
            }

            PFN_xrPerfSettingsSetPerformanceLevelEXT pfnPerfSettingsSetPerformanceLevelEXT = NULL;
            OXR(xrGetInstanceProcAddr(
                Instance,
                "xrPerfSettingsSetPerformanceLevelEXT",
                (PFN_xrVoidFunction*)(&pfnPerfSettingsSetPerformanceLevelEXT)));

            OXR(pfnPerfSettingsSetPerformanceLevelEXT(
                Session, XR_PERF_SETTINGS_DOMAIN_CPU_EXT, cpuPerfLevel));
            OXR(pfnPerfSettingsSetPerformanceLevelEXT(
                Session, XR_PERF_SETTINGS_DOMAIN_GPU_EXT, gpuPerfLevel));

            PFN_xrSetAndroidApplicationThreadKHR pfnSetAndroidApplicationThreadKHR = NULL;
            OXR(xrGetInstanceProcAddr(
                Instance,
                "xrSetAndroidApplicationThreadKHR",
                (PFN_xrVoidFunction*)(&pfnSetAndroidApplicationThreadKHR)));

            OXR(pfnSetAndroidApplicationThreadKHR(
                Session, XR_ANDROID_THREAD_TYPE_APPLICATION_MAIN_KHR, MainThreadTid));
            OXR(pfnSetAndroidApplicationThreadKHR(
                Session, XR_ANDROID_THREAD_TYPE_RENDERER_MAIN_KHR, RenderThreadTid));
        }
#endif // defined(XR_USE_PLATFORM_ANDROID)
    } else if (state == XR_SESSION_STATE_STOPPING) {
#if defined(XR_USE_PLATFORM_ANDROID)
        assert(Resumed == false);
#endif // defined(XR_USE_PLATFORM_ANDROID)
        assert(SessionActive);
        OXR(xrEndSession(Session));
        SessionActive = false;
    }
}

void App::HandleXrEvents() {
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
            case XR_TYPE_EVENT_DATA_EVENTS_LOST:
                ALOGV("xrPollEvent: received XR_TYPE_EVENT_DATA_EVENTS_LOST event");
                break;
            case XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING:
                ALOGV("xrPollEvent: received XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING event");
                break;
            case XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED:
                ALOGV("xrPollEvent: received XR_TYPE_EVENT_DATA_INTERACTION_PROFILE_CHANGED event");
                break;
            case XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT: {
#if defined(XR_USE_PLATFORM_ANDROID)
                const XrEventDataPerfSettingsEXT* perf_settings_event =
                    (XrEventDataPerfSettingsEXT*)(baseEventHeader);
                ALOGV(
                    "xrPollEvent: received XR_TYPE_EVENT_DATA_PERF_SETTINGS_EXT event: type %d subdomain %d : level %d -> level %d",
                    perf_settings_event->type,
                    perf_settings_event->subDomain,
                    perf_settings_event->fromLevel,
                    perf_settings_event->toLevel);
#endif // defined(XR_USE_PLATFORM_ANDROID)
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
                    case XR_SESSION_STATE_FOCUSED:
                        Focused = true;
                        break;
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

#if defined(XR_USE_PLATFORM_ANDROID)
/*
================================================================================

Native Activity

================================================================================
*/

/**
 * Process the next main command.
 */
static void app_handle_cmd(struct android_app* androidApp, int32_t cmd) {
    App& app = *(App*)androidApp->userData;

    switch (cmd) {
        // There is no APP_CMD_CREATE. The ANativeActivity creates the
        // application thread from onCreate(). The application thread
        // then calls android_main().
        case APP_CMD_START: {
            ALOGV("onStart()");
            ALOGV("    APP_CMD_START");
            break;
        }
        case APP_CMD_RESUME: {
            ALOGV("onResume()");
            ALOGV("    APP_CMD_RESUME");
            app.Resumed = true;
            break;
        }
        case APP_CMD_PAUSE: {
            ALOGV("onPause()");
            ALOGV("    APP_CMD_PAUSE");
            app.Resumed = false;
            break;
        }
        case APP_CMD_STOP: {
            ALOGV("onStop()");
            ALOGV("    APP_CMD_STOP");
            break;
        }
        case APP_CMD_DESTROY: {
            ALOGV("onDestroy()");
            ALOGV("    APP_CMD_DESTROY");
            break;
        }
        case APP_CMD_INIT_WINDOW: {
            ALOGV("surfaceCreated()");
            ALOGV("    APP_CMD_INIT_WINDOW");
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            ALOGV("surfaceDestroyed()");
            ALOGV("    APP_CMD_TERM_WINDOW");
            break;
        }
    }
}

bool CheckUseScenePermission(JNIEnv* env, jobject activityObject);

#endif // defined(XR_USE_PLATFORM_ANDROID)

/**
 * This is the main entry point of a native application that is using
 * android_native_app_glue.  It runs in its own thread, with its own
 * event loop for receiving input events and doing other things.
 */
#if defined(XR_USE_PLATFORM_ANDROID)
void android_main(struct android_app* androidApp) {
#else
int main() {
#endif
#if defined(XR_USE_PLATFORM_ANDROID)
    ALOGV("----------------------------------------------------------------");
    ALOGV("android_app_entry()");
    ALOGV("    android_main()");

    JNIEnv* Env;
    (*androidApp->activity->vm).AttachCurrentThread(&Env, nullptr);

    // Note that AttachCurrentThread will reset the thread name.
    prctl(PR_SET_NAME, (long)"OVR::Main", 0, 0, 0);
#endif // defined(XR_USE_PLATFORM_ANDROID)

    App app;

#if defined(XR_USE_PLATFORM_ANDROID)
    PFN_xrInitializeLoaderKHR xrInitializeLoaderKHR;
    xrGetInstanceProcAddr(
        XR_NULL_HANDLE, "xrInitializeLoaderKHR", (PFN_xrVoidFunction*)&xrInitializeLoaderKHR);
    if (xrInitializeLoaderKHR != NULL) {
        XrLoaderInitInfoAndroidKHR loaderInitializeInfoAndroid = {
            XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};
        loaderInitializeInfoAndroid.applicationVM = androidApp->activity->vm;
        loaderInitializeInfoAndroid.applicationContext = androidApp->activity->clazz;
        xrInitializeLoaderKHR((XrLoaderInitInfoBaseHeaderKHR*)&loaderInitializeInfoAndroid);
    }
#endif // defined(XR_USE_PLATFORM_ANDROID)

    // Log available layers.
    {
        XrResult result;

        PFN_xrEnumerateApiLayerProperties xrEnumerateApiLayerProperties;
        OXR(result = xrGetInstanceProcAddr(
                XR_NULL_HANDLE,
                "xrEnumerateApiLayerProperties",
                (PFN_xrVoidFunction*)&xrEnumerateApiLayerProperties));
        if (result != XR_SUCCESS) {
            ALOGE("Failed to get xrEnumerateApiLayerProperties function pointer.");
            exit(1);
        }

        uint32_t layerCount = 0;
        OXR(xrEnumerateApiLayerProperties(0, &layerCount, NULL));
        std::vector<XrApiLayerProperties> layerProperties(
            layerCount, {XR_TYPE_API_LAYER_PROPERTIES});
        OXR(xrEnumerateApiLayerProperties(layerCount, &layerCount, layerProperties.data()));

        for (const auto& layer : layerProperties) {
            ALOGV("Found layer %s", layer.layerName);
        }
    }

    // Check that the extensions required are present.
    const char* const requiredExtensionNames[] = {
#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
        XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME,
#elif defined(XR_USE_GRAPHICS_API_OPENGL)
        XR_KHR_OPENGL_ENABLE_EXTENSION_NAME,
#endif
#if defined(XR_USE_PLATFORM_ANDROID)
        XR_EXT_PERFORMANCE_SETTINGS_EXTENSION_NAME,
        XR_KHR_ANDROID_THREAD_SETTINGS_EXTENSION_NAME,
#endif // defined(XR_USE_PLATFORM_ANDROID)
        XR_FB_PASSTHROUGH_EXTENSION_NAME,
        XR_META_ENVIRONMENT_DEPTH_EXTENSION_NAME
    };
    const uint32_t numRequiredExtensions =
        sizeof(requiredExtensionNames) / sizeof(requiredExtensionNames[0]);

    // Check the list of required extensions against what is supported by the runtime.
    {
        XrResult result;
        PFN_xrEnumerateInstanceExtensionProperties xrEnumerateInstanceExtensionProperties;
        OXR(result = xrGetInstanceProcAddr(
                XR_NULL_HANDLE,
                "xrEnumerateInstanceExtensionProperties",
                (PFN_xrVoidFunction*)&xrEnumerateInstanceExtensionProperties));
        if (result != XR_SUCCESS) {
            ALOGE("Failed to get xrEnumerateInstanceExtensionProperties function pointer.");
            exit(1);
        }

        uint32_t numInputExtensions = 0;
        uint32_t numOutputExtensions = 0;
        OXR(xrEnumerateInstanceExtensionProperties(
            NULL, numInputExtensions, &numOutputExtensions, NULL));
        ALOGV("xrEnumerateInstanceExtensionProperties found %u extension(s).", numOutputExtensions);

        numInputExtensions = numOutputExtensions;

        auto extensionProperties = new XrExtensionProperties[numOutputExtensions];

        for (uint32_t i = 0; i < numOutputExtensions; i++) {
            extensionProperties[i].type = XR_TYPE_EXTENSION_PROPERTIES;
            extensionProperties[i].next = NULL;
        }

        OXR(xrEnumerateInstanceExtensionProperties(
            NULL, numInputExtensions, &numOutputExtensions, extensionProperties));
        for (uint32_t i = 0; i < numOutputExtensions; i++) {
            ALOGV("Extension #%d = '%s'.", i, extensionProperties[i].extensionName);
        }

        for (uint32_t i = 0; i < numRequiredExtensions; i++) {
            bool found = false;
            for (uint32_t j = 0; j < numOutputExtensions; j++) {
                if (!strcmp(requiredExtensionNames[i], extensionProperties[j].extensionName)) {
                    ALOGV("Found required extension %s", requiredExtensionNames[i]);
                    found = true;
                    break;
                }
            }
            if (!found) {
                ALOGE("Failed to find required extension %s", requiredExtensionNames[i]);
                exit(1);
            }
        }

        delete[] extensionProperties;
    }

    // Create the OpenXR instance.
    XrApplicationInfo appInfo = {};
    strcpy(appInfo.applicationName, "XrPassthroughOcclusion");
    appInfo.applicationVersion = 0;
    strcpy(appInfo.engineName, "Oculus Mobile Sample");
    appInfo.engineVersion = 0;
    appInfo.apiVersion = XR_CURRENT_API_VERSION;

    XrInstanceCreateInfo instanceCreateInfo = {XR_TYPE_INSTANCE_CREATE_INFO};
    instanceCreateInfo.createFlags = 0;
    instanceCreateInfo.applicationInfo = appInfo;
    instanceCreateInfo.enabledApiLayerCount = 0;
    instanceCreateInfo.enabledApiLayerNames = NULL;
    instanceCreateInfo.enabledExtensionCount = numRequiredExtensions;
    instanceCreateInfo.enabledExtensionNames = requiredExtensionNames;

#if defined(XR_USE_PLATFORM_ANDROID)
    if (!CheckUseScenePermission(Env, androidApp->activity->clazz)) {
        ALOGW(
            "com.oculus.USE_SCENE permission should be requested before creation of OpenXR instance. "
            "Application will not function correctly without it.");
    } else {
        ALOGV("com.oculus.USE_SCENE permission WAS granted");
    }
#endif // defined(XR_USE_PLATFORM_ANDROID)

    XrResult initResult;
    OXR(initResult = xrCreateInstance(&instanceCreateInfo, &app.Instance));
    if (initResult != XR_SUCCESS) {
        ALOGE("Failed to create XR app.Instance: %d.", initResult);
        exit(1);
    }
    // Set the global used in macros
    instance = app.Instance;

    XrInstanceProperties instanceInfo = {XR_TYPE_INSTANCE_PROPERTIES};
    OXR(xrGetInstanceProperties(app.Instance, &instanceInfo));
    ALOGV(
        "Runtime %s: Version : %u.%u.%u",
        instanceInfo.runtimeName,
        XR_VERSION_MAJOR(instanceInfo.runtimeVersion),
        XR_VERSION_MINOR(instanceInfo.runtimeVersion),
        XR_VERSION_PATCH(instanceInfo.runtimeVersion));

    XrSystemGetInfo systemGetInfo = {XR_TYPE_SYSTEM_GET_INFO};
    systemGetInfo.formFactor = XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;

    XrSystemId systemId;
    OXR(initResult = xrGetSystem(app.Instance, &systemGetInfo, &systemId));
    if (initResult != XR_SUCCESS) {
        ALOGE("Failed to get system.");
        exit(1);
    }

    XrSystemProperties systemProperties = {XR_TYPE_SYSTEM_PROPERTIES};
    OXR(xrGetSystemProperties(app.Instance, systemId, &systemProperties));

    ALOGV(
        "System Properties: Name=%s VendorId=%x",
        systemProperties.systemName,
        systemProperties.vendorId);
    ALOGV(
        "System Graphics Properties: MaxWidth=%d MaxHeight=%d MaxLayers=%d",
        systemProperties.graphicsProperties.maxSwapchainImageWidth,
        systemProperties.graphicsProperties.maxSwapchainImageHeight,
        systemProperties.graphicsProperties.maxLayerCount);
    ALOGV(
        "System Tracking Properties: OrientationTracking=%s PositionTracking=%s",
        systemProperties.trackingProperties.orientationTracking ? "True" : "False",
        systemProperties.trackingProperties.positionTracking ? "True" : "False");

    assert(App::kMaxLayerCount <= systemProperties.graphicsProperties.maxLayerCount);

    // Get the graphics requirements.
#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
    PFN_xrGetOpenGLESGraphicsRequirementsKHR pfnGetOpenGLESGraphicsRequirementsKHR = NULL;
    OXR(xrGetInstanceProcAddr(
        app.Instance,
        "xrGetOpenGLESGraphicsRequirementsKHR",
        (PFN_xrVoidFunction*)(&pfnGetOpenGLESGraphicsRequirementsKHR)));

    XrGraphicsRequirementsOpenGLESKHR graphicsRequirements = {
        XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
    OXR(pfnGetOpenGLESGraphicsRequirementsKHR(app.Instance, systemId, &graphicsRequirements));
#elif defined(XR_USE_GRAPHICS_API_OPENGL)
    PFN_xrGetOpenGLGraphicsRequirementsKHR pfnGetOpenGLGraphicsRequirementsKHR = NULL;
    OXR(xrGetInstanceProcAddr(
        app.Instance,
        "xrGetOpenGLGraphicsRequirementsKHR",
        (PFN_xrVoidFunction*)(&pfnGetOpenGLGraphicsRequirementsKHR)));

    XrGraphicsRequirementsOpenGLKHR graphicsRequirements = {
        XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
    OXR(pfnGetOpenGLGraphicsRequirementsKHR(app.Instance, systemId, &graphicsRequirements));
#endif

    // Create the EGL Context
    app.egl.CreateContext(nullptr);

    // Check the graphics requirements.
    int eglMajor = 0;
    int eglMinor = 0;
    glGetIntegerv(GL_MAJOR_VERSION, &eglMajor);
    glGetIntegerv(GL_MINOR_VERSION, &eglMinor);
    const XrVersion eglVersion = XR_MAKE_VERSION(eglMajor, eglMinor, 0);
    if (eglVersion < graphicsRequirements.minApiVersionSupported ||
        eglVersion > graphicsRequirements.maxApiVersionSupported) {
        ALOGE("GLES version %d.%d not supported", eglMajor, eglMinor);
        exit(0);
    }

#if defined(ANDROID)
    app.MainThreadTid = gettid();
#else
    app.MainThreadTid = (int)std::hash<std::thread::id>{}(std::this_thread::get_id());
#endif

    app.SystemId = systemId;

    INIT_PFN(xrCreatePassthroughFB);
    INIT_PFN(xrDestroyPassthroughFB);
    INIT_PFN(xrPassthroughStartFB);
    INIT_PFN(xrPassthroughPauseFB);
    INIT_PFN(xrCreatePassthroughLayerFB);
    INIT_PFN(xrDestroyPassthroughLayerFB);
    INIT_PFN(xrPassthroughLayerSetStyleFB);
    INIT_PFN(xrPassthroughLayerPauseFB);
    INIT_PFN(xrPassthroughLayerResumeFB);

    INIT_PFN(xrCreateEnvironmentDepthProviderMETA);
    INIT_PFN(xrDestroyEnvironmentDepthProviderMETA);
    INIT_PFN(xrStartEnvironmentDepthProviderMETA);
    INIT_PFN(xrStopEnvironmentDepthProviderMETA);
    INIT_PFN(xrCreateEnvironmentDepthSwapchainMETA);
    INIT_PFN(xrGetEnvironmentDepthSwapchainStateMETA);
    INIT_PFN(xrDestroyEnvironmentDepthSwapchainMETA);
    INIT_PFN(xrEnumerateEnvironmentDepthSwapchainImagesMETA);
    INIT_PFN(xrAcquireEnvironmentDepthImageMETA);
    INIT_PFN(xrSetEnvironmentDepthHandRemovalMETA);

    // Create the OpenXR Session.
#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
    XrGraphicsBindingOpenGLESAndroidKHR graphicsBinding = {
        XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};
    graphicsBinding.display = app.egl.Display;
    graphicsBinding.config = app.egl.Config;
    graphicsBinding.context = app.egl.Context;
#elif defined(XR_USE_GRAPHICS_API_OPENGL)
    XrGraphicsBindingOpenGLWin32KHR graphicsBinding = {XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR};
    graphicsBinding.hDC = app.egl.hDC;
    graphicsBinding.hGLRC = app.egl.hGLRC;
#endif

    XrSessionCreateInfo sessionCreateInfo = {XR_TYPE_SESSION_CREATE_INFO};
    sessionCreateInfo.next = &graphicsBinding;
    sessionCreateInfo.createFlags = 0;
    sessionCreateInfo.systemId = app.SystemId;

    OXR(initResult = xrCreateSession(app.Instance, &sessionCreateInfo, &app.Session));
    if (initResult != XR_SUCCESS) {
        ALOGE("Failed to create XR session: %d.", initResult);
        exit(1);
    }

    // App only supports the primary stereo view config.
    const XrViewConfigurationType supportedViewConfigType =
        XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;

    // Enumerate the viewport configurations.
    uint32_t viewportConfigTypeCount = 0;
    OXR(xrEnumerateViewConfigurations(
        app.Instance, app.SystemId, 0, &viewportConfigTypeCount, NULL));

    auto viewportConfigurationTypes = new XrViewConfigurationType[viewportConfigTypeCount];

    OXR(xrEnumerateViewConfigurations(
        app.Instance,
        app.SystemId,
        viewportConfigTypeCount,
        &viewportConfigTypeCount,
        viewportConfigurationTypes));

    ALOGV("Available Viewport Configuration Types: %d", viewportConfigTypeCount);

    for (uint32_t i = 0; i < viewportConfigTypeCount; i++) {
        const XrViewConfigurationType viewportConfigType = viewportConfigurationTypes[i];

        ALOGV(
            "Viewport configuration type %d : %s",
            viewportConfigType,
            viewportConfigType == supportedViewConfigType ? "Selected" : "");

        XrViewConfigurationProperties viewportConfig = {XR_TYPE_VIEW_CONFIGURATION_PROPERTIES};
        OXR(xrGetViewConfigurationProperties(
            app.Instance, app.SystemId, viewportConfigType, &viewportConfig));
        ALOGV(
            "FovMutable=%s ConfigurationType %d",
            viewportConfig.fovMutable ? "true" : "false",
            viewportConfig.viewConfigurationType);

        uint32_t viewCount;
        OXR(xrEnumerateViewConfigurationViews(
            app.Instance, app.SystemId, viewportConfigType, 0, &viewCount, NULL));

        if (viewCount > 0) {
            auto elements = new XrViewConfigurationView[viewCount];

            for (uint32_t e = 0; e < viewCount; e++) {
                elements[e].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
                elements[e].next = NULL;
            }

            OXR(xrEnumerateViewConfigurationViews(
                app.Instance, app.SystemId, viewportConfigType, viewCount, &viewCount, elements));

            // Log the view config info for each view type for debugging purposes.
            for (uint32_t e = 0; e < viewCount; e++) {
                const XrViewConfigurationView* element = &elements[e];
                (void)element;

                ALOGV(
                    "Viewport [%d]: Recommended Width=%d Height=%d SampleCount=%d",
                    e,
                    element->recommendedImageRectWidth,
                    element->recommendedImageRectHeight,
                    element->recommendedSwapchainSampleCount);

                ALOGV(
                    "Viewport [%d]: Max Width=%d Height=%d SampleCount=%d",
                    e,
                    element->maxImageRectWidth,
                    element->maxImageRectHeight,
                    element->maxSwapchainSampleCount);
            }

            // Cache the view config properties for the selected config type.
            if (viewportConfigType == supportedViewConfigType) {
                assert(viewCount == kNumEyes);
                for (uint32_t e = 0; e < viewCount; e++) {
                    app.ViewConfigurationView[e] = elements[e];
                }
            }

            delete[] elements;
        } else {
            ALOGE("Empty viewport configuration type: %d", viewCount);
        }
    }

    delete[] viewportConfigurationTypes;

    // Get the viewport configuration info for the chosen viewport configuration type.
    app.ViewportConfig.type = XR_TYPE_VIEW_CONFIGURATION_PROPERTIES;

    OXR(xrGetViewConfigurationProperties(
        app.Instance, app.SystemId, supportedViewConfigType, &app.ViewportConfig));

    // Create a space to the first path
    XrReferenceSpaceCreateInfo spaceCreateInfo = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
    spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_VIEW;
    spaceCreateInfo.poseInReferenceSpace.orientation.w = 1.0f;
    OXR(xrCreateReferenceSpace(app.Session, &spaceCreateInfo, &app.HeadSpace));

    spaceCreateInfo.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
    OXR(xrCreateReferenceSpace(app.Session, &spaceCreateInfo, &app.LocalSpace));

    XrView projections[kNumEyes];
    for (int eye = 0; eye < kNumEyes; eye++) {
        projections[eye] = XrView{XR_TYPE_VIEW};
    }

#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
    using SwapchainImageType = XrSwapchainImageOpenGLESKHR;
    static constexpr auto kSwapChainImageType = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR;
#elif defined(XR_USE_GRAPHICS_API_OPENGL)
    using SwapchainImageType = XrSwapchainImageOpenGLKHR;
    static constexpr auto kSwapChainImageType = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR;
#endif

    GLenum format = GL_SRGB8_ALPHA8;
    int width = app.ViewConfigurationView[0].recommendedImageRectWidth;
    int height = app.ViewConfigurationView[0].recommendedImageRectHeight;

    XrSwapchainCreateInfo swapChainCreateInfo = {XR_TYPE_SWAPCHAIN_CREATE_INFO};
    swapChainCreateInfo.usageFlags =
        XR_SWAPCHAIN_USAGE_SAMPLED_BIT | XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT;
    swapChainCreateInfo.format = format;
    swapChainCreateInfo.sampleCount = 1;
    swapChainCreateInfo.width = width;
    swapChainCreateInfo.height = height;
    swapChainCreateInfo.faceCount = 1;
    swapChainCreateInfo.arraySize = 2;
    swapChainCreateInfo.mipCount = 1;

    OXR(xrCreateSwapchain(app.Session, &swapChainCreateInfo, &app.ColorSwapchain));
    OXR(xrEnumerateSwapchainImages(app.ColorSwapchain, 0, &app.SwapchainLength, nullptr));

    std::vector<SwapchainImageType> colorImages(app.SwapchainLength);
    for (uint32_t i = 0; i < app.SwapchainLength; ++i) {
        colorImages[i] = {kSwapChainImageType};
    }

    OXR(xrEnumerateSwapchainImages(
        app.ColorSwapchain,
        app.SwapchainLength,
        &app.SwapchainLength,
        (XrSwapchainImageBaseHeader*)colorImages.data()));

    std::vector<GLuint> colorTextures(app.SwapchainLength);
    for (uint32_t i = 0; i < app.SwapchainLength; ++i) {
        colorTextures[i] = GLuint(colorImages[i].image);
    }

    app.appRenderer.Create(
        format, width, height, kNumMultiSamples, app.SwapchainLength, colorTextures.data());

    AppInput_init(app);

    // Create passthrough objects
    XrPassthroughFB passthrough = XR_NULL_HANDLE;
    XrPassthroughLayerFB passthroughLayer = XR_NULL_HANDLE;
    {
        XrPassthroughCreateInfoFB ptci = {XR_TYPE_PASSTHROUGH_CREATE_INFO_FB};
        XrResult result;
        OXR(result = xrCreatePassthroughFB(app.Session, &ptci, &passthrough));

        if (XR_SUCCEEDED(result)) {
            XrPassthroughLayerCreateInfoFB plci = {XR_TYPE_PASSTHROUGH_LAYER_CREATE_INFO_FB};
            plci.passthrough = passthrough;
            plci.purpose = XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB;
            OXR(xrCreatePassthroughLayerFB(app.Session, &plci, &passthroughLayer));
        }
    }

    OXR(xrPassthroughStartFB(passthrough));
    OXR(xrPassthroughLayerResumeFB(passthroughLayer));

    // Create the environment depth provider.
    const XrEnvironmentDepthProviderCreateInfoMETA environmentDepthProviderCreateInfo{
        /*type=*/XR_TYPE_ENVIRONMENT_DEPTH_PROVIDER_CREATE_INFO_META,
        /*next=*/nullptr,
        /*createFlags=*/0};
    OXR(xrCreateEnvironmentDepthProviderMETA(
        app.Session, &environmentDepthProviderCreateInfo, &app.EnvironmentDepthProvider));

    // Create the depth swapchain.
    XrEnvironmentDepthSwapchainCreateInfoMETA environmentDepthSwapchainCreateInfo{
        XR_TYPE_ENVIRONMENT_DEPTH_SWAPCHAIN_CREATE_INFO_META};

    OXR(xrCreateEnvironmentDepthSwapchainMETA(
        app.EnvironmentDepthProvider,
        &environmentDepthSwapchainCreateInfo,
        &app.EnvironmentDepthSwapchain));

    XrEnvironmentDepthSwapchainStateMETA environmentDepthSwapchainState{
        XR_TYPE_ENVIRONMENT_DEPTH_SWAPCHAIN_STATE_META};
    OXR(xrGetEnvironmentDepthSwapchainStateMETA(
        app.EnvironmentDepthSwapchain, &environmentDepthSwapchainState));
    uint32_t environmentDepthSwapChainLength = 0;
    OXR(xrEnumerateEnvironmentDepthSwapchainImagesMETA(
        app.EnvironmentDepthSwapchain, 0, &environmentDepthSwapChainLength, nullptr));

    // Populate the swapchain image array.
    std::vector<SwapchainImageType> environmentDepthImages(environmentDepthSwapChainLength);
    for (uint32_t i = 0; i < environmentDepthSwapChainLength; ++i) {
        environmentDepthImages[i] = {kSwapChainImageType};
    }

    OXR(xrEnumerateEnvironmentDepthSwapchainImagesMETA(
        app.EnvironmentDepthSwapchain,
        environmentDepthSwapChainLength,
        &environmentDepthSwapChainLength,
        (XrSwapchainImageBaseHeader*)environmentDepthImages.data()));

    std::vector<GLuint> environmentDepthTextures(environmentDepthSwapChainLength);
    for (uint32_t i = 0; i < environmentDepthSwapChainLength; ++i) {
        environmentDepthTextures[i] = GLuint(environmentDepthImages[i].image);
    }

    OXR(xrStartEnvironmentDepthProviderMETA(app.EnvironmentDepthProvider));

#if defined(XR_USE_PLATFORM_ANDROID)
    androidApp->userData = &app;
    androidApp->onAppCmd = app_handle_cmd;
#endif // defined(XR_USE_PLATFORM_ANDROID)

#if defined(XR_USE_PLATFORM_ANDROID)
    while (androidApp->destroyRequested == 0)
#else
    while (true)
#endif
    {
#if defined(XR_USE_PLATFORM_ANDROID)
        // Read all pending events.
        for (;;) {
            int events;
            struct android_poll_source* source;
            // If the timeout is zero, returns immediately without blocking.
            // If the timeout is negative, waits indefinitely until an event appears.
            const int timeoutMilliseconds = (app.Resumed == false && app.SessionActive == false &&
                                             androidApp->destroyRequested == 0)
                ? -1
                : 0;
            if (ALooper_pollAll(timeoutMilliseconds, NULL, &events, (void**)&source) < 0) {
                break;
            }

            // Process this event.
            if (source != NULL) {
                source->process(androidApp, source);
            }
        }
#elif defined(XR_USE_PLATFORM_WIN32)
        MSG msg;
        while (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) > 0) {
            if (msg.message == WM_QUIT) {
                app.ShouldExit = true;
            } else {
                ::TranslateMessage(&msg);
                ::DispatchMessage(&msg);
            }
        }
#endif // defined(XR_USE_PLATFORM_ANDROID)

        app.HandleXrEvents();

        if (app.ShouldExit) {
            break;
        }

        AppInput_syncActions(app);

        // Create the scene if not yet created.
        // The scene is created here to be able to show a loading icon.
        if (!app.appRenderer.scene.IsCreated()) {
            // Create the scene.
            app.appRenderer.scene.Create();
        }

        // xrWaitFrame() is going to fail if session is not active
        if (!app.SessionActive) {
            continue;
        }

        // NOTE: OpenXR does not use the concept of frame indices. Instead,
        // XrWaitFrame returns the predicted display time.
        XrFrameWaitInfo waitFrameInfo = {XR_TYPE_FRAME_WAIT_INFO};

        XrFrameState frameState = {XR_TYPE_FRAME_STATE};

        OXR(xrWaitFrame(app.Session, &waitFrameInfo, &frameState));

        // Get the HMD pose, predicted for the middle of the time period during which
        // the new eye images will be displayed. The number of frames predicted ahead
        // depends on the pipeline depth of the engine and the synthesis rate.
        // The better the prediction, the less black will be pulled in at the edges.
        XrFrameBeginInfo beginFrameDesc = {XR_TYPE_FRAME_BEGIN_INFO};
        OXR(xrBeginFrame(app.Session, &beginFrameDesc));

        XrPosef xfLocalFromHead;
        {
            XrSpaceLocation loc = {XR_TYPE_SPACE_LOCATION};
            OXR(xrLocateSpace(
                app.HeadSpace, app.LocalSpace, frameState.predictedDisplayTime, &loc));
            xfLocalFromHead = loc.pose;
        }

        XrViewState viewState = {XR_TYPE_VIEW_STATE};

        XrViewLocateInfo projectionInfo = {XR_TYPE_VIEW_LOCATE_INFO};
        projectionInfo.viewConfigurationType = app.ViewportConfig.viewConfigurationType;
        projectionInfo.displayTime = frameState.predictedDisplayTime;
        projectionInfo.space = app.HeadSpace;

        uint32_t projectionCapacityInput = kNumEyes;
        uint32_t projectionCountOutput = projectionCapacityInput;

        OXR(xrLocateViews(
            app.Session,
            &projectionInfo,
            &viewState,
            projectionCapacityInput,
            &projectionCountOutput,
            projections));

        // update input information
        std::vector<XrSpace> controllerSpaces;
        if (leftControllerActive) {
            controllerSpaces.push_back(leftControllerAimSpace);
        }
        if (rightControllerActive) {
            controllerSpaces.push_back(rightControllerAimSpace);
        }
        app.appRenderer.scene.TrackedControllers.clear();
        for (const XrSpace controllerSpace : controllerSpaces) {
            XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
            OXR(xrLocateSpace(
                controllerSpace, app.LocalSpace, frameState.predictedDisplayTime, &loc));
            app.appRenderer.scene.TrackedControllers.push_back({});
            app.appRenderer.scene.TrackedControllers.back().Pose = OvrFromXr(loc.pose);
        }

        AppRenderer::FrameIn frameIn;
        uint32_t chainIndex = 0;
        XrSwapchainImageAcquireInfo acquireInfo = {XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO, NULL};
        OXR(xrAcquireSwapchainImage(app.ColorSwapchain, &acquireInfo, &chainIndex));

        frameIn.SwapChainIndex = int(chainIndex);
        frameIn.ScreenNearZ = kProjectionNearZ;
        frameIn.ScreenFarZ = kProjectionFarZ;

        XrPosef xfLocalFromEye[kNumEyes];

        for (int eye = 0; eye < kNumEyes; eye++) {
            XrPosef xfHeadFromEye = projections[eye].pose;
            XrPosef_Multiply(&xfLocalFromEye[eye], &xfLocalFromHead, &xfHeadFromEye);

            XrPosef xfEyeFromLocal = XrPosef_Inverse(xfLocalFromEye[eye]);

            XrMatrix4x4f viewMat{};
            XrMatrix4x4f_CreateFromRigidTransform(&viewMat, &xfEyeFromLocal);

            const XrFovf fov = projections[eye].fov;
            XrMatrix4x4f projMat;
            XrMatrix4x4f_CreateProjectionFov(
                &projMat, GRAPHICS_OPENGL_ES, fov, kProjectionNearZ, kProjectionFarZ);

            frameIn.View[eye] = OvrFromXr(viewMat);
            frameIn.Proj[eye] = OvrFromXr(projMat);
        }

        XrSwapchainImageWaitInfo waitInfo = {XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};
        waitInfo.timeout = 1000000000; /* timeout in nanoseconds */
        XrResult res = xrWaitSwapchainImage(app.ColorSwapchain, &waitInfo);
        int retry = 0;
        while (res == XR_TIMEOUT_EXPIRED) {
            res = xrWaitSwapchainImage(app.ColorSwapchain, &waitInfo);
            retry++;
            ALOGV(
                " Retry xrWaitSwapchainImage %d times due to XR_TIMEOUT_EXPIRED (duration %f seconds)",
                retry,
                waitInfo.timeout * (1E-9));
        }

        XrEnvironmentDepthImageAcquireInfoMETA environmentDepthAcquireInfo{
            XR_TYPE_ENVIRONMENT_DEPTH_IMAGE_ACQUIRE_INFO_META};
        environmentDepthAcquireInfo.space = app.LocalSpace;
        environmentDepthAcquireInfo.displayTime = frameState.predictedDisplayTime;
        XrEnvironmentDepthImageMETA environmentDepthImage{XR_TYPE_ENVIRONMENT_DEPTH_IMAGE_META};
        environmentDepthImage.views[0].type = XR_TYPE_ENVIRONMENT_DEPTH_IMAGE_VIEW_META;
        environmentDepthImage.views[1].type = XR_TYPE_ENVIRONMENT_DEPTH_IMAGE_VIEW_META;

        const XrResult acquireResult = xrAcquireEnvironmentDepthImageMETA(
            app.EnvironmentDepthProvider, &environmentDepthAcquireInfo, &environmentDepthImage);
        if (acquireResult == XR_SUCCESS) {
            ALOGE(
                "Received depth frame at swapchain idx %d near = %f far = %f     w = %d  h = %d",
                environmentDepthImage.swapchainIndex,
                environmentDepthImage.nearZ,
                environmentDepthImage.farZ,
                width,
                height);

            frameIn.HasDepth = true;
            frameIn.DepthTexture =
                environmentDepthTextures.at(environmentDepthImage.swapchainIndex);
            frameIn.DepthNearZ = environmentDepthImage.nearZ;
            frameIn.DepthFarZ = environmentDepthImage.farZ;

            for (int eye = 0; eye < kNumEyes; ++eye) {
                // TODO(T163815090): Use the actual returned pose once verified correct.
                // const XrPosef xfLocalFromDepthEye = environmentDepthImage.views[eye].pose;
                const XrPosef xfLocalFromDepthEye = xfLocalFromEye[eye];
                frameIn.T_DepthCoord_ScreenCoord[eye] = MakeDestFromSourceMapping(
                    /*destSize=*/OVR::Vector2f(1.0f, 1.0f),
                    /*destFov=*/environmentDepthImage.views[eye].fov,
                    /*xfLocalFromDestCamera=*/xfLocalFromDepthEye,
                    /*sourceSize=*/OVR::Vector2f(width, height),
                    /*sourceFov=*/projections[eye].fov,
                    /*xfLocalFromSourceCamera=*/xfLocalFromEye[eye]);
            }
        } else {
            ALOGE("No depth image received. Result = %d", acquireResult);
            frameIn.HasDepth = false;
        }

        app.appRenderer.RenderFrame(frameIn);

        XrSwapchainImageReleaseInfo releaseInfo = {XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO, NULL};
        OXR(xrReleaseSwapchainImage(app.ColorSwapchain, &releaseInfo));

        // Set-up the compositor layers for this frame.
        // NOTE: Multiple independent layers are allowed, but they need to be added
        // in a depth consistent order.

        XrCompositionLayerProjectionView proj_views[2] = {};

        app.LayerCount = 0;
        memset(app.Layers, 0, sizeof(CompositionLayerUnion) * App::kMaxLayerCount);

        // passthrough layer is backmost layer (if available)
        if (passthroughLayer != XR_NULL_HANDLE) {
            XrCompositionLayerPassthroughFB passthrough_layer = {
                XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB};
            passthrough_layer.layerHandle = passthroughLayer;
            passthrough_layer.flags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
            passthrough_layer.space = XR_NULL_HANDLE;

            app.Layers[app.LayerCount].Passthrough = passthrough_layer;
            app.LayerCount++;
        }

        XrCompositionLayerProjection proj_layer = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
        proj_layer.layerFlags = XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT;
        proj_layer.layerFlags |= XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT;
        proj_layer.layerFlags |= XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
        proj_layer.space = app.LocalSpace;
        proj_layer.viewCount = kNumEyes;
        proj_layer.views = proj_views;

        for (int eye = 0; eye < kNumEyes; eye++) {
            XrCompositionLayerProjectionView& proj_view = proj_views[eye];
            proj_view = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
            proj_view.pose = xfLocalFromEye[eye];
            proj_view.fov = projections[eye].fov;

            proj_view.subImage.swapchain = app.ColorSwapchain;
            proj_view.subImage.imageRect.offset.x = 0;
            proj_view.subImage.imageRect.offset.y = 0;
            proj_view.subImage.imageRect.extent.width = width;
            proj_view.subImage.imageRect.extent.height = height;
            proj_view.subImage.imageArrayIndex = eye;
        }

        app.Layers[app.LayerCount++].Projection = proj_layer;

        // Compose the layers for this frame.
        const XrCompositionLayerBaseHeader* layers[App::kMaxLayerCount] = {};
        for (int i = 0; i < app.LayerCount; i++) {
            layers[i] = (const XrCompositionLayerBaseHeader*)&app.Layers[i];
        }

        XrFrameEndInfo endFrameInfo = {XR_TYPE_FRAME_END_INFO};
        endFrameInfo.displayTime = frameState.predictedDisplayTime;
        endFrameInfo.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        endFrameInfo.layerCount = app.LayerCount;
        endFrameInfo.layers = layers;

        OXR(xrEndFrame(app.Session, &endFrameInfo));
    }

    OXR(xrStopEnvironmentDepthProviderMETA(app.EnvironmentDepthProvider));
    OXR(xrDestroyEnvironmentDepthProviderMETA(app.EnvironmentDepthProvider));

    OXR(xrPassthroughPauseFB(passthrough));
    OXR(xrDestroyPassthroughFB(passthrough));

    app.appRenderer.Destroy();

    AppInput_shutdown();

    OXR(xrDestroySwapchain(app.ColorSwapchain));
    OXR(xrDestroyEnvironmentDepthSwapchainMETA(app.EnvironmentDepthSwapchain));
    OXR(xrDestroySpace(app.HeadSpace));
    OXR(xrDestroySpace(app.LocalSpace));
    OXR(xrDestroySession(app.Session));
    OXR(xrDestroyInstance(app.Instance));

    app.egl.DestroyContext();

#if defined(XR_USE_PLATFORM_ANDROID)
    (*androidApp->activity->vm).DetachCurrentThread();
#endif // defined(XR_USE_PLATFORM_ANDROID)
}

#if defined(XR_USE_PLATFORM_ANDROID)

template <typename Result>
Result JNI_CheckResult(Result result, const char* function, JNIEnv* env) {
    if constexpr (std::is_pointer<Result>::value) {
        if (result == nullptr) {
            ALOGE("JNI function failed %s", function);
            abort();
        }
    }
    if (env->ExceptionCheck() == JNI_TRUE) {
        ALOGE("JNI function caused a java exception %s", function);
        abort();
    }
    return result;
}

bool CheckUseScenePermission(JNIEnv* env, jobject activityObject) {
#define JNI_CHECK_RESULT(func) JNI_CheckResult(func, #func, env);
    jstring strPermission = JNI_CHECK_RESULT(env->NewStringUTF("com.oculus.permission.USE_SCENE"));
    jclass clsActivity = JNI_CHECK_RESULT(env->FindClass("android/app/Activity"));
    jmethodID methodCheckSelfPermission = JNI_CHECK_RESULT(
        env->GetMethodID(clsActivity, "checkSelfPermission", "(Ljava/lang/String;)I"));
    jint intPermissionResult = JNI_CHECK_RESULT(
        env->CallIntMethod(activityObject, methodCheckSelfPermission, strPermission));
    jclass clsPackageManager =
        JNI_CHECK_RESULT(env->FindClass("android/content/pm/PackageManager"));
    jfieldID fidPermissionGranted =
        JNI_CHECK_RESULT(env->GetStaticFieldID(clsPackageManager, "PERMISSION_GRANTED", "I"));
    jint intPermissionGranted =
        JNI_CHECK_RESULT(env->GetStaticIntField(clsPackageManager, fidPermissionGranted));
    env->DeleteLocalRef(strPermission);
    return intPermissionResult == intPermissionGranted;
#undef JNI_CHECK_RESULT
}

#endif // defined(XR_USE_PLATFORM_ANDROID)
