#include "core/window.hpp"
#include "glad/glad.h"
#include <GLFW/glfw3.h>
#include <iostream>

static bool mouseLocked = true;

static void FramebufferSizeCallback(GLFWwindow *window, int width, int height) {
  float targetAspectRatio = 16.0f / 9.0f;
  float currentAspectRatio = static_cast<float>(width) / height;

  if (currentAspectRatio > targetAspectRatio) {
    int newWidth = static_cast<int>(height * targetAspectRatio);
    int xOffset = (width - newWidth) / 2;
    glScissor(xOffset, 0, newWidth, height);
    glViewport(xOffset, 0, newWidth, height);
    return;
  }

  int newHeight = static_cast<int>(width / targetAspectRatio);
  int yOffset = (height - newHeight) / 2;
  glScissor(0, yOffset, width, newHeight);
  glViewport(0, yOffset, width, newHeight);
}

static void KeyCallback(GLFWwindow *window, int key, int scancode, int action,
                        int mode) {
  auto *inputState =
      static_cast<InputState *>(glfwGetWindowUserPointer(window));
  if (!inputState)
    return;

  if (key == GLFW_KEY_F11 && action == GLFW_PRESS) {
    if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      mouseLocked = false;
    } else {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      mouseLocked = true;
    }
  }

  if (!mouseLocked)
    return;

  if (key >= 0 && key < 1024) {
    if (action == GLFW_PRESS)
      inputState->keys[key] = true;
    else if (action == GLFW_RELEASE)
      inputState->keys[key] = false;
  }

  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, true);
}

static void MouseButtonCallback(GLFWwindow *window, int button, int action,
                                int mods) {
  auto *inputState =
      static_cast<InputState *>(glfwGetWindowUserPointer(window));
  if (!inputState)
    return;

  if (!mouseLocked)
    return;

  if (button >= 0 && button < 8) {
    inputState->mouseButtons[button] = (action == GLFW_PRESS);
  }
}

static void CursorPosCallback(GLFWwindow *window, double xposIn,
                              double yposIn) {
  auto *inputState =
      static_cast<InputState *>(glfwGetWindowUserPointer(window));
  if (!inputState)
    return;

  if (!mouseLocked) {
    inputState->firstMouse = true; // reset mouse state when unlocking
    return;
  }

  float xpos = static_cast<float>(xposIn);
  float ypos = static_cast<float>(yposIn);

  if (inputState->firstMouse) {
    inputState->lastMouseX = xpos;
    inputState->lastMouseY = ypos;
    inputState->firstMouse = false;
  }

  inputState->mouseDeltaX = xpos - inputState->lastMouseX;
  inputState->mouseDeltaY = inputState->lastMouseY - ypos; // inverted Y
  inputState->lastMouseX = xpos;
  inputState->lastMouseY = ypos;
}

void Window::Init(int width, int height, const char *title,
                  InputState &inputState) {
  this->width = width;
  this->height = height;
  this->inputState = &inputState;

  glfwInit();
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  handle = glfwCreateWindow(width, height, title, nullptr, nullptr);
  if (!handle) {
    std::cerr << "Failed to create GLFW window" << std::endl;
    glfwTerminate();
    return;
  }

  glfwMakeContextCurrent(handle);

  if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
    std::cerr << "Failed to initialize GLAD" << std::endl;
    return;
  }

  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);
  glEnable(GL_SCISSOR_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glfwSetWindowUserPointer(handle, &inputState);
  glfwSetKeyCallback(handle, KeyCallback);
  glfwSetMouseButtonCallback(handle, MouseButtonCallback);
  glfwSetCursorPosCallback(handle, CursorPosCallback);
  glfwSetFramebufferSizeCallback(handle, FramebufferSizeCallback);
  glfwSetInputMode(handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  SetViewport(width, height);
}

void Window::SetViewport(int w, int h) {
  float targetAspectRatio = 16.0f / 9.0f;
  float currentAspectRatio = static_cast<float>(w) / h;

  if (currentAspectRatio > targetAspectRatio) {
    int newWidth = static_cast<int>(h * targetAspectRatio);
    int xOffset = (w - newWidth) / 2;
    glScissor(xOffset, 0, newWidth, h);
    glViewport(xOffset, 0, newWidth, h);
    return;
  }

  int newHeight = static_cast<int>(w / targetAspectRatio);
  int yOffset = (h - newHeight) / 2;
  glScissor(0, yOffset, w, newHeight);
  glViewport(0, yOffset, w, newHeight);
}

void Window::Shutdown() {
  if (handle) {
    glfwDestroyWindow(handle);
    handle = nullptr;
  }
  glfwTerminate();
}

bool Window::ShouldClose() const {
  return handle ? glfwWindowShouldClose(handle) : true;
}

void Window::SwapBuffers() {
  if (handle)
    glfwSwapBuffers(handle);
}

void Window::PollEvents() {
  glfwPollEvents();
}
