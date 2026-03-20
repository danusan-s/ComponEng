#pragma once
#include "system.hpp"
#include <GLFW/glfw3.h>

struct InputState {
  bool keys[1024] = {false};
  float mouseX = 0.0f;
  float mouseY = 0.0f;
  float lastMouseX = 0.0f;
  float lastMouseY = 0.0f;
  float mouseDeltaX = 0.0f;
  float mouseDeltaY = 0.0f;
  bool firstMouse = true;
  bool mouseButtons[8] = {false};
};

class InputSystem : public System {
public:
  void Init(World &world) override;
  void Update(float deltaTime) override;

  InputState &GetState() { return state; }

  void KeyCallback(int key, int scancode, int action, int mode);
  void MouseButtonCallback(int button, int action, int mods);
  void MouseCallback(double xposIn, double yposIn);

private:
  InputState state;
};
