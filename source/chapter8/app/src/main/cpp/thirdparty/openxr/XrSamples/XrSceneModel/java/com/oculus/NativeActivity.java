// Copyright (c) Facebook Technologies, LLC and its affiliates. All Rights reserved.
package com.oculus;

import android.content.pm.PackageManager;
import android.os.Bundle;
import android.util.Log;

/**
 * When using NativeActivity, we currently need to handle loading of dependent shared libraries
 * manually before a shared library that depends on them is loaded, since there is not currently a
 * way to specify a shared library dependency for NativeActivity via the manifest meta-data.
 *
 * <p>The simplest method for doing so is to subclass NativeActivity with an empty activity that
 * calls System.loadLibrary on the dependent libraries, which is unfortunate when the goal is to
 * write a pure native C/C++ only Android activity.
 *
 * <p>A native-code only solution is to load the dependent libraries dynamically using dlopen().
 * However, there are a few considerations, see:
 * https://groups.google.com/forum/#!msg/android-ndk/l2E2qh17Q6I/wj6s_6HSjaYJ
 *
 * <p>1. Only call dlopen() if you're sure it will succeed as the bionic dynamic linker will
 * remember if dlopen failed and will not re-try a dlopen on the same lib a second time.
 *
 * <p>2. Must remember what libraries have already been loaded to avoid infinitely looping when
 * libraries have circular dependencies.
 */
public class NativeActivity extends android.app.NativeActivity {
  private static final String PERMISSION_USE_SCENE = "com.oculus.permission.USE_SCENE";
  private static final int REQUEST_CODE_PERMISSION_USE_SCENE = 1;
  private static final String TAG = "XrSceneModel";

  static {
    System.loadLibrary("openxr_loader");
    System.loadLibrary("scenemodel");
  }

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);
    requestScenePermissionIfNeeded();
  }

  private void requestScenePermissionIfNeeded() {
    Log.d(TAG, "requestScenePermissionIfNeeded");
    if (checkSelfPermission(PERMISSION_USE_SCENE) != PackageManager.PERMISSION_GRANTED) {
      Log.d(TAG, "Permission has not been granted, request " + PERMISSION_USE_SCENE);
      requestPermissions(
          new String[] {PERMISSION_USE_SCENE}, REQUEST_CODE_PERMISSION_USE_SCENE);
    }
  }
}
