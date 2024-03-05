#ifndef METAX2_ENVIRONMENT_DEPTH_H_
#define METAX2_ENVIRONMENT_DEPTH_H_ 1

/**********************
This file is @generated from the OpenXR XML API registry.
Language    :   C99
Copyright   :   (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.
***********************/

#include <openxr/openxr.h>

#ifdef __cplusplus
extern "C" {
#endif


#ifndef XR_METAX2_environment_depth

#define XR_METAX2_environment_depth 1
XR_DEFINE_HANDLE(XrEnvironmentDepthProviderMETAX2)
#define XR_METAX2_environment_depth_SPEC_VERSION 1
#define XR_METAX2_ENVIRONMENT_DEPTH_EXTENSION_NAME "XR_METAX2_environment_depth"
// XrEnvironmentDepthProviderMETAX2
static const XrObjectType XR_OBJECT_TYPE_ENVIRONMENT_DEPTH_PROVIDER_METAX2 = (XrObjectType) 1000262000;
static const XrStructureType XR_TYPE_ENVIRONMENT_DEPTH_PROVIDER_CREATE_INFO_METAX2 = (XrStructureType) 1000262000;
static const XrStructureType XR_TYPE_ENVIRONMENT_DEPTH_SWAPCHAIN_CREATE_INFO_METAX2 = (XrStructureType) 1000262001;
static const XrStructureType XR_TYPE_ENVIRONMENT_DEPTH_SWAPCHAIN_PROPERTIES_METAX2 = (XrStructureType) 1000262002;
static const XrStructureType XR_TYPE_ENVIRONMENT_DEPTH_IMAGE_ACQUIRE_INFO_METAX2 = (XrStructureType) 1000262003;
static const XrStructureType XR_TYPE_ENVIRONMENT_DEPTH_IMAGE_VIEW_METAX2 = (XrStructureType) 1000262004;
static const XrStructureType XR_TYPE_ENVIRONMENT_DEPTH_IMAGE_METAX2 = (XrStructureType) 1000262005;
static const XrStructureType XR_TYPE_ENVIRONMENT_DEPTH_HAND_REMOVAL_SET_INFO_METAX2 = (XrStructureType) 1000262006;
static const XrStructureType XR_TYPE_SYSTEM_ENVIRONMENT_DEPTH_PROPERTIES_METAX2 = (XrStructureType) 1000262007;
// Warning: The requested depth image is not yet available.
static const XrResult XR_ENVIRONMENT_DEPTH_NOT_AVAILABLE_METAX2 = (XrResult) 1000262000;
typedef XrFlags64 XrEnvironmentDepthProviderCreateFlagsMETAX2;

// Flag bits for XrEnvironmentDepthProviderCreateFlagsMETAX2

typedef XrFlags64 XrEnvironmentDepthSwapchainCreateFlagsMETAX2;

// Flag bits for XrEnvironmentDepthSwapchainCreateFlagsMETAX2

typedef struct XrEnvironmentDepthProviderCreateInfoMETAX2 {
    XrStructureType                                type;
    const void* XR_MAY_ALIAS                       next;
    XrEnvironmentDepthProviderCreateFlagsMETAX2    createFlags;
} XrEnvironmentDepthProviderCreateInfoMETAX2;

typedef struct XrEnvironmentDepthSwapchainCreateInfoMETAX2 {
    XrStructureType                                 type;
    const void* XR_MAY_ALIAS                        next;
    XrEnvironmentDepthSwapchainCreateFlagsMETAX2    createFlags;
} XrEnvironmentDepthSwapchainCreateInfoMETAX2;

typedef struct XrEnvironmentDepthSwapchainPropertiesMETAX2 {
    XrStructureType           type;
    void* XR_MAY_ALIAS        next;
    XrSwapchain               swapchain;
    XrSwapchainCreateFlags    createFlags;
    XrSwapchainUsageFlags     usageFlags;
    int64_t                   format;
    uint32_t                  sampleCount;
    uint32_t                  width;
    uint32_t                  height;
    uint32_t                  faceCount;
    uint32_t                  arraySize;
    uint32_t                  mipCount;
} XrEnvironmentDepthSwapchainPropertiesMETAX2;

typedef struct XrEnvironmentDepthImageAcquireInfoMETAX2 {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrSpace                     space;
    XrTime                      displayTime;
} XrEnvironmentDepthImageAcquireInfoMETAX2;

typedef struct XrEnvironmentDepthImageViewMETAX2 {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrPosef                     pose;
    uint32_t                    swapchainIndex;
    uint32_t                    imageArrayIndex;
    XrFovf                      fov;
    float                       minDepth;
    float                       maxDepth;
    float                       nearZ;
    float                       farZ;
} XrEnvironmentDepthImageViewMETAX2;

typedef struct XrEnvironmentDepthImageMETAX2 {
    XrStructureType                       type;
    const void* XR_MAY_ALIAS              next;
    XrTime                                generationTime;
    XrTime                                displayTime;
    uint32_t                              viewCount;
    XrEnvironmentDepthImageViewMETAX2*    views;
} XrEnvironmentDepthImageMETAX2;

typedef struct XrEnvironmentDepthHandRemovalSetInfoMETAX2 {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
    XrBool32                    enabled;
} XrEnvironmentDepthHandRemovalSetInfoMETAX2;

typedef struct XrSystemEnvironmentDepthPropertiesMETAX2 {
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
    XrBool32              supportsEnvironmentDepth;
} XrSystemEnvironmentDepthPropertiesMETAX2;

typedef XrResult (XRAPI_PTR *PFN_xrCreateEnvironmentDepthProviderMETAX2)(XrSession session, const XrEnvironmentDepthProviderCreateInfoMETAX2* createInfo, XrEnvironmentDepthProviderMETAX2* environmentDepthProvider);
typedef XrResult (XRAPI_PTR *PFN_xrDestroyEnvironmentDepthProviderMETAX2)(XrEnvironmentDepthProviderMETAX2 environmentDepthProvider);
typedef XrResult (XRAPI_PTR *PFN_xrStartEnvironmentDepthProviderMETAX2)(XrEnvironmentDepthProviderMETAX2 environmentDepthProvider);
typedef XrResult (XRAPI_PTR *PFN_xrStopEnvironmentDepthProviderMETAX2)(XrEnvironmentDepthProviderMETAX2 environmentDepthProvider);
typedef XrResult (XRAPI_PTR *PFN_xrCreateEnvironmentDepthSwapchainMETAX2)(XrEnvironmentDepthProviderMETAX2 environmentDepthProvider, const XrEnvironmentDepthSwapchainCreateInfoMETAX2* createInfo, XrEnvironmentDepthSwapchainPropertiesMETAX2* swapchainProperties);
typedef XrResult (XRAPI_PTR *PFN_xrAcquireEnvironmentDepthImageMETAX2)(XrEnvironmentDepthProviderMETAX2 environmentDepthProvider, const XrEnvironmentDepthImageAcquireInfoMETAX2* acquireInfo, XrEnvironmentDepthImageMETAX2* environmentDepthImage);
typedef XrResult (XRAPI_PTR *PFN_xrSetEnvironmentDepthHandRemovalMETAX2)(XrEnvironmentDepthProviderMETAX2 environmentDepthProvider, const XrEnvironmentDepthHandRemovalSetInfoMETAX2* setInfo);

#ifndef XR_NO_PROTOTYPES
#ifdef XR_EXTENSION_PROTOTYPES
XRAPI_ATTR XrResult XRAPI_CALL xrCreateEnvironmentDepthProviderMETAX2(
    XrSession                                   session,
    const XrEnvironmentDepthProviderCreateInfoMETAX2* createInfo,
    XrEnvironmentDepthProviderMETAX2*           environmentDepthProvider);

XRAPI_ATTR XrResult XRAPI_CALL xrDestroyEnvironmentDepthProviderMETAX2(
    XrEnvironmentDepthProviderMETAX2            environmentDepthProvider);

XRAPI_ATTR XrResult XRAPI_CALL xrStartEnvironmentDepthProviderMETAX2(
    XrEnvironmentDepthProviderMETAX2            environmentDepthProvider);

XRAPI_ATTR XrResult XRAPI_CALL xrStopEnvironmentDepthProviderMETAX2(
    XrEnvironmentDepthProviderMETAX2            environmentDepthProvider);

XRAPI_ATTR XrResult XRAPI_CALL xrCreateEnvironmentDepthSwapchainMETAX2(
    XrEnvironmentDepthProviderMETAX2            environmentDepthProvider,
    const XrEnvironmentDepthSwapchainCreateInfoMETAX2* createInfo,
    XrEnvironmentDepthSwapchainPropertiesMETAX2* swapchainProperties);

XRAPI_ATTR XrResult XRAPI_CALL xrAcquireEnvironmentDepthImageMETAX2(
    XrEnvironmentDepthProviderMETAX2            environmentDepthProvider,
    const XrEnvironmentDepthImageAcquireInfoMETAX2* acquireInfo,
    XrEnvironmentDepthImageMETAX2*              environmentDepthImage);

XRAPI_ATTR XrResult XRAPI_CALL xrSetEnvironmentDepthHandRemovalMETAX2(
    XrEnvironmentDepthProviderMETAX2            environmentDepthProvider,
    const XrEnvironmentDepthHandRemovalSetInfoMETAX2* setInfo);
#endif /* XR_EXTENSION_PROTOTYPES */
#endif /* !XR_NO_PROTOTYPES */
#endif /* XR_METAX2_environment_depth */

#ifdef  XR_METAX2_ENVIRONMENT_DEPTH_TAG_ALIAS
typedef XrEnvironmentDepthProviderMETAX2 XrEnvironmentDepthProviderMETA;
typedef XrEnvironmentDepthProviderCreateInfoMETAX2 XrEnvironmentDepthProviderCreateInfoMETA;
typedef XrEnvironmentDepthProviderCreateFlagsMETAX2 XrEnvironmentDepthProviderCreateFlagsMETA;

typedef XrEnvironmentDepthSwapchainCreateFlagsMETAX2 XrEnvironmentDepthSwapchainCreateFlagsMETA;

typedef XrEnvironmentDepthSwapchainCreateInfoMETAX2 XrEnvironmentDepthSwapchainCreateInfoMETA;
typedef XrEnvironmentDepthSwapchainPropertiesMETAX2 XrEnvironmentDepthSwapchainPropertiesMETA;
typedef XrEnvironmentDepthImageAcquireInfoMETAX2 XrEnvironmentDepthImageAcquireInfoMETA;
typedef XrEnvironmentDepthImageViewMETAX2 XrEnvironmentDepthImageViewMETA;
typedef XrEnvironmentDepthImageMETAX2 XrEnvironmentDepthImageMETA;
typedef XrEnvironmentDepthHandRemovalSetInfoMETAX2 XrEnvironmentDepthHandRemovalSetInfoMETA;
typedef XrSystemEnvironmentDepthPropertiesMETAX2 XrSystemEnvironmentDepthPropertiesMETA;
#define XR_META_environment_depth_SPEC_VERSION XR_METAX2_environment_depth_SPEC_VERSION
#define XR_META_ENVIRONMENT_DEPTH_EXTENSION_NAME XR_METAX2_ENVIRONMENT_DEPTH_EXTENSION_NAME
#define XR_OBJECT_TYPE_ENVIRONMENT_DEPTH_PROVIDER_META XR_OBJECT_TYPE_ENVIRONMENT_DEPTH_PROVIDER_METAX2
#define XR_TYPE_ENVIRONMENT_DEPTH_PROVIDER_CREATE_INFO_META XR_TYPE_ENVIRONMENT_DEPTH_PROVIDER_CREATE_INFO_METAX2
#define XR_TYPE_ENVIRONMENT_DEPTH_SWAPCHAIN_CREATE_INFO_META XR_TYPE_ENVIRONMENT_DEPTH_SWAPCHAIN_CREATE_INFO_METAX2
#define XR_TYPE_ENVIRONMENT_DEPTH_SWAPCHAIN_PROPERTIES_META XR_TYPE_ENVIRONMENT_DEPTH_SWAPCHAIN_PROPERTIES_METAX2
#define XR_TYPE_ENVIRONMENT_DEPTH_IMAGE_ACQUIRE_INFO_META XR_TYPE_ENVIRONMENT_DEPTH_IMAGE_ACQUIRE_INFO_METAX2
#define XR_TYPE_ENVIRONMENT_DEPTH_IMAGE_VIEW_META XR_TYPE_ENVIRONMENT_DEPTH_IMAGE_VIEW_METAX2
#define XR_TYPE_ENVIRONMENT_DEPTH_IMAGE_META XR_TYPE_ENVIRONMENT_DEPTH_IMAGE_METAX2
#define XR_TYPE_ENVIRONMENT_DEPTH_HAND_REMOVAL_SET_INFO_META XR_TYPE_ENVIRONMENT_DEPTH_HAND_REMOVAL_SET_INFO_METAX2
#define XR_TYPE_SYSTEM_ENVIRONMENT_DEPTH_PROPERTIES_META XR_TYPE_SYSTEM_ENVIRONMENT_DEPTH_PROPERTIES_METAX2
#define XR_ENVIRONMENT_DEPTH_NOT_AVAILABLE_META XR_ENVIRONMENT_DEPTH_NOT_AVAILABLE_METAX2
#endif /* XR_METAX2_ENVIRONMENT_DEPTH_TAG_ALIAS */


#ifdef __cplusplus
}
#endif

#endif
