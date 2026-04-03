#pragma once

#include "glad/glad.h"
#include "input_state.hpp"
#include <GLFW/glfw3.h>

/**
 * @brief Abstraction over GLFW window management and OpenGL context setup.
 *
 * Handles window creation, OpenGL initialization, viewport configuration,
 * and exposes input state. Also provides convenience methods for the
 * main loop in engine (shouldClose, swapBuffers, pollEvents).
 */
class Window {
public:
  // Initialize GLFW, create window and OpenGL context, set up input callbacks
  void init(int width, int height, const char *title);
  // Clean up GLFW resources and destroy window
  void shutdown();

  // Convenience methods that wrap GLFW calls for the main loop
  bool shouldClose() const;
  void swapBuffers();
  void pollEvents();

  GLFWwindow *getHandle() const {
    return m_handle;
  }

  InputState m_inputState;

private:
  GLFWwindow *m_handle = nullptr;
  int m_width = 0;
  int m_height = 0;

  void initOpenGL();
  void setViewport(int w, int h);
};
