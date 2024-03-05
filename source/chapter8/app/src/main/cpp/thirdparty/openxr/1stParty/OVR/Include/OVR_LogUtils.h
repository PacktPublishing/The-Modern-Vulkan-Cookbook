// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

/************************************************************************************

Filename    :   OVR_LogUtils.h (Previously Log.h)
Content     :   Macros and helpers for Android logging.
Created     :   4/15/2014
Authors     :   Jonathan E. Wright

*************************************************************************************/

#if !defined(OVRLib_Log_h)
#define OVRLib_Log_h

#include <atomic>
#include <chrono>
#include <memory>

#include "OVR_Types.h"
#include "OVR_Std.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h> /* va_list, va_start, va_arg, va_end */
#include <time.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#if defined(WIN32) || defined(WIN64) || defined(_WIN32) || defined(_WIN64)
#if !defined(NOMINMAX)
#define NOMINMAX // stop Windows.h from redefining min and max and breaking std::min / std::max
#endif
#include <windows.h> // OutputDebugString
#endif

#if defined(OVR_OS_ANDROID)
#include <android/log.h>
#include <jni.h>

void LogWithTag(const int prio, const char* tag, const char* fmt, ...)
    __attribute__((__format__(printf, 3, 4))) __attribute__((__nonnull__(3)));

void LogWithFileTag(const int prio, const char* fileTag, const char* fmt, ...)
    __attribute__((__format__(printf, 3, 4))) __attribute__((__nonnull__(3)));

#endif

// Log with an explicit tag
inline void LogWithTag(const int prio, const char* tag, const char* fmt, ...) {
#if defined(OVR_OS_ANDROID)
    va_list ap;
    va_start(ap, fmt);
    __android_log_vprint(prio, tag, fmt, ap);
    va_end(ap);
#elif defined(OVR_OS_WIN32)
    OVR_UNUSED(tag);
    OVR_UNUSED(prio);

    va_list args;
    va_start(args, fmt);

    char buffer[4096];
    vsnprintf_s(buffer, 4096, _TRUNCATE, fmt, args);
    va_end(args);

    OutputDebugStringA(buffer);
#elif (defined(OVR_OS_LINUX) || defined(OVR_OS_MAC) || defined(OVR_OS_IPHONE))
    OVR_UNUSED(prio);
    OVR_UNUSED(tag);
    OVR_UNUSED(fmt);
#else
#warning "LogWithTag not implemented for this given OVR_OS_"
#endif
}

/// The log timer is copied from XR_LOG's log timer
namespace {
struct OVR_LogTimer {
    explicit constexpr OVR_LogTimer(std::chrono::steady_clock::duration duration)
        : nextLogTime_(std::chrono::steady_clock::time_point::min().time_since_epoch().count()),
          duration_(duration.count()) {}

    bool OVR_shouldLogNow() {
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        auto nextLogTime = nextLogTime_.load(std::memory_order_relaxed);
        // It is possible that multiple shouldLogNow() calls observe nextLogTime <= now
        // simultaneously, the compare-exchange however will only succeed for one of them.
        return nextLogTime <= now &&
            nextLogTime_.compare_exchange_strong(
                nextLogTime, now + duration_, std::memory_order_relaxed);
    }

   private:
    std::atomic<std::chrono::steady_clock::duration::rep> nextLogTime_;
    const std::chrono::steady_clock::duration::rep duration_;
};

template <typename T1, typename T2>
bool OVR_shouldLogNow(T1 nanoseconds, T2 dummyLambda) {
    static OVR_LogTimer timer{std::chrono::steady_clock::duration(nanoseconds)};
    return timer.OVR_shouldLogNow();
}
} // namespace

// Strips the directory and extension from fileTag to give a concise log tag
inline void FilePathToTag(const char* filePath, char* strippedTag, const int strippedTagSize) {
    if (strippedTag == nullptr || strippedTagSize == 0) {
        return;
    }

    // scan backwards from the end to the first slash
    const int len = static_cast<int>(strlen(filePath));
    int slash;
    for (slash = len - 1; slash > 0 && filePath[slash] != '/' && filePath[slash] != '\\'; slash--) {
    }
    if (filePath[slash] == '/' || filePath[slash] == '\\') {
        slash++;
    }
    // copy forward until a dot or 0
    int i;
    for (i = 0; i < strippedTagSize - 1; i++) {
        const char c = filePath[slash + i];
        if (c == '.' || c == 0) {
            break;
        }
        strippedTag[i] = c;
    }
    strippedTag[i] = 0;
}

