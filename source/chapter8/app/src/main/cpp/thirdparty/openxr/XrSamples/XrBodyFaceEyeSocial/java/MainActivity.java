// Copyright (c) Facebook Technologies, LLC and its affiliates. All Rights reserved.
package com.oculus.xrsamples.xrbodyfaceeyesocial;

import android.content.pm.PackageManager;
import android.os.Bundle;
import java.util.ArrayList;
import java.util.List;

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
public class MainActivity extends android.app.NativeActivity {
  private static final String PERMISSION_EYE_TRACKING = "com.oculus.permission.EYE_TRACKING";
  private static final String PERMISSION_FACE_TRACKING = "com.oculus.permission.FACE_TRACKING";
  private static final String PERMISSION_RECORD_AUDIO = "android.permission.RECORD_AUDIO";
  private static final int REQUEST_CODE_PERMISSION_EYE_AND_FACE_TRACKING = 1;

  static {
    System.loadLibrary("openxr_loader");
    System.loadLibrary("xrbodyfaceeyesocial");
  }

  @Override
  protected void onCreate(Bundle savedInstanceState) {
    super.onCreate(savedInstanceState);

    // This sample makes use of eye and face tracking data at all times
    // so we are requesting it in onCreate. If an app, only requires eye
    // and face tracking data deep in their experience it may make sense
    // to query this later in the app lifecycle.
    requestFaceEyeTrackingPermissionsIfNeeded();
  }

  private void requestFaceEyeTrackingPermissionsIfNeeded() {
    List<String> permissionsToRequest = new ArrayList<>();
    if (checkSelfPermission(PERMISSION_EYE_TRACKING) != PackageManager.PERMISSION_GRANTED) {
      permissionsToRequest.add(PERMISSION_EYE_TRACKING);
    }
    if (checkSelfPermission(PERMISSION_FACE_TRACKING) != PackageManager.PERMISSION_GRANTED) {
      permissionsToRequest.add(PERMISSION_FACE_TRACKING);
    }
    if (checkSelfPermission(PERMISSION_RECORD_AUDIO) != PackageManager.PERMISSION_GRANTED) {
      permissionsToRequest.add(PERMISSION_RECORD_AUDIO);
    }

    if (!permissionsToRequest.isEmpty()) {
      String[] permissionsAsArray =
          permissionsToRequest.toArray(new String[permissionsToRequest.size()]);
      requestPermissions(permissionsAsArray, REQUEST_CODE_PERMISSION_EYE_AND_FACE_TRACKING);
    }
  }
}
