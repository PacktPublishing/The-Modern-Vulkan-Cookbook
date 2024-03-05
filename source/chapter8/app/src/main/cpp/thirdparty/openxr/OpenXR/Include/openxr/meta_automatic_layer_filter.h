#ifndef META_AUTOMATIC_LAYER_FILTER_H_
#define META_AUTOMATIC_LAYER_FILTER_H_ 1

/**********************
This file is @generated from the OpenXR XML API registry.
Language    :   C99
Copyright   :   (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.
***********************/

#include <openxr/openxr.h>

#ifdef __cplusplus
extern "C" {
#endif


#ifndef XR_META_automatic_layer_filter

#define XR_META_automatic_layer_filter 1
#define XR_META_automatic_layer_filter_SPEC_VERSION 1
#define XR_META_AUTOMATIC_LAYER_FILTER_EXTENSION_NAME "XR_META_automatic_layer_filter"
// Specifies that the runtime may automatically toggle a texture filtering mechanism to improve visual quality of layer
static const XrCompositionLayerSettingsFlagsFB XR_COMPOSITION_LAYER_SETTINGS_AUTO_LAYER_FILTER_BIT_META = (XrCompositionLayerSettingsFlagsFB) 0x00000020;
#endif /* XR_META_automatic_layer_filter */

#ifdef __cplusplus
}
#endif

#endif
