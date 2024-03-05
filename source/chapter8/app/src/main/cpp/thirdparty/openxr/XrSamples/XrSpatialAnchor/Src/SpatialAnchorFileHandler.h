#pragma once

#include "SpatialAnchorExternalDataHandler.h"
#include <string>
#include <vector>
#include <openxr/openxr.h>
#include <openxr/fb_spatial_entity_user.h>

class SpatialAnchorFileHandler : public SpatialAnchorExternalDataHandler {
 public:
  SpatialAnchorFileHandler();

  // LoadShareUserList loads the list of FBIDs of users with whom to share Spatial Anchors.
  bool LoadShareUserList(std::vector<XrSpaceUserIdFB>& userIdList) override;
  bool LoadInboundSpatialAnchorList(std::vector<XrUuidEXT>& spatialAnchorList) override;
  bool WriteSharedSpatialAnchorList(const std::vector<XrUuidEXT>& spatialAnchorList, const std::vector<XrSpaceUserIdFB>& userIdList) override;

 private:
  std::string dataDir;
  const char* kShareUserListFilename = "shareUserList.txt";
  const char* kInboundSpatialAnchorListFilename = "inboundSpatialAnchorList.txt";
  const char* kSharedSpatialAnchorListFilename = "sharedSpatialAnchorList.txt";

// Replace this value with the path you want the named files above to be.
// Make sure to include the trailing slash (backslash for Windows).
#ifdef WIN32
  const char* kDefaultDataPath = "C:\\temp_SpatialAnchorXr\\";
#else
  const char* kDefaultDataPath = "/sdcard/Android/data/com.oculus.sdk.spatialanchor/files/";
#endif
};
