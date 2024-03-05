// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

package com.oculus.sdk.xrcontrollers.emulator;

import java.io.PrintWriter;
import java.io.FileDescriptor;

public class MainActivity extends android.app.NativeActivity {
  private static native String nativeGetDumpStr();

  static {
    System.loadLibrary("openxr_loader");
    System.loadLibrary("xrcontrollers_emulator");
  }

  @Override
  public void dump(String prefix, FileDescriptor fd, PrintWriter writer, String[] args) {
    writer.println();
    writer.println("========== CONTROLLER EMULATOR ==========");
    writer.println("Emulator Events Begin");
    writer.println(nativeGetDumpStr());
    writer.println("Emulator Events End");
  }
}
