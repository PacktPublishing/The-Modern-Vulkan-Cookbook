#!/usr/bin/env python3
# (c) Meta Platforms, Inc. and affiliates. Confidential and proprietary.

# This script is used for generate XrSample from the beginning by copying over the XrAppBase and
# modify all necessary field based on the extension name

import errno
import os
import shutil
import sys

def remove(path):
    """ param <path> could either be relative or absolute. """
    if os.path.isfile(path) or os.path.islink(path):
        os.remove(path)  # remove the file
    elif os.path.isdir(path):
        shutil.rmtree(path)  # remove dir and all contains
    else:
        raise ValueError("file {} is not a file or dir.".format(path))


def CopyDist(src, dst):
    try:
        shutil.copytree(src, dst)

        # delete any files generated after build XrAppBase
        dstAndroid = os.path.join(dst, "Projects/Android")
        filesInAndroid = ["AndroidManifest.xml", "buck_build.bat", "buck_build.py",
            "build.bat", "build.gradle", "build.py", "keystore.properties", "settings.gradle", "jni"]

        for f in os.listdir(dstAndroid):
            if f not in filesInAndroid :
                remove(os.path.join(dstAndroid, f))
    except OSError as exc:  # python >2.5
        if exc.errno in (errno.ENOTDIR, errno.EINVAL):
            shutil.copy(src, dst)
        else:
            raise


def BuildXrSample(appName, isPublicApp = True, readyForPublicRelease = True):
    folderName = "Xr" + appName
    appLocation = "XrSamples"
    packageLocation = "sdk"
    
    targetRootPath = f"//arvr/projects/xrruntime/mobile/{appLocation}/{folderName}"
    appClass = folderName + "App"
    appTitle = folderName + " Sample"
    packageName = f"com.oculus.{packageLocation}.{folderName.lower()}"
    targetName = f"{appLocation.lower()}_{folderName.lower()}"

    # use full path here to deal with command like `XrSamples/generate_xr_sample_template.py`
    dirname = os.path.split(os.path.dirname(os.path.realpath(__file__)))[0] + f"/{appLocation}"
    src = os.path.join(dirname, "XrAppBase")
    dst = os.path.join(dirname, folderName)
    CopyDist(src, dst)

    excludeDir = ["assets"]

    for dp, dn, filenames in os.walk(dst):
        dn[:] = [d for d in dn if d not in excludeDir]
        for f in filenames:
            if f == ".DS_Store":
                continue
            fileName = os.path.join(dp, f)
            print(fileName)

            # Read in the file
            with open(fileName, "r") as file:
                fileData = file.read()

            # Replace
            fileData = fileData.replace(
                f"//arvr/projects/xrruntime/mobile/{appLocation}/XrAppBase", targetRootPath
            )
            fileData = fileData.replace(
                f"{appLocation}:XrAppBase", f"{appLocation}:" + folderName
            )
            fileData = fileData.replace("XrAppBaseApp", appClass)
            fileData = fileData.replace("XrAppBase", folderName)

            fileData = fileData.replace("com.oculus.sdk.xrappbase", packageName)
            fileData = fileData.replace(f"{appLocation.lower()}_xrappbase", targetName)
            fileData = fileData.replace("xrappbase", folderName.lower())

            fileData = fileData.replace("Xr App Base", appTitle)

            # Write the file out again
            with open(fileName, "wb") as file:
                file.write(bytes(fileData, "UTF-8"))


def main():
    appName = input(
"""
What is your app's name?
    Input Example:
        NameOfYourApp
    Expectation:
        App Folder: Xr[NameOfYourApp]
        Target: xrsamples_xr[nameofyourapp]
        Package: com.oculus.sdk.xr[nameofyourapp]
        App Class Name: Xr[NameOfYourApp]App\n
"""
    )
    isPublicApp = True
    readyForPublicRelease = True
    
    BuildXrSample(appName, isPublicApp, readyForPublicRelease)


if __name__ == "__main__":
    main()
