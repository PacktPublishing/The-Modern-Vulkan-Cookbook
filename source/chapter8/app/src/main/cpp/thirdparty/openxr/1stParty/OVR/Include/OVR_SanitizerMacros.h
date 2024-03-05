// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#pragma once

#if defined(__has_feature)
#if __has_feature(address_sanitizer)
#define OVR_USING_ADDRESS_SANITIZER
#endif
#endif

#if defined(OVR_USING_ADDRESS_SANITIZER)

// Refer to https://clang.llvm.org/docs/AddressSanitizer.html#issue-suppression
#define OVR_NO_SANITIZE_ADDRESS __attribute__((no_sanitize_address))

#else // __has_feature(address_sanitizer)

#define OVR_NO_SANITIZE_ADDRESS

#endif // defined(OVR_USING_ADDRESS_SANITIZER)