// Strips the directory and extension from fileTag to give a concise log tag
inline void LogWithFileTag(const int prio, const char* fileTag, const char* fmt, ...) {
    if (fileTag == nullptr) {
        return;
    }
#if defined(OVR_OS_ANDROID)
    va_list ap, ap2;

    // fileTag will be something like "jni/App.cpp", which we
    // want to strip down to just "App"
    char strippedTag[128];

    FilePathToTag(fileTag, strippedTag, static_cast<int>(sizeof(strippedTag)));

    va_start(ap, fmt);

    // Calculate the length of the log message... if its too long __android_log_vprint() will clip
    // it!
    va_copy(ap2, ap);
    const int requiredLen = vsnprintf(NULL, 0, fmt, ap2);
    va_end(ap2);
    if (requiredLen < 0) {
        __android_log_write(prio, strippedTag, "ERROR: failed to calculate log length!");
        return;
    }
    const size_t loglen = static_cast<size_t>(requiredLen);
    if (prio == ANDROID_LOG_ERROR) {
        // For FAIL messages which are longer than 512, truncate at 512.
        // We do not know the max size of abort message that will be taken by SIGABRT. 512 has been
        // verified to work
        char* formattedMsg = (char*)malloc(512);
        if (formattedMsg == nullptr) {
            __android_log_write(prio, strippedTag, "ERROR: out of memory allocating log message!");
        } else {
            va_copy(ap2, ap);
            vsnprintf(formattedMsg, 512U, fmt, ap2);
            va_end(ap2);
            __android_log_assert("FAIL", strippedTag, "%s", formattedMsg);
            free(formattedMsg);
        }
    }
    if (loglen < 512) {
        // For short messages just use android's default formatting path (which has a fixed size
        // buffer on the stack).
        __android_log_vprint(prio, strippedTag, fmt, ap);
    } else {
        // For long messages allocate off the heap to avoid blowing the stack...
        char* formattedMsg = (char*)malloc(loglen + 1);
        if (formattedMsg == nullptr) {
            __android_log_write(prio, strippedTag, "ERROR: out of memory allocating log message!");
        } else {
            vsnprintf(formattedMsg, loglen + 1, fmt, ap);
            __android_log_write(prio, strippedTag, formattedMsg);
            free(formattedMsg);
        }
    }

    va_end(ap);
#elif defined(OVR_OS_WIN32)
    OVR_UNUSED(fileTag);
    OVR_UNUSED(prio);

    va_list args;
    va_start(args, fmt);

    char buffer[4096];
    vsnprintf_s(buffer, 4096, _TRUNCATE, fmt, args);
    va_end(args);

    OutputDebugStringA(buffer);
    OutputDebugStringA("\n");
#elif (defined(OVR_OS_LINUX) || defined(OVR_OS_MAC) || defined(OVR_OS_IPHONE))
    OVR_UNUSED(prio);
    OVR_UNUSED(fileTag);
    OVR_UNUSED(fmt);
#else
#warning "LogWithFileTag not implemented for this given OVR_OS_"
#endif
}

#if defined(OVR_OS_WIN32) // allow this file to be included in PC projects

#define OVR_LOG(...) LogWithFileTag(0, __FILE__, __VA_ARGS__)
#define OVR_WARN(...) LogWithFileTag(0, __FILE__, __VA_ARGS__)

// This used to be called OVR_ERROR, but it would crash on mobile devices,
// but not on Windows. This was surprising to many devs and has led to multiple serious incidents.
// Please do not use this function. Be more explicit about the intended behavior, and use FAIL or
// WARN instead.
#define OVR_ERROR_CRASH_MOBILE_USE_WARN_OR_FAIL(...) \
    { LogWithFileTag(0, __FILE__, __VA_ARGS__); }

#define OVR_FAIL(...)                             \
    {                                             \
        LogWithFileTag(0, __FILE__, __VA_ARGS__); \
        abort();                                  \
    }
