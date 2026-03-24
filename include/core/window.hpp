#pragma once
#include "input_state.hpp"
#include <GLFW/glfw3.h>

class Window {
public:
  void Init(int width, int height, const char *title);
  void Shutdown();

  bool ShouldClose() const;
  void SwapBuffers();
  void PollEvents();

  GLFWwindow *GetHandle() const {
    return handle;
  }

  InputState inputState;

private:
  GLFWwindow *handle = nullptr;
  int width = 0;
  int height = 0;

  void InitOpenGL();
  void SetViewport(int w, int h);
};
