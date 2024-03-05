#pragma once

#include <vector>
#include <openxr/openxr.h>
#include <openxr/fb_spatial_entity_user.h>

class SpatialAnchorExternalDataHandler {
 public:
  virtual ~SpatialAnchorExternalDataHandler() {}

  // LoadShareUserList loads the list of FBIDs of users with whom to share Spatial Anchors.
  virtual bool LoadShareUserList(std::vector<XrSpaceUserIdFB>& userIdList) = 0;

  // LoadInboundSpatialAnchorList loads the list of Spatial Anchors that have
  // been shared with the current user.
  virtual bool LoadInboundSpatialAnchorList(std::vector<XrUuidEXT>& spatialAnchorList) = 0;

  // WriteSharedSpatialAnchorList emits the list of Spatial Anchors shared by the current user
  // to the specified list of users.
  virtual bool WriteSharedSpatialAnchorList(const std::vector<XrUuidEXT>& spatialAnchorList, const std::vector<XrSpaceUserIdFB>& userIdList) = 0;
};
