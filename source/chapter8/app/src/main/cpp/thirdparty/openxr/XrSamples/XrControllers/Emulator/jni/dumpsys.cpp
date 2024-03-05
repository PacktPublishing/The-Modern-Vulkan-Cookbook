#include "Emulator/Src/EventLogger.h"
#include <jni.h>

extern "C" {
JNIEXPORT jstring JNICALL Java_com_oculus_sdk_xrcontrollers_emulator_MainActivity_nativeGetDumpStr(
    JNIEnv* jni,
    jobject) {

  return jni->NewStringUTF(XrControllers::Emulator::EventLogger::DumpEvents().c_str());
}
}
