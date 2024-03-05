/************************************************************************************

Filename  : SpatialAnchorFileHandler.cpp
Content   : Handler for text file-based communication of Spatial Anchors and User IDs.
Created   :
Authors   :

Copyright : Copyright (c) Meta Platforms, Inc. and its affiliates. All rights reserved.

*************************************************************************************/

#include <cassert>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>

#include "SpatialAnchorFileHandler.h"
#include "SpatialAnchorUtilities.h"

#if defined(ANDROID)
#include <android/log.h>
#endif

#define DEBUG 1

#if defined(ANDROID)
#define OVR_LOG_TAG "SpatialAnchorFileHandler"

#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, OVR_LOG_TAG, __VA_ARGS__)
#define ALOGW(...) __android_log_print(ANDROID_LOG_WARN, OVR_LOG_TAG, __VA_ARGS__)
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, OVR_LOG_TAG, __VA_ARGS__)
#else
#define ALOGE(...)       \
    printf("ERROR: ");   \
    printf(__VA_ARGS__); \
    printf("\n")
#define ALOGW(...)       \
    printf("WARN: "); \
    printf(__VA_ARGS__); \
    printf("\n")
#define ALOGV(...)       \
    printf("VERBOSE: "); \
    printf(__VA_ARGS__); \
    printf("\n")
#endif

SpatialAnchorFileHandler::SpatialAnchorFileHandler() {
  dataDir = kDefaultDataPath;
  ALOGV("Using data path %s", dataDir.c_str());
  assert(dataDir.back() == '/' || dataDir.back() == '\\');
}

bool SpatialAnchorFileHandler::LoadShareUserList(std::vector<XrSpaceUserIdFB>& userIdList) {
  ALOGV("LoadShareUserList");

  std::string filePath = dataDir + kShareUserListFilename;

  ::FILE* file = ::fopen(filePath.c_str(), "r");
  if (!file) {
    ALOGE("LoadShareUserList: Failed to open file: %s", filePath.c_str());
    return false;
  }
  if (::feof(file)) {
    ALOGE("LoadShareUserList: File is empty");
    return false;
  }
  const int maxUserIdCstrLength = 21;
  char line[maxUserIdCstrLength];
  while (!::feof(file)) {
    ::fscanf(file, "%20s", line);
    if (::ferror(file)) {
      ALOGE("LoadShareUserList: Failed to read from file: %s", filePath.c_str());
      fclose(file);
      return false;
    }
    XrSpaceUserIdFB userId = (XrSpaceUserIdFB)strtoull(line, nullptr, 10);
    userIdList.emplace_back(userId);
  }
  fclose(file);
  return true;
}

bool SpatialAnchorFileHandler::LoadInboundSpatialAnchorList(std::vector<XrUuidEXT>& spatialAnchorList) {
  ALOGV("LoadInboundSpatialAnchorList");

  std::string filePath = dataDir + kInboundSpatialAnchorListFilename;

  ::FILE* file = ::fopen(filePath.c_str(), "r");
  if (!file) {
    ALOGE("LoadInboundSpatialAnchorList: Failed to open file: %s", filePath.c_str());
    return false;
  }
  if (::feof(file)) {
    ALOGE("LoadInboundSpatialAnchorList: File is empty");
    return false;
  }
  const int uuidCstrLength = XR_UUID_SIZE_EXT * 2 + 1;
  char line[uuidCstrLength];
  while (!::feof(file)) {
    ::fscanf(file, "%32s\n", line);
    if (::ferror(file)) {
      ALOGE("LoadInboundSpatialAnchorList: Failed to read from file: %s", filePath.c_str());
      fclose(file);
      return false;
    }
    XrUuidEXT uuid;
    if (!hexStringToUuid(line, uuid)) {
      ALOGE("LoadInboundSpatialAnchorList: Failed to parse UUID string: %s", line);
    }
    spatialAnchorList.emplace_back(uuid);
  }
  fclose(file);
  return true;
}

bool SpatialAnchorFileHandler::WriteSharedSpatialAnchorList(const std::vector<XrUuidEXT>& spatialAnchorList, const std::vector<XrSpaceUserIdFB>& userIdList) {
  ALOGV("WriteSharedSpatialAnchorList");
  if (spatialAnchorList.size() == 0) {
    ALOGE("WriteSharedSpatialAnchorList: spatialAnchorList cannot be empty");
    return false;
  }

  std::string filePath = dataDir + kSharedSpatialAnchorListFilename;
  ::FILE* file = ::fopen(filePath.c_str(), "w");
  if (!file) {
    ALOGE("Failed to create file: %s", filePath.c_str());
    return false;
  }

  int res;
  for (uint32_t i = 0; i < spatialAnchorList.size(); i++) {
    // We'll use a human-readable format for easier debugging.
    res = ::fprintf(file, "%s\n", uuidToHexString(spatialAnchorList[i]).c_str());
    if (res <= 0) {
      ALOGE("Failed to write data to file: %s", filePath.c_str());
      break;
    }
  }
  fclose(file);
  return (res > 0);
}
