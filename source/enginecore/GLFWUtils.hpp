#pragma once

#if defined(_WIN32)

#define GLFW_EXPOSE_NATIVE_WIN32
#define GLFW_EXPOSE_NATIVE_WGL
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "enginecore/Camera.hpp"

extern bool initWindow(GLFWwindow** outWindow, EngineCore::Camera* cameraPtr,
                       int width = 1600, int height = 1200);

#endif