#define OVR_LOG_WITH_TAG(__tag__, ...) LogWithTag(0, __FILE__, __VA_ARGS__)
#define OVR_ASSERT_WITH_TAG(__expr__, __tag__)

#elif defined(OVR_OS_ANDROID)

// Our standard logging (and far too much of our debugging) involves writing
// to the system log for viewing with logcat.  Previously we defined separate
// LOG() macros in each file to give them file-specific tags for filtering;
// now we use this helper function to use a OVR_LOG_TAG define (via cflags or
// #define OVR_LOG_TAG in source file) when available. Fallback to using a massaged
// __FILE__ macro turning the file base in to a tag -- jni/App.cpp becomes the
// tag "App".
#ifdef OVR_LOG_TAG
#define OVR_LOG(...) ((void)LogWithTag(ANDROID_LOG_INFO, OVR_LOG_TAG, __VA_ARGS__))
#define OVR_WARN(...) ((void)LogWithTag(ANDROID_LOG_WARN, OVR_LOG_TAG, __VA_ARGS__))

// This used to be called OVR_ERROR, but it would crash on mobile devices,
// but not on Windows. This was surprising to many devs and has led to multiple serious incidents.
// Please do not use this function. Be more explicit about the intended behavior, and use FAIL or
// WARN instead.
#define OVR_ERROR_CRASH_MOBILE_USE_WARN_OR_FAIL(...) \
    { (void)LogWithTag(ANDROID_LOG_ERROR, OVR_LOG_TAG, __VA_ARGS__); }

#define OVR_FAIL(...)                                                  \
    {                                                                  \
        (void)LogWithTag(ANDROID_LOG_ERROR, OVR_LOG_TAG, __VA_ARGS__); \
        abort();                                                       \
    }
#else
#define OVR_LOG(...) LogWithFileTag(ANDROID_LOG_INFO, __FILE__, __VA_ARGS__)
#define OVR_WARN(...) LogWithFileTag(ANDROID_LOG_WARN, __FILE__, __VA_ARGS__)

// This used to be called OVR_ERROR, but it would crash on mobile devices,
// but not on Windows. This was surprising to many devs and has led to multiple serious incidents.
// Please do not use this function. Be more explicit about the intended behavior, and use FAIL or
// WARN instead.
#define OVR_ERROR_CRASH_MOBILE_USE_WARN_OR_FAIL(...) \
    { LogWithFileTag(ANDROID_LOG_ERROR, __FILE__, __VA_ARGS__); }

#define OVR_FAIL(...)                                             \
    {                                                             \
        LogWithFileTag(ANDROID_LOG_ERROR, __FILE__, __VA_ARGS__); \
        abort();                                                  \
    }
#endif

#define OVR_LOG_WITH_TAG(__tag__, ...) ((void)LogWithTag(ANDROID_LOG_INFO, __tag__, __VA_ARGS__))
#define OVR_WARN_WITH_TAG(__tag__, ...) ((void)LogWithTag(ANDROID_LOG_WARN, __tag__, __VA_ARGS__))
#define OVR_FAIL_WITH_TAG(__tag__, ...)                            \
    {                                                              \
        (void)LogWithTag(ANDROID_LOG_ERROR, __tag__, __VA_ARGS__); \
        abort();                                                   \
    }

// LOG (usually defined on a per-file basis to write to a specific tag) is for logging that can be
// checked in enabled and generally only prints once or infrequently. SPAM is for logging you want
// to see every frame but should never be checked in
#if defined(OVR_BUILD_DEBUG)
// you should always comment this out before checkin
//#define ALLOW_LOG_SPAM
#endif

#if defined(ALLOW_LOG_SPAM)
#define SPAM(...) LogWithTag(ANDROID_LOG_VERBOSE, "Spam", __VA_ARGS__)
#else
#define SPAM(...) \
    {}
#endif

