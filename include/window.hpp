#pragma once
#include "input_state.hpp"

struct GLFWwindow;

class Window {
public:
  void Init(int width, int height, const char *title, InputState &inputState);
  void Shutdown();

  bool ShouldClose() const;
  void SwapBuffers();
  void PollEvents();

  GLFWwindow *GetHandle() const { return handle; }

private:
  GLFWwindow *handle = nullptr;
  int width = 0;
  int height = 0;
  InputState *inputState = nullptr;

  void InitOpenGL();
  void SetViewport(int w, int h);
};
