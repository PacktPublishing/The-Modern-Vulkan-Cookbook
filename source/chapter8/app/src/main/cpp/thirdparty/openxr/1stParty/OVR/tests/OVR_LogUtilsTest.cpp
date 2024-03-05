// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

#include <OVR_LogUtils.h>

#include <gtest/gtest.h>

#include <arvr/libraries/thread_safety_analysis/Mutex.h>

namespace OVR {
namespace {

TEST(LogUtilsTest, FatalTest) {
    EXPECT_DEATH(OVR_FAIL("death message"), "death message");
}

TEST(LogUtilsTest, CompatibleWithTSA) {
    struct Foo {
        facebook::xr::tsa::mutex mutex_;
        int i_ XR_TSA_GUARDED_BY(mutex_) = 0;

        void a() {
            facebook::xr::tsa::lock_guard lock(mutex_);
            OVR_LOG("%i", i_);
        }
    };

    Foo foo;
    foo.a();
}

#if (defined(OVR_OS_MAC) || defined(OVR_OS_LINUX) || defined(OVR_OS_IPHONE))
TEST(LogUtilsTest, ovrLogConvertPrintfToString) {
    std::string result = ovrLogConvertPrintfToString("foo");
    EXPECT_EQ(result, "foo");

    result = ovrLogConvertPrintfToString("%s", "foo");
    EXPECT_EQ(result, "foo");

    result = ovrLogConvertPrintfToString("%s %i", "foo", 1);
    EXPECT_EQ(result, "foo 1");
}
#endif

} // namespace
} // namespace OVR
