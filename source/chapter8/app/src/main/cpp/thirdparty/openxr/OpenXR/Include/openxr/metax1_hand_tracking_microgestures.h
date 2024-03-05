#ifndef METAX1_HAND_TRACKING_MICROGESTURES_H_
#define METAX1_HAND_TRACKING_MICROGESTURES_H_ 1

/**********************
This file is @generated from the OpenXR XML API registry.
Language    :   C99
Copyright   :   (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.
***********************/

#include <openxr/openxr.h>

#ifdef __cplusplus
extern "C" {
#endif


#ifndef XR_METAX1_hand_tracking_microgestures

#define XR_METAX1_hand_tracking_microgestures 1
#define XR_METAX1_hand_tracking_microgestures_SPEC_VERSION 1
#define XR_METAX1_HAND_TRACKING_MICROGESTURES_EXTENSION_NAME "XR_METAX1_hand_tracking_microgestures"
#define XR_METAX1_hand_tracking_microgestures_total_gesture_types_METAX1 5
static const XrStructureType XR_TYPE_SYSTEM_MICROGESTURE_PROPERTIES_METAX1 = (XrStructureType) 1000252001;
static const XrStructureType XR_TYPE_MICROGESTURE_DETECTION_START_INFO_METAX1 = (XrStructureType) 1000252002;
static const XrStructureType XR_TYPE_MICROGESTURE_DETECTION_STOP_INFO_METAX1 = (XrStructureType) 1000252003;
typedef struct XrSystemMicrogesturePropertiesMETAX1 {
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
    XrBool32              supportsMicrogestures;
} XrSystemMicrogesturePropertiesMETAX1;

typedef struct XrMicrogestureDetectionStartInfoMETAX1 {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
} XrMicrogestureDetectionStartInfoMETAX1;

typedef struct XrMicrogestureDetectionStopInfoMETAX1 {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
} XrMicrogestureDetectionStopInfoMETAX1;

typedef XrResult (XRAPI_PTR *PFN_xrStartMicrogestureDetectionMETAX1)(XrSession session, const XrMicrogestureDetectionStartInfoMETAX1* startInfo);
typedef XrResult (XRAPI_PTR *PFN_xrStopMicrogestureDetectionMETAX1)(XrSession session, const XrMicrogestureDetectionStopInfoMETAX1* stopInfo);

#ifndef XR_NO_PROTOTYPES
#ifdef XR_EXTENSION_PROTOTYPES
XRAPI_ATTR XrResult XRAPI_CALL xrStartMicrogestureDetectionMETAX1(
    XrSession                                   session,
    const XrMicrogestureDetectionStartInfoMETAX1* startInfo);

XRAPI_ATTR XrResult XRAPI_CALL xrStopMicrogestureDetectionMETAX1(
    XrSession                                   session,
    const XrMicrogestureDetectionStopInfoMETAX1* stopInfo);
#endif /* XR_EXTENSION_PROTOTYPES */
#endif /* !XR_NO_PROTOTYPES */
#endif /* XR_METAX1_hand_tracking_microgestures */

#ifdef  XR_METAX1_HAND_TRACKING_MICROGESTURES_TAG_ALIAS
typedef XrSystemMicrogesturePropertiesMETAX1 XrSystemMicrogesturePropertiesMETA;
typedef XrMicrogestureDetectionStartInfoMETAX1 XrMicrogestureDetectionStartInfoMETA;
typedef XrMicrogestureDetectionStopInfoMETAX1 XrMicrogestureDetectionStopInfoMETA;
#define XR_META_hand_tracking_microgestures_SPEC_VERSION XR_METAX1_hand_tracking_microgestures_SPEC_VERSION
#define XR_META_HAND_TRACKING_MICROGESTURES_EXTENSION_NAME XR_METAX1_HAND_TRACKING_MICROGESTURES_EXTENSION_NAME
#define XR_META_hand_tracking_microgestures_total_gesture_types_META XR_METAX1_hand_tracking_microgestures_total_gesture_types_METAX1
#define XR_TYPE_SYSTEM_MICROGESTURE_PROPERTIES_META XR_TYPE_SYSTEM_MICROGESTURE_PROPERTIES_METAX1
#define XR_TYPE_MICROGESTURE_DETECTION_START_INFO_META XR_TYPE_MICROGESTURE_DETECTION_START_INFO_METAX1
#define XR_TYPE_MICROGESTURE_DETECTION_STOP_INFO_META XR_TYPE_MICROGESTURE_DETECTION_STOP_INFO_METAX1
#endif /* XR_METAX1_HAND_TRACKING_MICROGESTURES_TAG_ALIAS */


#ifdef __cplusplus
}
#endif

#endif
