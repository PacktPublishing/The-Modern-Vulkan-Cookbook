#ifndef OPENXR_SAMPLE_COMMON_H
#define OPENXR_SAMPLE_COMMON_H

#include <android/log.h>

#define TAG "OPENXR_SAMPLE"

#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR,    TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN,     TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO,     TAG, __VA_ARGS__)
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG,    TAG, __VA_ARGS__)

#define XR_CHECK(statement) \
    {                                \
        const XrResult result = statement; \
        if (result != XR_SUCCESS) {  \
            LOGE("result != XR_SUCCESS at %s:%d with result == %d", __FILE__, __LINE__, result); \
        }                                 \
    }

#endif //OPENXR_SAMPLE_COMMON_H
