#pragma once

struct InputState {
  bool keys[1024] = {false};
  float mouseX = 0.0f;
  float mouseY = 0.0f;
  float lastMouseX = 0.0f;
  float lastMouseY = 0.0f;
  bool firstMouse = true;
  bool mouseButtons[8] = {false};
};
