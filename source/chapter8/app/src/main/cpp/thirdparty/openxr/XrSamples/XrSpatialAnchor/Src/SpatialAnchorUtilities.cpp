/************************************************************************************

Filename  : SpatialAnchorUtilities.cpp
Content   : Utility functions for XrSpatialAnchor
Created   :
Authors   :

Copyright : Copyright (c) Meta Platforms, Inc. and its affiliates. All rights reserved.

*************************************************************************************/

#include "SpatialAnchorUtilities.h"

std::string bin2hex(const uint8_t* src, uint32_t size) {
    std::string res;
    res.reserve(size * 2);
    const char hex[] = "0123456789ABCDEF";
    for (uint32_t i = 0; i < size; ++i) {
        uint8_t c = src[i];
        res += hex[c >> 4];
        res += hex[c & 0xf];
    }
    return res;
}

std::string uuidToHexString(const XrUuidEXT& uuid) {
    return bin2hex(reinterpret_cast<const uint8_t*>(uuid.data), XR_UUID_SIZE_EXT);
}

bool hexStringToUuid(const std::string& hex, XrUuidEXT& uuid) {
    if (hex.length() != XR_UUID_SIZE_EXT * 2) {
        return false;
    }
    for (uint32_t i = 0, k = 0; i < XR_UUID_SIZE_EXT; i++, k+=2) {
        std::string byteStr = hex.substr(k, 2);
        uuid.data[i] = (uint8_t)stol(byteStr, nullptr, 16);
    }
    return true;
}

bool isExtensionEnumerated(
    const char* extensionName,
    XrExtensionProperties enumeratedExtensions[],
    uint32_t enumeratedExtensionCount) {
    for (uint32_t i = 0; i < enumeratedExtensionCount; i++) {
        if (strcmp(extensionName, enumeratedExtensions[i].extensionName) == 0) {
            return true;
        }
    }
    return false;
}