// TODO: we need a define for internal builds that will compile in assertion messages but not debug
// breaks and we need a define for external builds that will do nothing when an assert is hit.
#if !defined(OVR_BUILD_DEBUG)
#define OVR_ASSERT_WITH_TAG(__expr__, __tag__)                             \
    {                                                                      \
        if (!(__expr__)) {                                                 \
            OVR_WARN_WITH_TAG(__tag__, "ASSERTION FAILED: %s", #__expr__); \
        }                                                                  \
    }
#else
#define OVR_ASSERT_WITH_TAG(__expr__, __tag__)                             \
    {                                                                      \
        if (!(__expr__)) {                                                 \
            OVR_WARN_WITH_TAG(__tag__, "ASSERTION FAILED: %s", #__expr__); \
            OVR_DEBUG_BREAK;                                               \
        }                                                                  \
    }
#endif

#elif (defined(OVR_OS_MAC) || defined(OVR_OS_LINUX) || defined(OVR_OS_IPHONE))
#include <string>
#include <folly/logging/xlog.h>

// Helper that converts a printf-style format string to an std::string. Needed
// because the folly macros don't accept printf-style strings.
static inline std::string ovrLogConvertPrintfToString(const char* format, ...) {
    va_list args;
    va_start(args, format);

    // Determine the required size of the formatted string
    va_list argsCopy; // we can't reuse va_list, so copy
    va_copy(argsCopy, args);
    const int size = std::vsnprintf(nullptr, 0, format, argsCopy);
    va_end(argsCopy);

    if (size <= 0) {
        va_end(args);
        return "";
    }

    // Allocate and format the string
    std::string result(size, '\0');
    std::vsnprintf(result.data(), result.size() + 1, format, args);

    va_end(args);

    return result;
}

#define OVR_LOG(...) XLOG(INFO, ovrLogConvertPrintfToString(__VA_ARGS__))
#define OVR_WARN(...) XLOG(WARN, ovrLogConvertPrintfToString(__VA_ARGS__))

// This used to be called OVR_ERROR, but it would crash on mobile devices,
// but not on Windows. This was surprising to many devs and has led to multiple serious incidents.
// Please do not use this function. Be more explicit about the intended behavior, and use FAIL or
// WARN instead.
#define OVR_ERROR_CRASH_MOBILE_USE_WARN_OR_FAIL(...) \
    XLOG(ERR, ovrLogConvertPrintfToString(__VA_ARGS__))

#define OVR_FAIL(...) XLOG(FATAL, ovrLogConvertPrintfToString(__VA_ARGS__))
#define OVR_LOG_WITH_TAG(__tag__, ...) OVR_LOG(__VA_ARGS__)
#define OVR_WARN_WITH_TAG(__tag__, ...) OVR_WARN(__VA_ARGS__)
#define OVR_ASSERT_WITH_TAG(__expr__, __tag__)           \
    {                                                    \
        if (!(__expr__)) {                               \
            OVR_FAIL("ASSERTION FAILED: %s", #__expr__); \
        }                                                \
    }

#else
#error "unknown OVR_OS"
#endif

// logs only the first time to avoid spam
#define OVR_LOG_ONCE(...)                  \
    {                                      \
        static bool alreadyLogged = false; \
        if (!alreadyLogged) {              \
            OVR_LOG(__VA_ARGS__);          \
            alreadyLogged = true;          \
        }                                  \
    }

#define OVR_WARN_ONCE(...)                 \
    {                                      \
        static bool alreadyWarned = false; \
        if (!alreadyWarned) {              \
            OVR_WARN(__VA_ARGS__);         \
            alreadyWarned = true;          \
        }                                  \
    }

#define OVR_LOG_IF(__condition__, ...) \
    {                                  \
        if (__condition__) {           \
            OVR_LOG(__VA_ARGS__);      \
        }                              \
    }

#define OVR_WARN_IF(__condition__, ...) \
    {                                   \
        if (__condition__) {            \
            OVR_WARN(__VA_ARGS__);      \
        }                               \
    }

#define OVR_LOG_EVERY_N_SEC(n, ...)                                                       \
    OVR_LOG_IF(                                                                           \
        (n) == 0 ? true : OVR_shouldLogNow(static_cast<long long>(1.0e9 * (n)), []() {}), \
        __VA_ARGS__)
#define OVR_WARN_EVERY_N_SEC(n, ...)                                                      \
    OVR_WARN_IF(                                                                          \
        (n) == 0 ? true : OVR_shouldLogNow(static_cast<long long>(1.0e9 * (n)), []() {}), \
        __VA_ARGS__)

#endif // OVRLib_Log_h
