#include "GLFWUtils.hpp"

#if defined(_WIN32)

#include <gli/gli.hpp>
#include <glm/glm.hpp>

#include "imgui.h"

int width_ = 0;
int height_ = 0;
glm::vec2 mousePos_ = glm::vec2(0.0f);
bool mousePressed_ = false;

static EngineCore::Camera* cameraPtr = nullptr;

bool initWindow(GLFWwindow** outWindow,
                EngineCore::Camera* inCameraPtr,
                int width,
                int height) {
  if (!glfwInit())
    return false;

  cameraPtr = inCameraPtr;

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
  const char* title = "Modern Vulkan Cookbook";

  // render full screen without overlapping taskbar
  GLFWmonitor* monitor = glfwGetPrimaryMonitor();
  const GLFWvidmode* mode = glfwGetVideoMode(monitor);

  uint32_t posX = 0;
  uint32_t posY = 0;

#ifdef _WIN32
  {
    // get usuable screen dimension for windows
    RECT rect;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &rect, 0);

    posX = 200;     // rect.left;
    posY = 200;     // rect.top;
    width = 1600;   // rect.right - rect.left;
    height = 1200;  // rect.bottom - rect.top;
  }
#endif

  GLFWwindow* window = glfwCreateWindow(width, height, title, nullptr, nullptr);

  if (!window) {
    glfwTerminate();
    return false;
  }

#if defined(WIN32)
  HWND hwnd = glfwGetWin32Window(window);
  SetWindowLongPtr(
      hwnd, GWL_STYLE,
      GetWindowLongPtrA(hwnd, GWL_STYLE) & ~(WS_MAXIMIZEBOX | WS_MINIMIZEBOX));
#endif

  glfwSetWindowPos(window, posX, posY);

  glfwSetErrorCallback([](int error, const char* description) {
    printf("GLFW Error (%i): %s\n", error, description);
  });

  glfwSetCursorPosCallback(window, [](auto* window, double x, double y) {
    if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) {
      return;
    }
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glm::vec2 newMousePos = glm::vec2(x / width, 1.0f - y / height);
    if (mousePressed_) {
      const auto delta = newMousePos - mousePos_;
      if (cameraPtr) {
        cameraPtr->rotate(delta);
      }
    }
    mousePos_ = newMousePos;
  });

  glfwSetMouseButtonCallback(
      window, [](auto* window, int button, int action, int mods) {
        if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureMouse) {
          return;
        }
        if (button == GLFW_MOUSE_BUTTON_LEFT)
          mousePressed_ = (action == GLFW_PRESS);
      });

  glfwSetKeyCallback(
      window, [](GLFWwindow* window, int key, int, int action, int mods) {
        if (ImGui::GetCurrentContext() && ImGui::GetIO().WantCaptureKeyboard) {
          return;
        }
        const bool pressed = action != GLFW_RELEASE;
        if (key == GLFW_KEY_ESCAPE && pressed) {
          glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        double deltaT = 1.f;
        if (mods & GLFW_MOD_SHIFT) {
          deltaT = 50.0f;
        }

        if (key == GLFW_KEY_ESCAPE && pressed)
          glfwSetWindowShouldClose(window, GLFW_TRUE);
        if (key == GLFW_KEY_W && pressed) {
          if (cameraPtr) {
            cameraPtr->move(-cameraPtr->direction(), deltaT);
          }
        }
        if (key == GLFW_KEY_S && pressed) {
          if (cameraPtr) {
            cameraPtr->move(cameraPtr->direction(), deltaT);
          }
        }
        if (key == GLFW_KEY_A && pressed) {
          if (cameraPtr) {
            cameraPtr->move(-cameraPtr->right(), deltaT);
          }
        }
        if (key == GLFW_KEY_D && pressed) {
          if (cameraPtr) {
            cameraPtr->move(cameraPtr->right(), deltaT);
          }
        }
        if (key == GLFW_KEY_Q && pressed) {
          if (cameraPtr) {
            cameraPtr->move(glm::vec3(0.0f, 1.0f, 0.0f), deltaT);
          }
        }
        if (key == GLFW_KEY_E && pressed) {
          if (cameraPtr) {
            cameraPtr->move(glm::vec3(0.0f, -1.0f, 0.0f), deltaT);
          }
        }
      });

  glfwGetWindowSize(window, &width_, &height_);

  if (outWindow) {
    *outWindow = window;
  }

  return true;
}

#endif
