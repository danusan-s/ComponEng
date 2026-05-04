#include "componeng/core/window.hpp"
#include "componeng/core/input_state.hpp"
#include "componeng/core/logger.hpp"
#include "glad/glad.h"
#include <GLFW/glfw3.h>

static bool g_mouseLocked = true;

static void framebufferSizeCallback(GLFWwindow *window, int width, int height) {
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

static void keyCallback(GLFWwindow *window, int key, int scancode, int action,
                        int mode) {
  auto *inputState =
      static_cast<InputState *>(glfwGetWindowUserPointer(window));
  if (!inputState)
    return;

  if (key == GLFW_KEY_F11 && action == GLFW_PRESS) {
    if (glfwGetInputMode(window, GLFW_CURSOR) == GLFW_CURSOR_DISABLED) {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
      g_mouseLocked = false;
    } else {
      glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
      g_mouseLocked = true;
    }
  }

  if (!g_mouseLocked)
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

static void mouseButtonCallback(GLFWwindow *window, int button, int action,
                                int mods) {
  auto *inputState =
      static_cast<InputState *>(glfwGetWindowUserPointer(window));
  if (!inputState)
    return;

  if (!g_mouseLocked)
    return;

  if (button >= 0 && button < 8) {
    inputState->mouseButtons[button] = (action == GLFW_PRESS);
  }
}

static void cursorPosCallback(GLFWwindow *window, double xposIn,
                              double yposIn) {
  auto *inputState =
      static_cast<InputState *>(glfwGetWindowUserPointer(window));
  if (!inputState)
    return;

  if (!g_mouseLocked) {
    inputState->firstMouse = true;
    return;
  }

  float xpos = static_cast<float>(xposIn);
  float ypos = static_cast<float>(yposIn);

  if (inputState->firstMouse) {
    inputState->lastMouseX = xpos;
    inputState->lastMouseY = ypos;
    inputState->firstMouse = false;
  }

  inputState->lastMouseX = inputState->mouseX;
  inputState->lastMouseY = inputState->mouseY;
  inputState->mouseX = xpos;
  inputState->mouseY = ypos;
}

void Window::init(int width, int height, const char *title,
                  IRenderDevice *renderDevice) {
  this->m_width = width;
  this->m_height = height;

  if (glfwInit() != GLFW_TRUE) {
    LOG_ERROR("Failed to initialize GLFW");
    return;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

  m_handle = glfwCreateWindow(width, height, title, nullptr, nullptr);
  if (!m_handle) {
    LOG_ERROR("Failed to create GLFW window");
    glfwTerminate();
    return;
  }

  glfwMakeContextCurrent(m_handle);

  m_renderDevice = renderDevice;
  m_renderDevice->init(m_handle);

  glfwSetWindowUserPointer(m_handle, &m_inputState);
  glfwSetKeyCallback(m_handle, keyCallback);
  glfwSetMouseButtonCallback(m_handle, mouseButtonCallback);
  glfwSetCursorPosCallback(m_handle, cursorPosCallback);
  glfwSetFramebufferSizeCallback(m_handle, framebufferSizeCallback);
  glfwSetInputMode(m_handle, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

  setViewport(width, height);
}

void Window::setViewport(int w, int h) {
  float targetAspectRatio = 16.0f / 9.0f;
  float currentAspectRatio = static_cast<float>(w) / h;

  if (currentAspectRatio > targetAspectRatio) {
    int newWidth = static_cast<int>(h * targetAspectRatio);
    int xOffset = (w - newWidth) / 2;
    m_renderDevice->setScissor(xOffset, 0, newWidth, h);
    m_renderDevice->setViewport(xOffset, 0, newWidth, h);
    return;
  }

  int newHeight = static_cast<int>(w / targetAspectRatio);
  int yOffset = (h - newHeight) / 2;
  m_renderDevice->setScissor(0, yOffset, w, newHeight);
  m_renderDevice->setViewport(0, yOffset, w, newHeight);
}

void Window::shutdown() {
  if (m_handle) {
    glfwDestroyWindow(m_handle);
    m_handle = nullptr;
  }
  glfwTerminate();
}

bool Window::shouldClose() const {
  return m_handle ? glfwWindowShouldClose(m_handle) : true;
}

void Window::swapBuffers() {
  if (m_handle)
    glfwSwapBuffers(m_handle);
}

void Window::pollEvents() {
  glfwPollEvents();
}
