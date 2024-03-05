// (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

package com.oculus.xrpassthroughocclusion;

import android.os.Bundle;
import android.util.Log;

public class MainNativeActivity extends android.app.NativeActivity {

  @Override
  public void onCreate(Bundle savedInstanceState) {
    Log.d(
        MainActivity.TAG,
        String.format("MainNativeActivity.onCreate() called"));
    super.onCreate(savedInstanceState);
  }

  // Called from native code to safely quit app.
  public void onNativeFinish() {
    Log.d(
        MainActivity.TAG,
        String.format("MainNativeActivity finish called from native app."));
    finishAndRemoveTask();
  }

  @Override
  public void onDestroy() {
    Log.d(
        MainActivity.TAG,
        String.format("MainNativeActivity.onDestroy() called"));
    super.onDestroy();
  }
}
