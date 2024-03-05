// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#include "JniUtils.h"

#include <gtest/gtest.h>
// Needed for getting android JNI parameters like context, VM, JNIEnv etc
#include <oxx_android_jni.h>
// Helper libraries to access the JNI information for the session. This hides most of the JNI
// ugliness
#include <system_utils/jni/Helper.h>

void OxxAndroidOnLoad(JavaVM* vm, JNIEnv* env) {
    arvr::system_utils::jni::Helper::androidMain(vm, env);
}

namespace OVR {
namespace {

TEST(JniUtils, NullVmShallReturnNullEnv) {
    // Arrange
    TempJniEnv jniEnv(nullptr);

    // Assert
    EXPECT_EQ(jniEnv.operator->(), nullptr);
}

TEST(JniUtils, Macro_NullVmShallReturnNullEnv) {
    // Arrange
    JavaVM* jvm = nullptr;
    JNI_TMP_ENV(jniEnv, jvm);

    // Assert
    EXPECT_EQ(jniEnv.operator->(), nullptr);
}

TEST(JniUtils, VmShallReturnNonNullEnv) {
    // Arrange
    auto vm = arvr::system_utils::jni::Helper::getVm();
    TempJniEnv jniEnv(vm);

    // Assert
    EXPECT_NE(jniEnv.operator->(), nullptr);
}

} // namespace
} // namespace OVR
