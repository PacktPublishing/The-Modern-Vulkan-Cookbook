// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#pragma once

///////////////////////////////////////////////////////////////////////////////////////////////////
// Notes on OVR_NO_SANITIZE_ADDRESS usage in this file:
//
// The reason why we need OVR_NO_SANITIZE_ADDRESS on some functions in this file is that, Android
// sometimes passes us JavaVm* with extremely huge address value, which is something like
// 0xb400007a923d7380. This address still works fine as JavaVM pointer such that we can invoke its
// method, but when ASAN instrumentation embedds the machine code to do shadow map calculation,
// because the address is in unexpectedly too big region for ASAN, the calculated address is
// pointing to non-shadowed memory, and results in SEGV.
// https://github.com/google/sanitizers/wiki/AddressSanitizerAlgorithm#mapping
//
// And the reason why we need to call JavaVM's functions via vm->functions->Xxx() instead of
// vm->Xxx(), is that the body of vm->Xxx() is written in jni.h, and we cannot add
// OVR_NO_SANITIZE_ADDRESS to jni.h. If we don't add it, the same ASAN problem happens at jni.h's
// implementation.
///////////////////////////////////////////////////////////////////////////////////////////////////

#if defined(OVR_OS_ANDROID)

#include "OVR_LogUtils.h"
#include "OVR_Std.h"
#include "OVR_SanitizerMacros.h"

#include <pthread.h>
#include <unistd.h>

// Wrapper function for JavaVM->Xxx() to suppress ASAN. See the notes at the beginning of this file.
OVR_NO_SANITIZE_ADDRESS
static inline jint attachCurrentThread(JavaVM* vm, JNIEnv** pEnv, void* thrArgs) {
#if defined(OVR_USING_ADDRESS_SANITIZER)
    return vm->functions->AttachCurrentThread(vm, pEnv, thrArgs);
#else
    return vm->AttachCurrentThread(pEnv, thrArgs);
#endif
}

// Wrapper function for JavaVM->Xxx() to suppress ASAN. See the notes at the beginning of this file.
OVR_NO_SANITIZE_ADDRESS
static inline jint detachCurrentThread(JavaVM* vm) {
#if defined(OVR_USING_ADDRESS_SANITIZER)
    return vm->functions->DetachCurrentThread(vm);
#else
    return vm->DetachCurrentThread();
#endif
}

// Wrapper function for JavaVM->Xxx() to suppress ASAN. See the notes at the beginning of this file.
OVR_NO_SANITIZE_ADDRESS
static inline jint getEnv(JavaVM* vm, void** env, jint version) {
#if defined(OVR_USING_ADDRESS_SANITIZER)
    return vm->functions->GetEnv(vm, env, version);
#else
    return vm->GetEnv(env, version);
#endif
}

inline jint ovr_AttachCurrentThread(JavaVM* vm, JNIEnv** jni, void* args) {
    // First read current thread name.
    char threadName[16] = {0}; // max thread name length is 15.
    char commpath[64] = {0};
    OVR::OVR_sprintf(commpath, sizeof(commpath), "/proc/%d/task/%d/comm", getpid(), gettid());
    FILE* f = fopen(commpath, "r");
    if (f != nullptr) {
        fread(threadName, 1, sizeof(threadName) - 1, f);
        fclose(f);
        // Remove any trailing new-line characters.
        for (int len = strlen(threadName) - 1;
             len >= 0 && (threadName[len] == '\n' || threadName[len] == '\r');
             len--) {
            threadName[len] = '\0';
        }
    }

    // Attach the thread to the JVM.
    const jint rtn = attachCurrentThread(vm, jni, args);
    if (rtn != JNI_OK) {
        OVR_FAIL("AttachCurrentThread returned %i", rtn);
    }
    // Restore the thread name after AttachCurrentThread() overwrites it.
    if (threadName[0] != '\0') {
        pthread_setname_np(pthread_self(), threadName);
    }
    return rtn;
}

inline jint ovr_DetachCurrentThread(JavaVM* vm) {
    const jint rtn = detachCurrentThread(vm);
    if (rtn != JNI_OK) {
        OVR_FAIL("DetachCurrentThread() returned %i", rtn);
    }
    return rtn;
}

// This function needs to suppress ASAN because there is a pointer copy of JavaVM, for which ASAN
// will instrument the memory check. See the notes at the beginning of this file.
OVR_NO_SANITIZE_ADDRESS
inline TempJniEnv::TempJniEnv(JavaVM* vm, const char*, int)
    : vm_(vm), jni_(nullptr), privateEnv_(false) {
    if (!vm_) {
        OVR_WARN("Null JavaVM is passed");
        return;
    }

    if (JNI_OK != getEnv(vm_, reinterpret_cast<void**>(&jni_), JNI_VERSION_1_6)) {
        OVR_LOG(
            "Creating temporary JNIEnv. This is a heavy operation and should be infrequent. To optimize, use JNI AttachCurrentThread on calling thread");
        // OVR_LOG( "Temporary JNIEnv created at %s:%d", file, line );
        ovr_AttachCurrentThread(vm_, &jni_, nullptr);
        privateEnv_ = true;
    } else {
        // Current thread is attached to the VM.
        // OVR_LOG( "Using caller's JNIEnv" );
    }
}

inline TempJniEnv::~TempJniEnv() {
    if (privateEnv_) {
        ovr_DetachCurrentThread(vm_);
    }
}

#endif // defined(OVR_OS_ANDROID)
