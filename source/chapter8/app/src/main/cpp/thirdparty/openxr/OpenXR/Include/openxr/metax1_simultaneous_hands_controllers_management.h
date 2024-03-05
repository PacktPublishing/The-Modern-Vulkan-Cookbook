#ifndef METAX1_SIMULTANEOUS_HANDS_CONTROLLERS_MANAGEMENT_H_
#define METAX1_SIMULTANEOUS_HANDS_CONTROLLERS_MANAGEMENT_H_ 1

/**********************
This file is @generated from the OpenXR XML API registry.
Language    :   C99
Copyright   :   (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.
***********************/

#include <openxr/openxr.h>

#ifdef __cplusplus
extern "C" {
#endif


#ifndef XR_METAX1_simultaneous_hands_controllers_management

#define XR_METAX1_simultaneous_hands_controllers_management 1
#define XR_METAX1_simultaneous_hands_controllers_management_SPEC_VERSION 1
#define XR_METAX1_SIMULTANEOUS_HANDS_CONTROLLERS_MANAGEMENT_EXTENSION_NAME "XR_METAX1_simultaneous_hands_controllers_management"
static const XrStructureType XR_TYPE_SYSTEM_SIMULTANEOUS_HANDS_CONTROLLERS_PROPERTIES_METAX1 = (XrStructureType) 1000280001;
static const XrStructureType XR_TYPE_SIMULTANEOUS_HANDS_CONTROLLERS_TRACKING_RESUME_INFO_METAX1 = (XrStructureType) 1000280002;
static const XrStructureType XR_TYPE_SIMULTANEOUS_HANDS_CONTROLLERS_TRACKING_PAUSE_INFO_METAX1 = (XrStructureType) 1000280003;
typedef struct XrSystemSimultaneousHandsControllersPropertiesMETAX1 {
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
    XrBool32              supportsSimultaneousHandsAndControllers;
} XrSystemSimultaneousHandsControllersPropertiesMETAX1;

typedef struct XrSimultaneousHandsControllersTrackingResumeInfoMETAX1 {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
} XrSimultaneousHandsControllersTrackingResumeInfoMETAX1;

typedef struct XrSimultaneousHandsControllersTrackingPauseInfoMETAX1 {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
} XrSimultaneousHandsControllersTrackingPauseInfoMETAX1;

typedef XrResult (XRAPI_PTR *PFN_xrResumeSimultaneousHandsControllersTrackingMETAX1)(XrSession session, const XrSimultaneousHandsControllersTrackingResumeInfoMETAX1* resumeInfo);
typedef XrResult (XRAPI_PTR *PFN_xrPauseSimultaneousHandsControllersTrackingMETAX1)(XrSession session, const XrSimultaneousHandsControllersTrackingPauseInfoMETAX1* pauseInfo);

#ifndef XR_NO_PROTOTYPES
#ifdef XR_EXTENSION_PROTOTYPES
XRAPI_ATTR XrResult XRAPI_CALL xrResumeSimultaneousHandsControllersTrackingMETAX1(
    XrSession                                   session,
    const XrSimultaneousHandsControllersTrackingResumeInfoMETAX1* resumeInfo);

XRAPI_ATTR XrResult XRAPI_CALL xrPauseSimultaneousHandsControllersTrackingMETAX1(
    XrSession                                   session,
    const XrSimultaneousHandsControllersTrackingPauseInfoMETAX1* pauseInfo);
#endif /* XR_EXTENSION_PROTOTYPES */
#endif /* !XR_NO_PROTOTYPES */
#endif /* XR_METAX1_simultaneous_hands_controllers_management */

#ifdef  XR_METAX1_SIMULTANEOUS_HANDS_CONTROLLERS_MANAGEMENT_TAG_ALIAS
typedef XrSystemSimultaneousHandsControllersPropertiesMETAX1 XrSystemSimultaneousHandsControllersPropertiesMETA;
typedef XrSimultaneousHandsControllersTrackingResumeInfoMETAX1 XrSimultaneousHandsControllersTrackingResumeInfoMETA;
typedef XrSimultaneousHandsControllersTrackingPauseInfoMETAX1 XrSimultaneousHandsControllersTrackingPauseInfoMETA;
#define XR_META_simultaneous_hands_controllers_management_SPEC_VERSION XR_METAX1_simultaneous_hands_controllers_management_SPEC_VERSION
#define XR_META_SIMULTANEOUS_HANDS_CONTROLLERS_MANAGEMENT_EXTENSION_NAME XR_METAX1_SIMULTANEOUS_HANDS_CONTROLLERS_MANAGEMENT_EXTENSION_NAME
#define XR_TYPE_SYSTEM_SIMULTANEOUS_HANDS_CONTROLLERS_PROPERTIES_META XR_TYPE_SYSTEM_SIMULTANEOUS_HANDS_CONTROLLERS_PROPERTIES_METAX1
#define XR_TYPE_SIMULTANEOUS_HANDS_CONTROLLERS_TRACKING_RESUME_INFO_META XR_TYPE_SIMULTANEOUS_HANDS_CONTROLLERS_TRACKING_RESUME_INFO_METAX1
#define XR_TYPE_SIMULTANEOUS_HANDS_CONTROLLERS_TRACKING_PAUSE_INFO_META XR_TYPE_SIMULTANEOUS_HANDS_CONTROLLERS_TRACKING_PAUSE_INFO_METAX1
#endif /* XR_METAX1_SIMULTANEOUS_HANDS_CONTROLLERS_MANAGEMENT_TAG_ALIAS */


#ifdef __cplusplus
}
#endif

#endif
