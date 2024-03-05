#pragma once

#include <openxr/openxr.h>
#include <string>

std::string bin2hex(const uint8_t* src, uint32_t size);
std::string uuidToHexString(const XrUuidEXT& uuid);
bool hexStringToUuid(const std::string& hex, XrUuidEXT& uuid);
bool isExtensionEnumerated(
    const char* extensionName,
    XrExtensionProperties enumeratedExtensions[],
    uint32_t enumeratedExtensionCount);
