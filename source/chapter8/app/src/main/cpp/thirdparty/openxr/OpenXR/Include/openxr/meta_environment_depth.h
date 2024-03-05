#ifndef META_ENVIRONMENT_DEPTH_H_
#define META_ENVIRONMENT_DEPTH_H_ 1

/**********************
This file is @generated from the OpenXR XML API registry.
Language    :   C99
Copyright   :   (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.
***********************/

#include <openxr/openxr.h>

#ifdef __cplusplus
extern "C" {
#endif


#ifndef XR_META_environment_depth

#define XR_META_environment_depth 1
XR_DEFINE_HANDLE(XrEnvironmentDepthProviderMETA)
XR_DEFINE_HANDLE(XrEnvironmentDepthSwapchainMETA)
#define XR_META_environment_depth_SPEC_VERSION 1
#define XR_META_ENVIRONMENT_DEPTH_EXTENSION_NAME "XR_META_environment_depth"
// XrEnvironmentDepthProviderMETA
static const XrObjectType XR_OBJECT_TYPE_ENVIRONMENT_DEPTH_PROVIDER_META = (XrObjectType) 1000291000;
// XrEnvironmentDepthSwapchainMETA
static const XrObjectType XR_OBJECT_TYPE_ENVIRONMENT_DEPTH_SWAPCHAIN_META = (XrObjectType) 1000291001;
static const XrStructureType XR_TYPE_ENVIRONMENT_DEPTH_PROVIDER_CREATE_INFO_META = (XrStructureType) 1000291000;
static const XrStructureType XR_TYPE_ENVIRONMENT_DEPTH_SWAPCHAIN_CREATE_INFO_META = (XrStructureType) 1000291001;
static const XrStructureType XR_TYPE_ENVIRONMENT_DEPTH_SWAPCHAIN_STATE_META = (XrStructureType) 1000291002;
static const XrStructureType XR_TYPE_ENVIRONMENT_DEPTH_IMAGE_ACQUIRE_INFO_META = (XrStructureType) 1000291003;
static const XrStructureType XR_TYPE_ENVIRONMENT_DEPTH_IMAGE_VIEW_META = (XrStructureType) 1000291004;
static const XrStructureType XR_TYPE_ENVIRONMENT_DEPTH_IMAGE_META = (XrStructureType) 1000291005;
static const XrStructureType XR_TYPE_ENVIRONMENT_DEPTH_HAND_REMOVAL_SET_INFO_META = (XrStructureType) 1000291006;
static const XrStructureType XR_TYPE_SYSTEM_ENVIRONMENT_DEPTH_PROPERTIES_META = (XrStructureType) 1000291007;
// Warning: The requested depth image is not yet available.
static const XrResult XR_ENVIRONMENT_DEPTH_NOT_AVAILABLE_META = (XrResult) 1000291000;
typedef XrFlags64 XrEnvironmentDepthProviderCreateFlagsMETA;

// Flag bits for XrEnvironmentDepthProviderCreateFlagsMETA

typedef XrFlags64 XrEnvironmentDepthSwapchainCreateFlagsMETA;

// Flag bits for XrEnvironmentDepthSwapchainCreateFlagsMETA

typedef struct XrEnvironmentDepthProviderCreateInfoMETA {
    XrStructureType                              type;
    const void* XR_MAY_ALIAS                     next;
    XrEnvironmentDepthProviderCreateFlagsMETA    createFlags;
} XrEnvironmentDepthProviderCreateInfoMETA;

typedef struct XrEnvironmentDepthSwapchainCreateInfoMETA {
    XrStructureType                               type;
    const void* XR_MAY_ALIAS                      next;
    XrEnvironmentDepthSwapchainCreateFlagsMETA    createFlags;
} XrEnvironmentDepthSwapchainCreateInfoMETA;

typedef struct XrEnvironmentDepthSwapchainStateMETA {
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
    uint32_t              width;
    uint32_t              height;
} XrEnvironmentDepthSwapchainStateMETA;

typedef struct XrEnvironmentDepthImageAcquireInfoMETA {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrSpace                     space;
    XrTime                      displayTime;
} XrEnvironmentDepthImageAcquireInfoMETA;

typedef struct XrEnvironmentDepthImageViewMETA {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrFovf                      fov;
    XrPosef                     pose;
} XrEnvironmentDepthImageViewMETA;

typedef struct XrEnvironmentDepthImageMETA {
    XrStructureType                    type;
    const void* XR_MAY_ALIAS           next;
    uint32_t                           swapchainIndex;
    float                              nearZ;
    float                              farZ;
    XrEnvironmentDepthImageViewMETA    views[2];
} XrEnvironmentDepthImageMETA;

typedef struct XrEnvironmentDepthHandRemovalSetInfoMETA {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrBool32                    enabled;
} XrEnvironmentDepthHandRemovalSetInfoMETA;

typedef struct XrSystemEnvironmentDepthPropertiesMETA {
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
    XrBool32              supportsEnvironmentDepth;
    XrBool32              supportsHandRemoval;
} XrSystemEnvironmentDepthPropertiesMETA;

typedef XrResult (XRAPI_PTR *PFN_xrCreateEnvironmentDepthProviderMETA)(XrSession session, const XrEnvironmentDepthProviderCreateInfoMETA* createInfo, XrEnvironmentDepthProviderMETA* environmentDepthProvider);
typedef XrResult (XRAPI_PTR *PFN_xrDestroyEnvironmentDepthProviderMETA)(XrEnvironmentDepthProviderMETA environmentDepthProvider);
typedef XrResult (XRAPI_PTR *PFN_xrStartEnvironmentDepthProviderMETA)(XrEnvironmentDepthProviderMETA environmentDepthProvider);
typedef XrResult (XRAPI_PTR *PFN_xrStopEnvironmentDepthProviderMETA)(XrEnvironmentDepthProviderMETA environmentDepthProvider);
typedef XrResult (XRAPI_PTR *PFN_xrCreateEnvironmentDepthSwapchainMETA)(XrEnvironmentDepthProviderMETA environmentDepthProvider, const XrEnvironmentDepthSwapchainCreateInfoMETA* createInfo, XrEnvironmentDepthSwapchainMETA* swapchain);
typedef XrResult (XRAPI_PTR *PFN_xrDestroyEnvironmentDepthSwapchainMETA)(XrEnvironmentDepthSwapchainMETA swapchain);
typedef XrResult (XRAPI_PTR *PFN_xrEnumerateEnvironmentDepthSwapchainImagesMETA)(XrEnvironmentDepthSwapchainMETA swapchain, uint32_t imageCapacityInput, uint32_t* imageCountOutput, XrSwapchainImageBaseHeader* images);
typedef XrResult (XRAPI_PTR *PFN_xrGetEnvironmentDepthSwapchainStateMETA)(XrEnvironmentDepthSwapchainMETA swapchain, XrEnvironmentDepthSwapchainStateMETA* state);
typedef XrResult (XRAPI_PTR *PFN_xrAcquireEnvironmentDepthImageMETA)(XrEnvironmentDepthProviderMETA environmentDepthProvider, const XrEnvironmentDepthImageAcquireInfoMETA* acquireInfo, XrEnvironmentDepthImageMETA* environmentDepthImage);
typedef XrResult (XRAPI_PTR *PFN_xrSetEnvironmentDepthHandRemovalMETA)(XrEnvironmentDepthProviderMETA environmentDepthProvider, const XrEnvironmentDepthHandRemovalSetInfoMETA* setInfo);

#ifndef XR_NO_PROTOTYPES
#ifdef XR_EXTENSION_PROTOTYPES
XRAPI_ATTR XrResult XRAPI_CALL xrCreateEnvironmentDepthProviderMETA(
    XrSession                                   session,
    const XrEnvironmentDepthProviderCreateInfoMETA* createInfo,
    XrEnvironmentDepthProviderMETA*             environmentDepthProvider);

XRAPI_ATTR XrResult XRAPI_CALL xrDestroyEnvironmentDepthProviderMETA(
    XrEnvironmentDepthProviderMETA              environmentDepthProvider);

XRAPI_ATTR XrResult XRAPI_CALL xrStartEnvironmentDepthProviderMETA(
    XrEnvironmentDepthProviderMETA              environmentDepthProvider);

XRAPI_ATTR XrResult XRAPI_CALL xrStopEnvironmentDepthProviderMETA(
    XrEnvironmentDepthProviderMETA              environmentDepthProvider);

XRAPI_ATTR XrResult XRAPI_CALL xrCreateEnvironmentDepthSwapchainMETA(
    XrEnvironmentDepthProviderMETA              environmentDepthProvider,
    const XrEnvironmentDepthSwapchainCreateInfoMETA* createInfo,
    XrEnvironmentDepthSwapchainMETA*            swapchain);

XRAPI_ATTR XrResult XRAPI_CALL xrDestroyEnvironmentDepthSwapchainMETA(
    XrEnvironmentDepthSwapchainMETA             swapchain);

XRAPI_ATTR XrResult XRAPI_CALL xrEnumerateEnvironmentDepthSwapchainImagesMETA(
    XrEnvironmentDepthSwapchainMETA             swapchain,
    uint32_t                                    imageCapacityInput,
    uint32_t*                                   imageCountOutput,
    XrSwapchainImageBaseHeader*                 images);

XRAPI_ATTR XrResult XRAPI_CALL xrGetEnvironmentDepthSwapchainStateMETA(
    XrEnvironmentDepthSwapchainMETA             swapchain,
    XrEnvironmentDepthSwapchainStateMETA*       state);

XRAPI_ATTR XrResult XRAPI_CALL xrAcquireEnvironmentDepthImageMETA(
    XrEnvironmentDepthProviderMETA              environmentDepthProvider,
    const XrEnvironmentDepthImageAcquireInfoMETA* acquireInfo,
    XrEnvironmentDepthImageMETA*                environmentDepthImage);

XRAPI_ATTR XrResult XRAPI_CALL xrSetEnvironmentDepthHandRemovalMETA(
    XrEnvironmentDepthProviderMETA              environmentDepthProvider,
    const XrEnvironmentDepthHandRemovalSetInfoMETA* setInfo);
#endif /* XR_EXTENSION_PROTOTYPES */
#endif /* !XR_NO_PROTOTYPES */
#endif /* XR_META_environment_depth */

#ifdef __cplusplus
}
#endif

#endif
