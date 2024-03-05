#ifndef META_SPATIAL_ENTITY_MESH_H_
#define META_SPATIAL_ENTITY_MESH_H_ 1

/**********************
This file is @generated from the OpenXR XML API registry.
Language    :   C99
Copyright   :   (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.
***********************/

#include <openxr/openxr.h>

#ifdef __cplusplus
extern "C" {
#endif


#ifndef XR_META_spatial_entity_mesh

#define XR_META_spatial_entity_mesh 1
#define XR_META_spatial_entity_mesh_SPEC_VERSION 1
#define XR_META_SPATIAL_ENTITY_MESH_EXTENSION_NAME "XR_META_spatial_entity_mesh"
static const XrSpaceComponentTypeFB XR_SPACE_COMPONENT_TYPE_TRIANGLE_MESH_META = (XrSpaceComponentTypeFB) 1000269000;
static const XrStructureType XR_TYPE_SPACE_TRIANGLE_MESH_GET_INFO_META = (XrStructureType) 1000269001;
static const XrStructureType XR_TYPE_SPACE_TRIANGLE_MESH_META = (XrStructureType) 1000269002;
typedef struct XrSpaceTriangleMeshGetInfoMETA {
    XrStructureType             type;
    const void* XR_MAY_ALIAS    next;
} XrSpaceTriangleMeshGetInfoMETA;

typedef struct XrSpaceTriangleMeshMETA {
    XrStructureType       type;
    void* XR_MAY_ALIAS    next;
    uint32_t              vertexCapacityInput;
    uint32_t              vertexCountOutput;
    XrVector3f*           vertices;
    uint32_t              indexCapacityInput;
    uint32_t              indexCountOutput;
    uint32_t*             indices;
} XrSpaceTriangleMeshMETA;

typedef XrResult (XRAPI_PTR *PFN_xrGetSpaceTriangleMeshMETA)(XrSpace space, const XrSpaceTriangleMeshGetInfoMETA* getInfo, XrSpaceTriangleMeshMETA* triangleMeshOutput);

#ifndef XR_NO_PROTOTYPES
#ifdef XR_EXTENSION_PROTOTYPES
XRAPI_ATTR XrResult XRAPI_CALL xrGetSpaceTriangleMeshMETA(
    XrSpace                                     space,
    const XrSpaceTriangleMeshGetInfoMETA*       getInfo,
    XrSpaceTriangleMeshMETA*                    triangleMeshOutput);
#endif /* XR_EXTENSION_PROTOTYPES */
#endif /* !XR_NO_PROTOTYPES */
#endif /* XR_META_spatial_entity_mesh */

#ifdef __cplusplus
}
#endif

#endif
