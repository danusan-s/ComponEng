#pragma once

struct Input {
  bool forward, backward, left, right;
  bool jump, crouch;
};

struct MouseInput {
  float deltaX, deltaY;
  bool leftButton, rightButton;
};
